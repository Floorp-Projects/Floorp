/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#ifndef JMC_H
#define JMC_H

#include "jritypes.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define JMC_PUBLIC_API JRI_PUBLIC_API 

typedef struct JMCInterfaceID {
	jint a, b, c, d;
} JMCInterfaceID;

#ifdef __cplusplus
#define EXTERN_C		extern "C"
#define EXTERN_C_WITHOUT_EXTERN "C"
#else
#undef EXTERN_C
#define EXTERN_C
#define EXTERN_C_WITHOUT_EXTERN
#endif /* cplusplus */

typedef struct JMCException JMCException;

JRI_PUBLIC_API(void)
JMCException_Destroy(struct JMCException *);

#define JMC_EXCEPTION(resultPtr, exceptionToReturn)		 \
	(((resultPtr) != NULL)					 \
	 ? ((*(resultPtr) = (exceptionToReturn), resultPtr))	 \
	 : (JMCException_Destroy(exceptionToReturn), resultPtr))

#define JMC_EXCEPTION_RETURNED(resultPtr)			 \
	((resultPtr) != NULL && *(resultPtr) != NULL)

#define JMCEXCEPTION_OUT_OF_MEMORY	((struct JMCException*)-1)

#define JMC_DELETE_EXCEPTION(resultPtr)				 \
	(JMCException_Destroy(*(resultPtr)), *(resultPtr) = NULL)

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* JMC_H */
