/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file defines constants to be used in the "subtype" field of
 * NSApplicationDefined type NSEvents.
 */

#ifndef WIDGET_COCOA_CUSTOMCOCOAEVENTS_H_
#define WIDGET_COCOA_CUSTOMCOCOAEVENTS_H_

// Empty event, just used for prodding the event loop into responding.
const short kEventSubtypeNone = 0;
// Tracer event, used for timing the event loop responsiveness.
const short kEventSubtypeTrace = 1;

#endif /* WIDGET_COCOA_CUSTOMCOCOAEVENTS_H_ */
