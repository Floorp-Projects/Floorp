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
#ifndef __SSL_ERR_H_
#define __SSL_ERR_H_


#define SSL_ERROR_BASE				(-0x3000)
#define SSL_ERROR_LIMIT				(SSL_ERROR_BASE + 1000)

#ifndef NO_SECURITY_ERROR_ENUM
typedef enum {
    SSL_ERROR_BAD_CERTIFICATE =	SSL_ERROR_BASE + 4
} SSLErrorCodes;
#endif

#define IS_SSL_ERROR(code) \
    (((code) >= SSL_ERROR_BASE) && ((code) < SSL_ERROR_LIMIT))

#endif /* __SSL_ERR_H_ */
