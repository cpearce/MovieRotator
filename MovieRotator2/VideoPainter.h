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

#include "Interfaces.h"

class PlaybackClock;
class VideoDecoder;
struct MediaSample;

struct textured_vertex{
    float x, y, z, rhw;  // The transformed(screen space) position for the vertex.
    float tu,tv;         // Texture coordinates
};



class VideoPainter : public EventHandler {
public:
  VideoPainter(HWND aEventTarget, HINSTANCE hInst,
               FLOAT aX, FLOAT aY, FLOAT aWidth, FLOAT aHeight);
  ~VideoPainter();

  HRESULT Init();

  // EventHandler. Note this instance listens to event on its own window,
  // not the application window. But ChrisP, if you want to listen on both,
  // you just need to switch on the hWnd passed in! :)
  bool Handle(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;

  HRESULT SetInputs(VideoDecoder* aDecoder,
                    PlaybackClock* mClock,
                    Rotation aRotation);

  HRESULT GetDeviceManager(IDirect3DDeviceManager9** aOutDevMan);
  HRESULT GetD3D9(IDirect3D9** aOutD3D9);

  void Reset();
  void Start();
  void Pause();

  // Rotates 90 degrees in specified direction.
  void RotateClockwise();
  void RotateAntiClockwise();

private:

  void OnPaint();
  void PaintFrame();
  HRESULT UpdateBounds();
  HRESULT UpdateTexture();

  HRESULT InitializeVertexBuffer();

  // Rotates the array of verticies around the center of the canvas
  // by specified amount in degrees.
  void RotateVertices(textured_vertex* vertices, unsigned length, float degrees);

  // Creates mTexture as an RGB texture so that it can hold a frame of aType.
  HRESULT CreateTexture(IMFMediaType* aType);

  HRESULT GetD3DSurfaceFromCurrentFrame(IDirect3DSurface9** aOutSurface);

  HRESULT UploadCurrentFrameToGPU(IDirect3DSurface9** aOutSurface);

  // Parent Window details.
  HWND mEventTarget;
  HINSTANCE mHInst;

  FLOAT mX;
  FLOAT mY;
  FLOAT mWidth;
  FLOAT mHeight;

  int mRotationDelta;
  int mDegreesRotated;

  // Our window.
  HWND mHWND;

  // The bounds in which we render the current frame.
  // This is scaled WRT the aspect ratio, and letterboxed inside the bounds
  // of the paintable area.
  RECT mRenderBounds;

  IDirect3D9Ptr mD3D9;
  IDirect3DDevice9Ptr mDevice;
  IDirect3DDeviceManager9Ptr mDeviceManager;
  UINT mResetToken;

  UINT32 mVideoFrameWidth;
  UINT32 mVideoFrameHeight;
  LONG mVideoStride;

  VideoDecoder* mDecoder;
  PlaybackClock* mClock;

  MediaSample* mCurrentFrame;

  //Transformed vertex with 1 set of texture coordinates
  static const DWORD tri_fvf=D3DFVF_XYZRHW|D3DFVF_TEX1;

  int mTriangleCount;

  IDirect3DVertexBuffer9Ptr mVertexBuffer;
  IDirect3DTexture9Ptr mTexture;

  UINT_PTR mTimer;

  bool mPaused;

};
