/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
