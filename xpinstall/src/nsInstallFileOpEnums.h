/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsInstallFileOpEnums_h__
#define nsInstallFileOpEnums_h__

typedef enum nsInstallFileOpEnums {
  NS_FOP_DIR_CREATE         = 0,
  NS_FOP_DIR_REMOVE         = 1,
  NS_FOP_DIR_RENAME         = 2,
  NS_FOP_FILE_COPY          = 3,
  NS_FOP_FILE_DELETE        = 4,
  NS_FOP_FILE_EXECUTE       = 5,
  NS_FOP_FILE_MOVE          = 6,
  NS_FOP_FILE_RENAME        = 7,
  NS_FOP_WIN_SHORTCUT       = 8,
  NS_FOP_MAC_ALIAS          = 9,
  NS_FOP_UNIX_LINK          = 10,
  NS_FOP_FILE_SET_STAT      = 11

} nsInstallFileOpEnums;

#endif /* nsInstallFileOpEnums_h__ */
