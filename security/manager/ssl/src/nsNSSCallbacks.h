/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *  Brian Ryner <bryner@brianryner.com>
*/

#ifndef _NSNSSCALLBACKS_H_
#define _NSNSSCALLBACKS_H_

#include "pk11func.h"
#include "nspr.h"

char* PR_CALLBACK
PK11PasswordPrompt(PK11SlotInfo *slot, PRBool retry, void* arg);

void PR_CALLBACK HandshakeCallback(PRFileDesc *fd, void *client_data);
SECStatus PR_CALLBACK AuthCertificateCallback(void* client_data, PRFileDesc* fd,
                                              PRBool checksig, PRBool isServer);

#endif // _NSNSSCALLBACKS_H_



