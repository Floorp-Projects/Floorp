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
#ifndef __SEC_ERR_H_
#define __SEC_ERR_H_


#ifdef XP_MAC
#define SEC_ERROR_BASE				(-2000)
#else
#define SEC_ERROR_BASE				(-0x2000)
#endif
#define SEC_ERROR_LIMIT				(SEC_ERROR_BASE + 1000)

#define IS_SEC_ERROR(code) \
    (((code) >= SEC_ERROR_BASE) && ((code) < SEC_ERROR_LIMIT))

#endif /* __SEC_ERR_H_ */
