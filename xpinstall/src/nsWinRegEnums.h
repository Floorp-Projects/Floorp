/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsWinRegEnums_h__
#define nsWinRegEnums_h__

typedef enum nsWinRegEnum {
  NS_WIN_REG_CREATE          = 1,
  NS_WIN_REG_DELETE          = 2,
  NS_WIN_REG_DELETE_VAL      = 3,
  NS_WIN_REG_SET_VAL_STRING  = 4,
  NS_WIN_REG_SET_VAL_NUMBER  = 5,
  NS_WIN_REG_SET_VAL         = 6

} nsWinRegEnum;


typedef enum nsWinRegValueEnum {
  NS_WIN_REG_SZ                          = 1,
  NS_WIN_REG_EXPAND_SZ                   = 2,
  NS_WIN_REG_BINARY                      = 3,
  NS_WIN_REG_DWORD                       = 4,
  NS_WIN_REG_DWORD_LITTLE_ENDIAN         = 4,
  NS_WIN_REG_DWORD_BIG_ENDIAN            = 5,
  NS_WIN_REG_LINK                        = 6,
  NS_WIN_REG_MULTI_SZ                    = 7,
  NS_WIN_REG_RESOURCE_LIST               = 8,
  NS_WIN_REG_FULL_RESOURCE_DESCRIPTOR    = 9,
  NS_WIN_REG_RESOURCE_REQUIREMENTS_LIST  = 10
} nsWinRegValueEnum;

#endif /* nsWinRegEnums_h__ */
