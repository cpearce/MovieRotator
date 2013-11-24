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
#include "AudioProcessor.h"

HRESULT
AudioProcessor::SetInputType(IMFMediaType* aInputType)
{
  ENSURE_TRUE(aInputType, E_POINTER);

  HRESULT hr = ConfigureOutputTypeForInput(aInputType);
  ENSURE_SUCCESS(hr, hr);

  mInputType = aInputType;
  return S_OK;
}

HRESULT
AudioProcessor::ConfigureOutputTypeForInput(IMFMediaType* aInputType)
{
  ENSURE_TRUE(aInputType, E_POINTER);

  HRESULT hr;

  GUID subtype;
  hr = aInputType->GetGUID(MF_MT_SUBTYPE, &subtype);
  ENSURE_SUCCESS(hr, hr);

  ENSURE_TRUE(subtype == MFAudioFormat_PCM, E_FAIL);

  // Otherwise we need to encode. Check to see if we need to resample.
  UINT32 samplesPerSecond;
  hr = aInputType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samplesPerSecond);
  ENSURE_SUCCESS(hr, hr);

  UINT32 channels;
  hr = aInputType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
  ENSURE_SUCCESS(hr, hr);

  if ((samplesPerSecond == 44100 || samplesPerSecond == 48000) &&
      channels == 2) {
    // The samples are of the rates supported by the AAC encoder,
    // as is the channel count, no need to resample, so pass through,
    // don't resample.
    mOutputType = aInputType;
    return S_OK;
  }

  // Otherwise, we need to resample!
  hr = ConfigureResampler(aInputType);
  ENSURE_SUCCESS(hr, hr);

  return S_OK;
}

static const UINT32 OutputNumChannels = 2;
static const UINT32 OutputBitsPerSample = 16;
static const UINT32 OutputSamplesPerSec = 48000;
static const UINT32 OutputBlockAlign = 4; // NumChannels * bytesPerSample
static const UINT32 OutputBytePerSec = 192000; // BlockAlign*SamplesPerSec

HRESULT
AudioProcessor::CreateResamplerOutputType()
{
  HRESULT hr;

  IMFMediaTypePtr outputType;
  hr = MFCreateMediaType(&outputType);
  ENSURE_SUCCESS(hr, hr);

  hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  ENSURE_SUCCESS(hr, hr);

  hr = outputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
  ENSURE_SUCCESS(hr, hr);

  hr = outputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, OutputBitsPerSample);
  ENSURE_SUCCESS(hr, hr);

  hr = outputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, OutputNumChannels);
  ENSURE_SUCCESS(hr, hr);

  hr = outputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, OutputSamplesPerSec);
  ENSURE_SUCCESS(hr, hr);

  hr = outputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, OutputBytePerSec);
  ENSURE_SUCCESS(hr, hr);

  hr = outputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, OutputBlockAlign);
  ENSURE_SUCCESS(hr, hr);

  hr = outputType->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK,
                             SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
  ENSURE_SUCCESS(hr, hr);

  hr = outputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
  ENSURE_SUCCESS(hr, hr);

  mOutputType = outputType;

  return S_OK;
}
HRESULT
AudioProcessor::ConfigureResampler(IMFMediaType* aInputType)
{
  HRESULT hr;

  hr = aInputType->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &mInputAvgBytesPerSec);
  ENSURE_SUCCESS(hr, hr);

  // Create the resampler MFT
  IUnknownPtr unknown;
  hr = CoCreateInstance(CLSID_CResamplerMediaObject,
                        NULL,
                        CLSCTX_INPROC_SERVER,
                        IID_IUnknown,
                        (void**)&unknown);
  ENSURE_SUCCESS(hr, hr);

  hr = unknown->QueryInterface(IID_PPV_ARGS(&mResampler));
  ENSURE_SUCCESS(hr, hr);

  hr = mResampler->QueryInterface(IID_PPV_ARGS(&mResampleProps));
  ENSURE_SUCCESS(hr, hr);

  hr = mResampleProps->SetHalfFilterLength(60); //< best conversion quality
  ENSURE_SUCCESS(hr, hr);

  // Set input type.
  hr = mResampler->SetInputType(0, aInputType, 0);
  ENSURE_SUCCESS(hr, hr);

  hr = CreateResamplerOutputType();
  ENSURE_SUCCESS(hr, hr);

  hr = mResampler->SetOutputType(0, mOutputType, 0);
  ENSURE_SUCCESS(hr, hr);

  hr = mResampler->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL);
  ENSURE_SUCCESS(hr, hr);

  hr = mResampler->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL);
  ENSURE_SUCCESS(hr, hr);

  hr = mResampler->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL);
  ENSURE_SUCCESS(hr, hr);

  return S_OK;
}

