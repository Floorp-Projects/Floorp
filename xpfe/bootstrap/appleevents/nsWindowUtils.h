/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Simon Fraser <sfraser@netscape.com>
 */


#ifndef nsWindowUtils_h_
#define nsWindowUtils_h_

#include <QuickDraw.h>
#include <MacWindows.h>

#include "nsAEDefs.h"


#ifdef __cplusplus
extern "C" {
#endif

long CountWindowsOfKind(TWindowKind windowKind);

WindowPtr GetNamedOrFrontmostWindow(TWindowKind windowKind, const char* windowName);
WindowPtr GetIndexedWindowOfKind(TWindowKind windowKind, TAEListIndex index);
TAEListIndex GetWindowIndex(TWindowKind windowKind, WindowPtr theWindow);

void GetCleanedWindowName(WindowPtr wind, char* outName, long maxLen);
void GetWindowUrlString(WindowPtr wind, char** outUrlStringPtr);
void GetWindowGlobalBounds(WindowPtr wind, Rect* outRect);

void LoadURLInWindow(WindowPtr wind, const char* urlString);

Boolean WindowIsResizeable(WindowPtr wind);
Boolean WindowIsZoomable(WindowPtr wind);
Boolean WindowIsZoomed(WindowPtr wind);
Boolean WindowHasTitleBar(WindowPtr wind);
Boolean WindowIsModal(WindowPtr wind);
Boolean WindowIsCloseable(WindowPtr wind);
Boolean WindowIsFloating(WindowPtr wind);
Boolean WindowIsModified(WindowPtr wind);


#ifdef __cplusplus
}
#endif


#endif // nsWindowUtils_h_
