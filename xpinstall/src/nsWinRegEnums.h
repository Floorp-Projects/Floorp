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
