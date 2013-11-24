// Copyright 2013  Chris Pearce
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stdafx.h"
#include "RotationTranscoder.h"
#include "TranscodeJobList.h"
#include "D2DManager.h"
#include "Utils.h"

using std::wstring;

RotationTranscoder::RotationTranscoder(const TranscodeJob* aJob)
  : mJob(aJob),
    mDecoderAudioStreamIndex(Unknown),
    mDecoderVideoStreamIndex(Unknown),
    mEncoderAudioStreamIndex(Unknown),
    mEncoderVideoStreamIndex(Unknown),
    mReaderOutputVideoStride(0),
    mWriterInputVideoStride(0),
    mH264Profile(Unknown),
    mProgress(0),
    mDuration(0),
    mLastVideoTimestamp(0),
    mInitialized(false),
    mAudioEOS(false),
    mVideoEOS(false)
{
}

RotationTranscoder::~RotationTranscoder()
{
}

HRESULT
RotationTranscoder::DetermineReaderOutputVideoType(IMFMediaType** aOutVideoType)
{
  HRESULT hr;

  // Read the first video frame, so that we can detect a format change.
  // Some videos have their frame size and/or aperature/pan/scan change
  // on the first frame.

  IMFSamplePtr sample;
  DWORD streamIndex, flags;
  LONGLONG timestamp;
  uint64_t videoSampleNum = 0;
  hr = mReader->ReadSample(mDecoderVideoStreamIndex,
                           0,                // Flags.
                           &streamIndex,     // Receives the actual stream index.
                           &flags,           // Receives status flags.
                           &timestamp,       // Receives the time stamp.
                           &sample);         // Receives the sample or NULL.
  if (FAILED(hr)) {
    DBGMSG(L"DetermineReaderOutputVideoType ReadSample failed with 0x%x\n", hr);
    return hr;
  }
  ENSURE_TRUE(streamIndex == mDecoderVideoStreamIndex, E_FAIL);

  if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
    DBGMSG(L"Detected type change on first video sample...\n");
  }

  IMFMediaTypePtr type;  
  hr = mReader->GetCurrentMediaType(mDecoderVideoStreamIndex, &type);
  ENSURE_SUCCESS(hr, hr);

  // Seek the reader back to the first frame.
  AutoPropVar var;
  hr = InitPropVariantFromInt64(0, &var);
  ENSURE_SUCCESS(hr, hr);

  hr = mReader->SetCurrentPosition(GUID_NULL, var);
  ENSURE_SUCCESS(hr, hr);

  *aOutVideoType = type.Detach();

  return S_OK;
}

HRESULT
RotationTranscoder::ConfigureReaderVideoOutput()
{
  HRESULT hr;

  // Get the native video output type of the reader, save the attributes that
  // we need for the re-encode.
  IMFMediaTypePtr nativeVideoType;
  hr = mReader->GetNativeMediaType(mDecoderVideoStreamIndex,
                                   0,
                                   &nativeVideoType);
  ENSURE_SUCCESS(hr, hr);
  DBGMSG(L"Reader native video type:\n");
  LogMediaType(nativeVideoType);
  DBGMSG(L"\n");

  // May not need these, encoder can calculate them or use default, but try
  // to retrieve if possible.
  nativeVideoType->GetUINT32(MF_MT_MPEG2_PROFILE, &mH264Profile);
  nativeVideoType->GetUINT32(MF_MT_AVG_BITRATE, &mAvgBitRate);

  hr = nativeVideoType->GetUINT32(MF_MT_INTERLACE_MODE, &mInterlaceMode);
  ENSURE_SUCCESS(hr, hr);

  // Configure video output for RGB.
  {
    IMFMediaTypePtr readerOutputVideoType;
    hr = MFCreateMediaType(&readerOutputVideoType);
    ENSURE_SUCCESS(hr, hr);
    hr = readerOutputVideoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    ENSURE_SUCCESS(hr, hr);
    hr = readerOutputVideoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    ENSURE_SUCCESS(hr, hr);
    hr = mReader->SetCurrentMediaType(mDecoderVideoStreamIndex,
                                      NULL,
                                      readerOutputVideoType);
    ENSURE_SUCCESS(hr, hr);
  }

  hr = DetermineReaderOutputVideoType(&mReaderOutputRGBVideoType);
  ENSURE_SUCCESS(hr, hr);

  hr = GetDefaultStride(mReaderOutputRGBVideoType, &mReaderOutputVideoStride);
  ENSURE_SUCCESS(hr, hr);

  hr = MFGetAttributeRatio(mReaderOutputRGBVideoType,
                           MF_MT_FRAME_RATE,
                           &mFrameRateNumer,
                           &mFrameRateDenom);
  ENSURE_SUCCESS(hr, hr);

  hr = MFGetAttributeRatio(mReaderOutputRGBVideoType,
                           MF_MT_PIXEL_ASPECT_RATIO,
                           &mPixelAspectRatioNumer,
                           &mPixelAspectRatioDenom);
  ENSURE_SUCCESS(hr, hr);

  hr = GetVideoDisplayArea(mReaderOutputRGBVideoType, &mReaderOuptutPictureRegion);
  ENSURE_SUCCESS(hr, hr);

  DBGMSG(L"Reader output video type:\n");
  LogMediaType(mReaderOutputRGBVideoType);
  DBGMSG(L"\n");

  return S_OK;
}

