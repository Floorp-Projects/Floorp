/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 *   Justin Bradford <jab@atdot.org>
 */

#ifndef _NSSOCKSIOLAYER_H_
#define _NSSOCKSIOLAYER_H_

#include "prtypes.h"
#include "prio.h"
#include "nsISOCKSSocketInfo.h"

nsresult nsSOCKSIOLayerNewSocket(const char *host, 
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRInt32 socksVersion,
                                 PRFileDesc **fd, 
                                 nsISupports **info);

nsresult nsSOCKSIOLayerAddToSocket(const char *host, 
                                   PRInt32 port,
                                   const char *proxyHost,
                                   PRInt32 proxyPort,
                                   PRInt32 socksVersion,
                                   PRFileDesc *fd, 
                                   nsISupports **info);

#endif /* _NSSOCKSIOLAYER_H_ */