HRESULT
AudioProcessor::GetOutputType(IMFMediaType** aOutType)
{
  ENSURE_TRUE(aOutType, E_POINTER);
  *aOutType = mOutputType;
  (*aOutType)->AddRef();
  return S_OK;
}

HRESULT
AudioProcessor::Process(IMFSample* aInput, IMFSample** aOutput, bool aEOS)
{
  ENSURE_TRUE(aInput, E_POINTER);
  ENSURE_TRUE(aOutput, E_POINTER);
  HRESULT hr;

  if (!mResampler) {
    // Pass through mode, just addref the sample and put it in aOutput.
    *aOutput = aInput;
    (*aOutput)->AddRef();
    return S_OK;
  }

  // Otherwise, we need to resample!

  // Calculate how many bytes we'll get out.
  DWORD numInputBytes = 0;
  hr = aInput->GetTotalLength(&numInputBytes);
  ENSURE_SUCCESS(hr, hr);
  int64_t numOutputBytes64 = numInputBytes * OutputBytePerSec / mInputAvgBytesPerSec;
  ENSURE_TRUE(numOutputBytes64 < INT32_MAX, E_FAIL);
  DWORD numOutputBytes = (DWORD)(numOutputBytes64);

  // Send the input to resampler.
  hr = mResampler->ProcessInput(0, aInput, 0);
  ENSURE_SUCCESS(hr, hr);

  // Clone the input sample. This is important, so that we copy timestamps, etc.
  IMFSamplePtr outSample;
  hr = CloneSample(aInput, &outSample);
  ENSURE_SUCCESS(hr, hr);

  if (aEOS) {
    hr = mResampler->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, NULL);
    ENSURE_SUCCESS(hr, hr);

    mResampler->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, NULL);
    ENSURE_SUCCESS(hr, hr);
  }

  while (1) {

    MFT_OUTPUT_DATA_BUFFER outputDataBuffer = {0};
    IMFSamplePtr sample;
    hr = MFCreateSample(&sample);
    ENSURE_SUCCESS(hr, hr);

    IMFMediaBufferPtr buffer;
    hr = MFCreateMemoryBuffer(numOutputBytes, &buffer);
    ENSURE_SUCCESS(hr, hr);

    hr = sample->AddBuffer(buffer);
    ENSURE_SUCCESS(hr, hr);

    outputDataBuffer.pSample = sample;

    DWORD dwStatus;
    hr = mResampler->ProcessOutput(0, 1, &outputDataBuffer, &dwStatus);
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      // conversion end
      // Shouldn't happen!
      break;
    }
    ENSURE_SUCCESS(hr, hr);

    DWORD totalLength = 0;
    hr = sample->GetTotalLength(&totalLength);
    ENSURE_SUCCESS(hr, hr);

    IMFMediaBufferPtr outBuffer;
    hr = MFCreateMemoryBuffer(totalLength, &outBuffer);
    ENSURE_SUCCESS(hr, hr);

    hr = sample->CopyToBuffer(outBuffer);
    ENSURE_SUCCESS(hr, hr);

    hr = outSample->AddBuffer(outBuffer);
    ENSURE_SUCCESS(hr, hr);
  }

  if (aEOS) {
    hr = mResampler->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, NULL);
    ENSURE_SUCCESS(hr, hr);
  }

  *aOutput = outSample.Detach();

  return S_OK;
}