HRESULT
RotationTranscoder::ConfigureReaderAudioOutput()
{
  HRESULT hr;

  // Extract the audio native type, and determine how we're going to encode
  // the audio, either by pass through, or by resampling decoded samples.
  IMFMediaTypePtr nativeAudioType;
  hr = mReader->GetNativeMediaType(mDecoderAudioStreamIndex,
                                   0,
                                   &nativeAudioType);
  ENSURE_SUCCESS(hr, hr);

  DBGMSG(L"Reader native audio type:\n");
  LogMediaType(nativeAudioType);
  DBGMSG(L"\n");

  GUID subtype;
  hr = nativeAudioType->GetGUID(MF_MT_SUBTYPE, &subtype);
  ENSURE_SUCCESS(hr, hr);


  // The audio isn't AAC. We'll decode it to PCM, and resample it so that
  // it meets the requirements of the AAC encoder (at most 2 channels,
  // 44.1KHz or 48KHz).

  // Configure the audio to be output in PCM.
  IMFMediaTypePtr readerOutputAudioType;
  hr = MFCreateMediaType(&readerOutputAudioType);
  ENSURE_SUCCESS(hr, hr);
  hr = readerOutputAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  ENSURE_SUCCESS(hr, hr);
  hr = readerOutputAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
  ENSURE_SUCCESS(hr, hr);
  hr = mReader->SetCurrentMediaType(mDecoderAudioStreamIndex,
                                    NULL,
                                    readerOutputAudioType);
  ENSURE_SUCCESS(hr, hr);

  // Decode one sample, to force a media type change if we're decoding HE-AAC
  // and the sample rate doesn't take into account SBR/PS. It will once we
  // decode the first sample.
  IMFSamplePtr sample;
  DWORD actualStreamIndex, flags;
  LONGLONG timestamp;
  hr = mReader->ReadSample(mDecoderAudioStreamIndex, 0, &actualStreamIndex, &flags, &timestamp, &sample);
  ENSURE_SUCCESS(hr, hr);

  ENSURE_TRUE(actualStreamIndex == mDecoderAudioStreamIndex, E_FAIL);

  if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
    DBGMSG(L"Detected type change on first audio sample...\n");
  }

  // Seek the reader back to the first frame.
  AutoPropVar var;
  hr = InitPropVariantFromInt64(0, &var);
  ENSURE_SUCCESS(hr, hr);
  hr = mReader->SetCurrentPosition(GUID_NULL, var);
  ENSURE_SUCCESS(hr, hr);

  // Extract the completed audio type, as determined by the reader.
  hr = mReader->GetCurrentMediaType(mDecoderAudioStreamIndex, &readerOutputAudioType);
  ENSURE_SUCCESS(hr, hr);

  DBGMSG(L"Reader output audio type:\n");
  LogMediaType(readerOutputAudioType);
  DBGMSG(L"\n");

  // Pass the type to the resampler, it'll figure out the encode media type.
  hr = mAudioProcessor.SetInputType(readerOutputAudioType);
  ENSURE_SUCCESS(hr, hr);

  return S_OK;
}

