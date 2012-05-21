/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

class gfxASurface;
class nsIntRegion;
class nsIntSize;

namespace mozilla {

namespace Framebuffer {

//
// The general usage of Framebuffer is
//
// -- in initialization code --
//  Open();
//
// -- ready to paint next frame --
//  nsRefPtr<gfxASurface> backBuffer = BackBuffer();
//  // ...
//  Paint(backBuffer);
//  // ...
//  Present();
//

// Return true if the fbdev was successfully opened, along with the
// dimensions of the screen.  If this fails, the result of all further
// calls is undefined.  Open() is idempotent.
bool Open(nsIntSize* aScreenSize);

// After Close(), the result of all further calls is undefined.
// Close() is idempotent, and Open() can be called again after
// Close().
void Close();

bool GetSize(nsIntSize *aScreenSize);

// Return the buffer to be drawn into, that will be the next frame.
gfxASurface* BackBuffer();

// Swap the front buffer for the back buffer.  |aUpdated| is the
// region of the back buffer that was repainted.
void Present(const nsIntRegion& aUpdated);

} // namespace Framebuffer

} // namespace mozilla
