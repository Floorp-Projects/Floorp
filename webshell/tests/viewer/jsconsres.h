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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsconsres_h___
#define jsconsres_h___


// Main menu id
#define JSCONSOLE_MENU                  20100

#define FILEMENUPOS                     0
#define ID_FILEMENU                     20200
// File menu items ids
#define ID_FILENEW                      20201
#define ID_FILEOPEN                     20202
#define ID_FILESAVE                     20203
#define ID_FILESAVEAS                   20204
// separator
#define ID_FILEEXIT                     20206

#define EDITMENUPOS                     1
#define ID_EDITMENU                     20300
// Edit menu items ids
#define ID_EDITUNDO                     20301
// separator
#define ID_EDITCUT                      20303
#define ID_EDITCOPY                     20304
#define ID_EDITPASTE                    20305
#define ID_EDITDELETE                   20306
// separator
#define ID_EDITSELECTALL                20308

#define COMMANDSMENUPOS                 2
#define ID_COMMANDSMENU                 20400
// Commands menu items ids
#define ID_COMMANDSEVALALL              20401
#define ID_COMMANDSEVALSEL              20402
// separator
#define ID_COMMANDSINSPECTOR            20404

#define HELPMENUPOS                     3
#define ID_HELPMENU                     20500
// Help menu items
#define ID_NOHELP                       20501

// 
// Accelerators table ids
//
#define ACCELERATOR_TABLE               1000

#endif // jsconsres_h___

