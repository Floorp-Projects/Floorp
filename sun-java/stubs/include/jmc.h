/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