HRESULT
RotationTranscoder::CreateReader()
{
  const wstring& inputFilename = mJob->GetInputFilename();
  const Rotation rotation = mJob->GetRotation();

  HRESULT hr;
  IMFAttributesPtr attributes;
  hr = MFCreateAttributes(&attributes, 1);
  ENSURE_SUCCESS(hr, hr);

  hr = attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

  // Create the source mReader from the URL.
  hr = MFCreateSourceReaderFromURL(inputFilename.c_str(), attributes, &mReader);
  ENSURE_SUCCESS(hr, hr);

  hr = GetSourceReaderDuration(mReader, &mDuration);

  // Figure out the index of the audio and video stream, so that
  // ReadSample(ANY_STREAM) can get the muxing order the same as the
  // muxed file.
  hr = GetReaderStreamIndexes(mReader,
                              &mDecoderAudioStreamIndex,
                              &mDecoderVideoStreamIndex);
  ENSURE_SUCCESS(hr, hr);
  ENSURE_TRUE(mDecoderVideoStreamIndex != -1, E_UNEXPECTED);

  hr = ConfigureReaderVideoOutput();
  ENSURE_SUCCESS(hr, hr);

  if (mDecoderAudioStreamIndex != -1) {
    hr = ConfigureReaderAudioOutput();
    ENSURE_SUCCESS(hr, hr);
  } else {
    mAudioEOS = true;
  }

  return S_OK;
}

HRESULT
AdjustFrameSizeForRotation(Rotation aRotation, UINT32* aWidth, UINT32* aHeight)
{
  ENSURE_TRUE(aWidth, E_POINTER);
  ENSURE_TRUE(aHeight, E_POINTER);
  if (aRotation == ROTATE_0 || aRotation == ROTATE_180) {
    // Dimensions are the same.
    return S_OK;
  }
  UINT32 temp = *aWidth;
  *aWidth = *aHeight;
  *aHeight = temp;

  return S_OK;
}

HRESULT
RotationTranscoder::GetEncoderVideoOutputType(IMFMediaType** aOutVideoType)
{
  HRESULT hr;

  IMFMediaTypePtr videoType;
  hr = MFCreateMediaType(&videoType);
  ENSURE_SUCCESS(hr, hr);
  hr = videoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  ENSURE_SUCCESS(hr, hr);
  hr = videoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  ENSURE_SUCCESS(hr, hr);
  // Note: We may not have successfully retrieved mAvgBitRate, that's OK, provided we set
  // it to -1. mAvgBitRate is initialized to -1 by the constructor.
  hr = videoType->SetUINT32(MF_MT_AVG_BITRATE, mAvgBitRate);
  ENSURE_SUCCESS(hr, hr);
  hr = videoType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
  ENSURE_SUCCESS(hr, hr);
  UINT32 width = mReaderOuptutPictureRegion.Area.cx; 
  UINT32 height = mReaderOuptutPictureRegion.Area.cy;
  hr = AdjustFrameSizeForRotation(mJob->GetRotation(), &width, &height);
  ENSURE_SUCCESS(hr, hr);
  
  if (height > 1080) {
    // TODO: Scale WRT aspect ratio, such that height <= 1080p.
    //float ratio = float(width) / float(height);
    //width = ratio * 1080;
    height = 1080;
  }
  hr = MFSetAttributeSize(videoType, MF_MT_FRAME_SIZE, width, height);
  ENSURE_SUCCESS(hr, hr);
  hr = MFSetAttributeRatio(videoType, MF_MT_FRAME_RATE, mFrameRateNumer, mFrameRateDenom);
  ENSURE_SUCCESS(hr, hr);

  // Swap the pixel aspect ratio if the frame is rotated 90 or 270 degrees.
  if (mJob->GetRotation() == ROTATE_180) {
    hr = MFSetAttributeRatio(videoType, MF_MT_PIXEL_ASPECT_RATIO, mPixelAspectRatioNumer, mPixelAspectRatioDenom);
  } else {
    hr = MFSetAttributeRatio(videoType, MF_MT_PIXEL_ASPECT_RATIO, mPixelAspectRatioDenom, mPixelAspectRatioNumer);
  }
  ENSURE_SUCCESS(hr, hr);
  
  if (mH264Profile > eAVEncH264VProfile_Main /* && !IsWindows8())*/ ) {
    // Note: Windows 8 Supports High as well.
    mH264Profile = eAVEncH264VProfile_Main;
  }
  if (mH264Profile != Unknown) {
    hr = videoType->SetUINT32(MF_MT_MPEG2_PROFILE, mH264Profile);
    ENSURE_SUCCESS(hr, hr);
  }
  *aOutVideoType = videoType.Detach();

  return S_OK;
}

