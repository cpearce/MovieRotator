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

#pragma once

#include "FrameRotator.h"
#include "AudioProcessor.h"

class TranscodeJob;

enum {
  Unknown = -1
};

class RotationTranscoder {
public:
  RotationTranscoder(const TranscodeJob* aJob);
  ~RotationTranscoder();

  HRESULT Initialize();

  // Transcodes. Call this in a loop until GetPercentComplete() returns 100.
  HRESULT Transcode();

  // Returns how many thousandths through the transcode we are.
  UINT32 GetProgress();

private:

  HRESULT CreateReader();
  HRESULT ConfigureReaderVideoOutput();
  HRESULT ConfigureReaderAudioOutput();
  
  HRESULT DetermineReaderOutputVideoType(IMFMediaType** aOutVideoType);

  HRESULT GetEncoderVideoOutputType(IMFMediaType** aOutVideoType);
  HRESULT GetEncoderAudioOutputType(IMFMediaType** aOutAudioType);
  HRESULT GetEncoderVideoInputType(IMFMediaType** aOutVideoType);
  HRESULT GetEncoderAudioInputType(IMFMediaType** aOutAudioType);

  FrameRotator mFrameRotator;
  AudioProcessor mAudioProcessor;

  const TranscodeJob* mJob;

  IMFMediaTypePtr mReaderOutputRGBVideoType;

  IMFSourceReaderPtr mReader;
  IMFSinkWriterPtr mWriter;

  DWORD mDecoderVideoStreamIndex;
  DWORD mDecoderAudioStreamIndex;
  DWORD mEncoderVideoStreamIndex;
  DWORD mEncoderAudioStreamIndex;

  LONG mReaderOutputVideoStride;
  LONG mWriterInputVideoStride;

  // H264 stream properties, extracked from the reader's output.
  UINT32 mH264Profile;
  UINT32 mAvgBitRate;
  UINT32 mInterlaceMode;
  UINT32 mFrameRateNumer;
  UINT32 mFrameRateDenom;
  UINT32 mPixelAspectRatioNumer;
  UINT32 mPixelAspectRatioDenom;

  // The region inside the frame in which the picture actually resides.
  MFVideoArea mReaderOuptutPictureRegion;

  UINT32 mProgress;

  LONGLONG mDuration;
  LONGLONG mLastVideoTimestamp;

  bool mInitialized;
  bool mAudioEOS;
  bool mVideoEOS;
};
