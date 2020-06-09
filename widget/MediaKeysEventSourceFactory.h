/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_MEDIAKEYSEVENTSOURCEFACTORY_H_
#define WIDGET_MEDIAKEYSEVENTSOURCEFACTORY_H_

namespace mozilla {
namespace dom {
class MediaControlKeySource;
}  // namespace dom
}  // namespace mozilla

namespace mozilla {
namespace widget {

// This function declaration is used to create a media keys event source on
// different platforms, each platform should have their own implementation.
extern mozilla::dom::MediaControlKeySource* CreateMediaControlKeySource();

}  // namespace widget
}  // namespace mozilla

#endif
