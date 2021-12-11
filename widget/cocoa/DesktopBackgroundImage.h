/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_COCOA_DESKTOPBACKGROUNDIMAGE_H_
#define WIDGET_COCOA_DESKTOPBACKGROUNDIMAGE_H_

class nsIFile;

namespace mozilla {
namespace widget {

void SetDesktopImage(nsIFile* aImage);

}  // namespace widget
}  // namespace mozilla

#endif  // WIDGET_COCOA_DESKTOPBACKGROUNDIMAGE_H_
