/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#undef DARWIN
#import <Cocoa/Cocoa.h>
class nsIWidget;


//
// protocol mozView
//
// A protocol listing all the methods that an object which wants
// to live in gecko's widget hierarchy must implement. |nsChildView|
// makes assumptions that any NSView with which it comes in contact will
// implement this protocol.
//

@protocol mozView

  // access the nsIWidget associated with this view. DOES NOT ADDREF.
- (nsIWidget*) widget;

  // access the native cocoa window (NSWindow) that this view
  // is in. It's necessary for a gecko NSView to keep track of the
  // window because |-window| returns nil when the view has been
  // removed from the view hierarchy (as is the case when it's hidden, 
  // since you can't just hide a view, that would make too much sense).
- (NSWindow*) getNativeWindow;
- (void) setNativeWindow: (NSWindow*)aWindow;

@end

