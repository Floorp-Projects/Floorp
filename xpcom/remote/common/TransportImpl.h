/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef __TransportImpl_h__
#define __TransportImpl_h__
#include "ITransport.h"
#include "IPCbridge.h"

class TransportImpl : public ITransport {
    NS_DECL_ISUPPORTS
    NS_IMETHOD Read(void** data, PRUint32 * size);
    NS_IMETHOD Write(void* data, PRUint32 size);
    TransportImpl(const char* serverName = NULL);
    virtual ~TransportImpl();
 protected:
    IPCBridge * bridge; //nb temporary
};
#endif  /* __TransportImpl_h__ */

