/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef __nsIRegistryUtils_h
#define __nsIRegistryUtils_h

#define NS_REGISTRY_CONTRACTID "@mozilla.org/registry;1"
#define NS_REGISTRY_CLASSNAME "Mozilla Registry"
/* be761f00-a3b0-11d2-996c-0080c7cb1081 */
#define NS_REGISTRY_CID \
{ 0xbe761f00, 0xa3b0, 0x11d2, \
  {0x99, 0x6c, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x81} }

/*------------------------------- Error Codes ----------------------------------
------------------------------------------------------------------------------*/
#define NS_ERROR_REG_BADTYPE          NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 1 )
#define NS_ERROR_REG_NO_MORE          NS_ERROR_GENERATE_SUCCESS( NS_ERROR_MODULE_REG, 2 )
#define NS_ERROR_REG_NOT_FOUND        NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 3 )
#define NS_ERROR_REG_NOFILE	          NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 4 )
#define NS_ERROR_REG_BUFFER_TOO_SMALL NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 5 )
#define NS_ERROR_REG_NAME_TOO_LONG    NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 6 )
#define NS_ERROR_REG_NO_PATH          NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 7 )
#define NS_ERROR_REG_READ_ONLY        NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 8 )
#define NS_ERROR_REG_BAD_UTF8         NS_ERROR_GENERATE_FAILURE( NS_ERROR_MODULE_REG, 9 )

#endif
