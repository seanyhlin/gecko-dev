/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScreenCocoa.h"
#include "nsObjCExceptions.h"
#include "nsCocoaUtils.h"

nsScreenCocoa::nsScreenCocoa (NSScreen *screen)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mScreen = [screen retain];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsScreenCocoa::~nsScreenCocoa ()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mScreen release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMETHODIMP
nsScreenCocoa::GetRect(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight)
{
  NSRect frame = [mScreen frame];

  nsIntRect r = nsCocoaUtils::CocoaRectToGeckoRectDevPix(frame, BackingScaleFactor());

  *outX = r.x;
  *outY = r.y;
  *outWidth = r.width;
  *outHeight = r.height;

  return NS_OK;
}

NS_IMETHODIMP
nsScreenCocoa::GetAvailRect(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight)
{
  NSRect frame = [mScreen visibleFrame];

  nsIntRect r = nsCocoaUtils::CocoaRectToGeckoRectDevPix(frame, BackingScaleFactor());

  *outX = r.x;
  *outY = r.y;
  *outWidth = r.width;
  *outHeight = r.height;

  return NS_OK;
}

NS_IMETHODIMP
nsScreenCocoa::GetPixelDepth(int32_t *aPixelDepth)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSWindowDepth depth = [mScreen depth];
  int bpp = NSBitsPerPixelFromDepth(depth);

  *aPixelDepth = bpp;
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsScreenCocoa::GetColorDepth(int32_t *aColorDepth)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSWindowDepth depth = [mScreen depth];
  int bpp = NSBitsPerPixelFromDepth (depth);

  *aColorDepth = bpp;
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsScreenCocoa::GetContentsScaleFactor(double *aContentsScaleFactor)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  *aContentsScaleFactor = (double) BackingScaleFactor();
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

CGFloat
nsScreenCocoa::BackingScaleFactor()
{
  return nsCocoaUtils::GetBackingScaleFactor(mScreen);
}
