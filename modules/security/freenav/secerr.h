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
#ifndef __SEC_ERR_H_
#define __SEC_ERR_H_


#define SEC_ERROR_BASE				(-0x2000)
#define SEC_ERROR_LIMIT				(SEC_ERROR_BASE + 1000)

#define IS_SEC_ERROR(code) \
    (((code) >= SEC_ERROR_BASE) && ((code) < SEC_ERROR_LIMIT))

#endif /* __SEC_ERR_H_ */
