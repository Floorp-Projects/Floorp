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
#ifndef __SSL_ERR_H_
#define __SSL_ERR_H_


#ifdef XP_MAC
#define SSL_ERROR_BASE				(-3000)
#else
#define SSL_ERROR_BASE				(-0x3000)
#endif
#define SSL_ERROR_LIMIT				(SSL_ERROR_BASE + 1000)

#ifndef NO_SECURITY_ERROR_ENUM
typedef enum {
    SSL_ERROR_BAD_CERTIFICATE =	SSL_ERROR_BASE + 4
} SSLErrorCodes;
#endif

#define IS_SSL_ERROR(code) \
    (((code) >= SSL_ERROR_BASE) && ((code) < SSL_ERROR_LIMIT))

#endif /* __SSL_ERR_H_ */