HRESULT
RotationTranscoder::GetEncoderVideoInputType(IMFMediaType** aOutVideoType)
{
  HRESULT hr;

  IMFMediaTypePtr videoType;
  hr = MFCreateMediaType(&videoType);
  ENSURE_SUCCESS(hr, hr);

  hr = videoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  ENSURE_SUCCESS(hr, hr);

  hr = videoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
  ENSURE_SUCCESS(hr, hr);

  hr = videoType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
  ENSURE_SUCCESS(hr, hr);

  UINT32 width = mReaderOuptutPictureRegion.Area.cx;
  UINT32 height = mReaderOuptutPictureRegion.Area.cy;
  hr = AdjustFrameSizeForRotation(mJob->GetRotation(), &width, &height);
  ENSURE_SUCCESS(hr, hr);

  hr = MFSetAttributeSize(videoType, MF_MT_FRAME_SIZE, width, height);
  ENSURE_SUCCESS(hr, hr);

  hr = MFSetAttributeRatio(videoType, MF_MT_FRAME_RATE, mFrameRateNumer, mFrameRateDenom);
  ENSURE_SUCCESS(hr, hr);

  hr = MFSetAttributeRatio(videoType, MF_MT_PIXEL_ASPECT_RATIO, mPixelAspectRatioNumer, mPixelAspectRatioDenom);
  ENSURE_SUCCESS(hr, hr);

  // Note: We *must* set the stride on the input video type, as if it's
  // negative and we don't report that the image will be encoded upside down!
  hr = videoType->SetUINT32(MF_MT_DEFAULT_STRIDE, width * 4);
  ENSURE_SUCCESS(hr, hr);

  *aOutVideoType = videoType.Detach();

  return S_OK;
}

HRESULT
RotationTranscoder::GetEncoderAudioOutputType(IMFMediaType** aOutAudioType)
{
  // Construct the encoder's output audio type. This is AAC, but with
  // the channel/sample rate of the audio processor's output type.
  // The AAC encoder only supports up to 2 channels, 44.1KHz or 48KHz,
  // so the audio processor resamples for us if need be. We can't use
  // the audio processor's output type, since it includes extra attributes
  // which confuses the encoder.

  HRESULT hr;

  IMFMediaTypePtr outputAudioType;
  hr = MFCreateMediaType(&outputAudioType);
  ENSURE_SUCCESS(hr, hr);

  hr = outputAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  ENSURE_SUCCESS(hr, hr);

  hr = outputAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
  ENSURE_SUCCESS(hr, hr);

  IMFMediaTypePtr processorOutputType;
  hr = mAudioProcessor.GetOutputType(&processorOutputType);
  ENSURE_SUCCESS(hr, hr);

  GUID attr[] = {
    MF_MT_AUDIO_BITS_PER_SAMPLE,
    MF_MT_AUDIO_NUM_CHANNELS,
    MF_MT_AUDIO_SAMPLES_PER_SECOND,
    MF_MT_AUDIO_AVG_BYTES_PER_SECOND
  };

  for (UINT i=0; i<ARRAYSIZE(attr); i++) {
    UINT32 value;
    hr = processorOutputType->GetUINT32(attr[i], &value);
    ENSURE_SUCCESS(hr, hr);

    hr = outputAudioType->SetUINT32(attr[i], value);
    ENSURE_SUCCESS(hr, hr);
  }

  // Just set the audio output average bit rate to the max 192 Kbps.
  hr = outputAudioType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 20000);
  ENSURE_SUCCESS(hr, hr);

  *aOutAudioType = outputAudioType.Detach();
  return S_OK;
}

