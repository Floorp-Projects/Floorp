/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is nsCursors.
 *
 * The Initial Developer of the Original Code is Andrew Thompson.
 * Portions created by the Andrew Thompson are Copyright (C) 2004
 * Andrew Thompson. All Rights Reserved.
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsCursorManager_h_
#define nsCursorManager_h_

#import <Foundation/Foundation.h>
#include "nsIWidget.h"
#include "nsMacCursor.h"

/*! @class      nsCursorManager
    @abstract   Singleton service provides access to all cursors available in the application.
    @discussion Use <code>nsCusorManager</code> to set the current cursor using an XP <code>nsCusor</code> enum value.
                <code>nsCursorManager</code> encapsulates the details of setting different types of cursors, animating
                cursors and cleaning up cursors when they are no longer in use.
 */
@interface nsCursorManager : NSObject
{
  @private
  NSMutableDictionary *mCursors;
  nsCursor mCurrentCursor;
}

/*! @method     setCursor:
    @abstract   Sets the current cursor.
    @discussion Sets the current cursor to the cursor indicated by the XP cursor constant given as an argument.
                Resources associated with the previous cursor are cleaned up.
    @param aCursor the cursor to use
*/
- (void) setCursor: (nsCursor) aCursor;

/*! @method     sharedInstance
    @abstract   Get the Singleton instance of the cursor manager.
    @discussion Use this method to obtain a reference to the cursor manager.
    @result a reference to the cursor manager
*/
+ (nsCursorManager *) sharedInstance;

/*! @method     dispose
    @abstract   Releases the shared instance of the cursor manager.
    @discussion Use dispose to clean up the cursor manager and associated cursors.
*/
+ (void) dispose;
@end

#endif // nsCursorManager_h_
