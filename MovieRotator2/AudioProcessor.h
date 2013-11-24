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

// Resamples audio if neccessary, so that it's in an appropriate format for
// the AAC encoder MFT, which only accepts PCM audio as 16 bits per sample,
// 1 or 2 channels, and 44100 and 48000 Hz.
class AudioProcessor {
public:

  HRESULT SetInputType(IMFMediaType* aType);

  HRESULT GetOutputType(IMFMediaType** aOutType);

  HRESULT Process(IMFSample* aInput, IMFSample** aOutput, bool aEOS);

private:

  HRESULT ConfigureOutputTypeForInput(IMFMediaType* aInputType);
  HRESULT ConfigureResampler(IMFMediaType* aInputType);
  HRESULT CreateResamplerOutputType();

  IMFMediaTypePtr mInputType;
  IMFMediaTypePtr mOutputType;

  IMFTransformPtr mResampler;
  IWMResamplerPropsPtr mResampleProps;

  UINT32 mInputAvgBytesPerSec;
};