HRESULT
RotationTranscoder::GetEncoderAudioInputType(IMFMediaType** aOutEncoderAudioInputType)
{
  HRESULT hr;

  // Construct the audio input media type. We copy the relevant fields from
  // the output type of the audio processor.

  IMFMediaTypePtr processorOutputType;
  hr = mAudioProcessor.GetOutputType(&processorOutputType);
  ENSURE_SUCCESS(hr, hr);

  IMFMediaTypePtr encoderInputAudioType;
  hr = MFCreateMediaType(&encoderInputAudioType);
  ENSURE_SUCCESS(hr, hr);

  GUID guidAttrs[] = {
    MF_MT_MAJOR_TYPE,
    MF_MT_SUBTYPE
  };

  for (UINT i=0; i<ARRAYSIZE(guidAttrs); i++) {
    GUID value;
    hr = processorOutputType->GetGUID(guidAttrs[i], &value);
    ENSURE_SUCCESS(hr, hr);

    hr = encoderInputAudioType->SetGUID(guidAttrs[i], value);
    ENSURE_SUCCESS(hr, hr);
  }

  GUID uintAttrs[] = {
    MF_MT_AUDIO_BITS_PER_SAMPLE,
    MF_MT_AUDIO_NUM_CHANNELS,
    MF_MT_AUDIO_SAMPLES_PER_SECOND,
    MF_MT_AUDIO_AVG_BYTES_PER_SECOND
  };

  for (UINT i=0; i<ARRAYSIZE(uintAttrs); i++) {
    UINT32 value;
    hr = processorOutputType->GetUINT32(uintAttrs[i], &value);
    ENSURE_SUCCESS(hr, hr);

    hr = encoderInputAudioType->SetUINT32(uintAttrs[i], value);
    ENSURE_SUCCESS(hr, hr);
  }

  *aOutEncoderAudioInputType = encoderInputAudioType.Detach();

  return S_OK;
}

HRESULT
RotationTranscoder::Initialize()
{
  HRESULT hr;

  hr = mFrameRotator.Init();
  ENSURE_SUCCESS(hr, hr);

  hr = CreateReader();
  ENSURE_SUCCESS(hr, hr);

  const wstring& outputFilename = mJob->GetOutputFilename();

  IMFAttributesPtr attributes;
  hr = MFCreateAttributes(&attributes, 1);
  ENSURE_SUCCESS(hr, hr);
  hr = attributes->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
  ENSURE_SUCCESS(hr, hr);

  hr = attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
  ENSURE_SUCCESS(hr, hr);

  hr = MFCreateSinkWriterFromURL(outputFilename.c_str(), NULL, attributes, &mWriter);
  ENSURE_SUCCESS(hr, hr);

  // Set the encoded output video type.
  IMFMediaTypePtr encoderVideoOutputType;
  hr = GetEncoderVideoOutputType(&encoderVideoOutputType);
  ENSURE_SUCCESS(hr, hr);
  hr = mWriter->AddStream(encoderVideoOutputType,
                          &mEncoderVideoStreamIndex);
  ENSURE_SUCCESS(hr, hr);
  DBGMSG(L"Writer output video type:\n");
  LogMediaType(encoderVideoOutputType);
  DBGMSG(L"\n");

  // Set the writer input video type.
  IMFMediaTypePtr writerInputVideoType;
  hr = GetEncoderVideoInputType(&writerInputVideoType);
  ENSURE_SUCCESS(hr, hr);

  hr = GetDefaultStride(writerInputVideoType, &mWriterInputVideoStride);
  ENSURE_SUCCESS(hr, hr);

  DBGMSG(L"Writer input video type:\n");
  LogMediaType(writerInputVideoType);
  DBGMSG(L"\n");

  hr = mWriter->SetInputMediaType(mEncoderVideoStreamIndex,
                                  writerInputVideoType,
                                  NULL);
  ENSURE_SUCCESS(hr, hr);

  if (mDecoderAudioStreamIndex != -1) {
    // We have audio. Set the encoded output audio type.
    IMFMediaTypePtr encoderAudioOutputType;
    hr = GetEncoderAudioOutputType(&encoderAudioOutputType);
    ENSURE_SUCCESS(hr, hr);
    hr = mWriter->AddStream(encoderAudioOutputType,
                            &mEncoderAudioStreamIndex);
    ENSURE_SUCCESS(hr, hr);

    DBGMSG(L"Writer output audio type:\n");
    LogMediaType(encoderAudioOutputType);
    DBGMSG(L"\n");

    // Set the writer's audio input type.
    IMFMediaTypePtr writerInputAudioType;
    hr = GetEncoderAudioInputType(&writerInputAudioType);
    ENSURE_SUCCESS(hr, hr);
    DBGMSG(L"Writer input audio type:\n");
    LogMediaType(writerInputAudioType);
    DBGMSG(L"\n");
    hr = mWriter->SetInputMediaType(mEncoderAudioStreamIndex,
                                    writerInputAudioType,
                                    NULL);
    if (FAILED(hr)) {
      DBGMSG(L"Failed to set writer input audio type: hr=0x%x\n", hr);
    }
    ENSURE_SUCCESS(hr, hr);
  }

  hr = mWriter->BeginWriting();
  ENSURE_SUCCESS(hr, hr);

  mInitialized = true;

  return S_OK;
}

