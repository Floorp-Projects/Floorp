/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __ftpCore_h___
#define __ftpCore_h___

#include "nsError.h"
 
//////////////////////////////
//// FTP CODES      RANGE: 20-30
//////////////////////////////
#define NS_ERROR_FTP_LOGIN \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 21)

#define NS_ERROR_FTP_MODE \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 22)

#define NS_ERROR_FTP_CWD \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 23)

#define NS_ERROR_FTP_PASV \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 24)

#define NS_ERROR_FTP_DEL_DIR \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 25)

#define NS_ERROR_FTP_MKDIR \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 26)

/**
 * Status nsresult codes: used with nsINotification objects 
 */
#define NS_NET_STATUS_BEGIN_FTP_TRANSACTION \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 27)

#define NS_NET_STATUS_END_FTP_TRANSACTION \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 28)

#endif // __ftpCore_h___






