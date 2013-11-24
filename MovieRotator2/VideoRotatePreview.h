#pragma once

#include "Interfaces.h"

class VideoRotatePreview : public Paintable {
public:
  VideoRotatePreview(FLOAT aLeft, FLOAT aTop,
                     FLOAT aWidth, FLOAT aHeight);
  ~VideoRotatePreview();

  HRESULT SetVideoFile(const std::wstring& aFilename);

  HRESULT SetRotation(Rotation aRotation);

  // Paintable.
  void Paint(ID2D1RenderTarget* renderTarget);

  // Clears the preview, resets state.
  void Reset();

private:
  BOOL           mTopDown;
  D2D1_SIZE_F    mAspectRatio;

  D2D1_RECT_F    mBounds;    // Bounding box, as a normalized rectangle.
  D2D1_RECT_F    mFill;        // Actual fill rectangle in pixels.
  D2D1_RECT_F    mSourceRect;

  // Client area bounding coords.
  const FLOAT mLeft;
  const FLOAT mTop;
  const FLOAT mWidth;
  const FLOAT mHeight;

  UINT32 mRotation; // In degrees.
};