// Transcodes. Call this in a loop until *aOutPercentComplete == 100.
HRESULT
RotationTranscoder::Transcode()
{
  IMFSamplePtr sample;
  DWORD streamIndex, flags;
  LONGLONG timestamp;
  HRESULT hr;
  uint64_t videoSampleNum = 0;

  hr = mReader->ReadSample(MF_SOURCE_READER_ANY_STREAM,
                           0,                // Flags.
                           &streamIndex,     // Receives the actual stream index.
                           &flags,           // Receives status flags.
                           &timestamp,     // Receives the time stamp.
                           &sample);         // Receives the sample or NULL.
  if (FAILED(hr)) {
    DBGMSG(L"ReadSample failed with 0x%x\n", hr);
    return hr;
  }

  bool isVideoSample = streamIndex == mDecoderVideoStreamIndex;
  bool isAudioSample = streamIndex == mDecoderAudioStreamIndex;

  if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
    if (isVideoSample) {
      mVideoEOS = true;
    } else if (isAudioSample) {
      mAudioEOS = true;
    }
  }
  if (flags & MF_SOURCE_READERF_NEWSTREAM) {
    DBGMSG(L"ReadSample flag: New stream\n");
  }
  if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
    DBGMSG(L"Current media type change to:\n");
    IMFMediaTypePtr type;
    hr = mReader->GetCurrentMediaType(streamIndex, &type);
    ENSURE_SUCCESS(hr, hr)
    LogMediaType(type);
    DBGMSG(L"\n");
  }
  if (flags & MF_SOURCE_READERF_STREAMTICK) {
    DBGMSG(L"Stream tick\n");
  }
  if (flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED) {
    // The format changed. Reconfigure the decoder.
    DBGMSG(L"MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED\n");
  }

  if (!sample) {
    if (mAudioEOS && mVideoEOS) {
      mProgress = 1000;
      hr = mWriter->Finalize();
      ENSURE_SUCCESS(hr, hr);
    }
    return S_OK;
  }

  if (!isAudioSample && !isVideoSample) {
    DBGMSG(L"This stream should be deselected!\n");
    return S_OK;
  }

  if (isVideoSample) {
    videoSampleNum++;
    IMFSamplePtr rotated;
    hr = mFrameRotator.RotateFrame(mJob->GetRotation(),
                                   sample,
                                   &mReaderOuptutPictureRegion,
                                   mReaderOutputVideoStride,
                                   mWriterInputVideoStride,
                                   &rotated);
    ENSURE_SUCCESS(hr, hr);
    sample = rotated;

    mLastVideoTimestamp = timestamp;
  } else {
    // Audio sample.
    IMFSamplePtr processed;
    hr = mAudioProcessor.Process(sample, &processed, mAudioEOS);
    ENSURE_SUCCESS(hr, hr);
    sample = processed;
  }

  DWORD encoderStreamIndex = isVideoSample ? mEncoderVideoStreamIndex
                                           : mEncoderAudioStreamIndex;
  hr = mWriter->WriteSample(encoderStreamIndex, sample);
  if (FAILED(hr)) {
    DBGMSG(L"Failed writing %s, sample 0x%x hr=0x%x\n", (isAudioSample ? L"audio" : L"video"), hr);
    return hr;
  }

  if (mAudioEOS && mVideoEOS) {
    mProgress = 1000;
    hr = mWriter->Finalize();
    ENSURE_SUCCESS(hr, hr);
  } else {
    UINT32 progress = floor(1000.0 * (double)(mLastVideoTimestamp) / (double)(mDuration));
    mProgress = max(1, min(999, progress));
  }
  return S_OK;
}

UINT32
RotationTranscoder::GetProgress()
{
  return mProgress;
}
