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

/*******************************************************************************
 * Source date: 22 Feb 1997 07:07:50 GMT
 * netscape/libimg/PSIMGCB module C header file
 ******************************************************************************/

#ifndef _MPSIMGCB_H_
#define _MPSIMGCB_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "jmc.h"

/*******************************************************************************
 * PSIMGCB
 ******************************************************************************/

/* The type of the PSIMGCB interface. */
struct PSIMGCBInterface;

/* The public type of a PSIMGCB instance. */
typedef struct PSIMGCB {
	const struct PSIMGCBInterface*	vtable;
} PSIMGCB;

/* The inteface ID of the PSIMGCB interface. */
#ifndef JMC_INIT_PSIMGCB_ID
extern EXTERN_C_WITHOUT_EXTERN const JMCInterfaceID PSIMGCB_ID;
#else
EXTERN_C const JMCInterfaceID PSIMGCB_ID = { 0x38775d44, 0x23525963, 0x7f763562, 0x625c5b2a };
#endif /* JMC_INIT_PSIMGCB_ID */
/*******************************************************************************
 * PSIMGCB Operations
 ******************************************************************************/

#define PSIMGCB_getInterface(self, a, exception)	\
	(((self)->vtable->getInterface)(self, PSIMGCB_getInterface_op, a, exception))

#define PSIMGCB_addRef(self, exception)	\
	(((self)->vtable->addRef)(self, PSIMGCB_addRef_op, exception))

#define PSIMGCB_release(self, exception)	\
	(((self)->vtable->release)(self, PSIMGCB_release_op, exception))

#define PSIMGCB_hashCode(self, exception)	\
	(((self)->vtable->hashCode)(self, PSIMGCB_hashCode_op, exception))

#define PSIMGCB_equals(self, obj, exception)	\
	(((self)->vtable->equals)(self, PSIMGCB_equals_op, obj, exception))

#define PSIMGCB_clone(self, exception)	\
	(((self)->vtable->clone)(self, PSIMGCB_clone_op, exception))

#define PSIMGCB_toString(self, exception)	\
	(((self)->vtable->toString)(self, PSIMGCB_toString_op, exception))

#define PSIMGCB_finalize(self, exception)	\
	(((self)->vtable->finalize)(self, PSIMGCB_finalize_op, exception))

#define PSIMGCB_NewPixmap(self, a, b, c, d, e)	\
	(((self)->vtable->NewPixmap)(self, PSIMGCB_NewPixmap_op, a, b, c, d, e))

#define PSIMGCB_UpdatePixmap(self, a, b, c, d, e, f)	\
	(((self)->vtable->UpdatePixmap)(self, PSIMGCB_UpdatePixmap_op, a, b, c, d, e, f))

#define PSIMGCB_ControlPixmapBits(self, a, b, c)	\
	(((self)->vtable->ControlPixmapBits)(self, PSIMGCB_ControlPixmapBits_op, a, b, c))

#define PSIMGCB_DestroyPixmap(self, a, b)	\
	(((self)->vtable->DestroyPixmap)(self, PSIMGCB_DestroyPixmap_op, a, b))

#define PSIMGCB_DisplayPixmap(self, a, b, c, d, e, f, g, h, i, j, k)	\
	(((self)->vtable->DisplayPixmap)(self, PSIMGCB_DisplayPixmap_op, a, b, c, d, e, f, g, h, i, j, k))

#define PSIMGCB_DisplayIcon(self, a, b, c, d)	\
	(((self)->vtable->DisplayIcon)(self, PSIMGCB_DisplayIcon_op, a, b, c, d))

#define PSIMGCB_GetIconDimensions(self, a, b, c, d)	\
	(((self)->vtable->GetIconDimensions)(self, PSIMGCB_GetIconDimensions_op, a, b, c, d))

/*******************************************************************************
 * PSIMGCB Interface
 ******************************************************************************/

struct netscape_jmc_JMCInterfaceID;
struct java_lang_Object;
struct java_lang_String;
struct netscape_jmc_CType;
#include "il_types.h"
struct netscape_libimg_int_t;

struct PSIMGCBInterface {
	void*	(*getInterface)(struct PSIMGCB* self, jint op, const JMCInterfaceID* a, JMCException* *exception);
	void	(*addRef)(struct PSIMGCB* self, jint op, JMCException* *exception);
	void	(*release)(struct PSIMGCB* self, jint op, JMCException* *exception);
	jint	(*hashCode)(struct PSIMGCB* self, jint op, JMCException* *exception);
	jbool	(*equals)(struct PSIMGCB* self, jint op, void* obj, JMCException* *exception);
	void*	(*clone)(struct PSIMGCB* self, jint op, JMCException* *exception);
	const char*	(*toString)(struct PSIMGCB* self, jint op, JMCException* *exception);
	void	(*finalize)(struct PSIMGCB* self, jint op, JMCException* *exception);
	void	(*NewPixmap)(struct PSIMGCB* self, jint op, void* a, jint b, jint c, IL_Pixmap* d, IL_Pixmap* e);
	void	(*UpdatePixmap)(struct PSIMGCB* self, jint op, void* a, IL_Pixmap* b, jint c, jint d, jint e, jint f);
	void	(*ControlPixmapBits)(struct PSIMGCB* self, jint op, void* a, IL_Pixmap* b, IL_PixmapControl c);
	void	(*DestroyPixmap)(struct PSIMGCB* self, jint op, void* a, IL_Pixmap* b);
	void	(*DisplayPixmap)(struct PSIMGCB* self, jint op, void* a, IL_Pixmap* b, IL_Pixmap* c, jint d, jint e, jint f, jint g, jint h, jint i, jint j, jint k);
	void	(*DisplayIcon)(struct PSIMGCB* self, jint op, void* a, jint b, jint c, jint d);
	void	(*GetIconDimensions)(struct PSIMGCB* self, jint op, void* a, int* b, int* c, jint d);
};

/*******************************************************************************
 * PSIMGCB Operation IDs
 ******************************************************************************/

typedef enum PSIMGCBOperations {
	PSIMGCB_getInterface_op,
	PSIMGCB_addRef_op,
	PSIMGCB_release_op,
	PSIMGCB_hashCode_op,
	PSIMGCB_equals_op,
	PSIMGCB_clone_op,
	PSIMGCB_toString_op,
	PSIMGCB_finalize_op,
	PSIMGCB_NewPixmap_op,
	PSIMGCB_UpdatePixmap_op,
	PSIMGCB_ControlPixmapBits_op,
	PSIMGCB_DestroyPixmap_op,
	PSIMGCB_DisplayPixmap_op,
	PSIMGCB_DisplayIcon_op,
	PSIMGCB_GetIconDimensions_op
} PSIMGCBOperations;

/*******************************************************************************
 * Writing your C implementation: "PSIMGCB.h"
 * *****************************************************************************
 * You must create a header file named "PSIMGCB.h" that implements
 * the struct PSIMGCBImpl, including the struct PSIMGCBImplHeader
 * as it's first field:
 * 
 * 		#include "MPSIMGCB.h" // generated header
 * 
 * 		struct PSIMGCBImpl {
 * 			PSIMGCBImplHeader	header;
 * 			<your instance data>
 * 		};
 * 
 * This header file will get included by the generated module implementation.
 ******************************************************************************/

/* Forward reference to the user-defined instance struct: */
typedef struct PSIMGCBImpl	PSIMGCBImpl;


/* This struct must be included as the first field of your instance struct: */
typedef struct PSIMGCBImplHeader {
	const struct PSIMGCBInterface*	vtablePSIMGCB;
	jint		refcount;
} PSIMGCBImplHeader;

/*******************************************************************************
 * Instance Casting Macros
 * These macros get your back to the top of your instance, PSIMGCB,
 * given a pointer to one of its interfaces.
 ******************************************************************************/

#define PSIMGCBImpl2IMGCBIF(PSIMGCBImplPtr) \
	((IMGCBIF*)((char*)(PSIMGCBImplPtr) + offsetof(PSIMGCBImplHeader, vtablePSIMGCB)))

#define IMGCBIF2PSIMGCBImpl(IMGCBIFPtr) \
	((PSIMGCBImpl*)((char*)(IMGCBIFPtr) - offsetof(PSIMGCBImplHeader, vtablePSIMGCB)))

#define PSIMGCBImpl2PSIMGCB(PSIMGCBImplPtr) \
	((PSIMGCB*)((char*)(PSIMGCBImplPtr) + offsetof(PSIMGCBImplHeader, vtablePSIMGCB)))

#define PSIMGCB2PSIMGCBImpl(PSIMGCBPtr) \
	((PSIMGCBImpl*)((char*)(PSIMGCBPtr) - offsetof(PSIMGCBImplHeader, vtablePSIMGCB)))

/*******************************************************************************
 * Operations you must implement
 ******************************************************************************/


extern JMC_PUBLIC_API(void*)
_PSIMGCB_getBackwardCompatibleInterface(struct PSIMGCB* self, const JMCInterfaceID* iid,
	JMCException* *exception);

extern JMC_PUBLIC_API(void)
_PSIMGCB_init(struct PSIMGCB* self, JMCException* *exception);

extern JMC_PUBLIC_API(void*)
_PSIMGCB_getInterface(struct PSIMGCB* self, jint op, const JMCInterfaceID* a, JMCException* *exception);

extern JMC_PUBLIC_API(void)
_PSIMGCB_addRef(struct PSIMGCB* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(void)
_PSIMGCB_release(struct PSIMGCB* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(jint)
_PSIMGCB_hashCode(struct PSIMGCB* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(jbool)
_PSIMGCB_equals(struct PSIMGCB* self, jint op, void* obj, JMCException* *exception);

extern JMC_PUBLIC_API(void*)
_PSIMGCB_clone(struct PSIMGCB* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(const char*)
_PSIMGCB_toString(struct PSIMGCB* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(void)
_PSIMGCB_finalize(struct PSIMGCB* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(void)
_PSIMGCB_NewPixmap(struct PSIMGCB* self, jint op, void* a, jint b, jint c, IL_Pixmap* d, IL_Pixmap* e);

extern JMC_PUBLIC_API(void)
_PSIMGCB_UpdatePixmap(struct PSIMGCB* self, jint op, void* a, IL_Pixmap* b, jint c, jint d, jint e, jint f);

extern JMC_PUBLIC_API(void)
_PSIMGCB_ControlPixmapBits(struct PSIMGCB* self, jint op, void* a, IL_Pixmap* b, IL_PixmapControl c);

extern JMC_PUBLIC_API(void)
_PSIMGCB_DestroyPixmap(struct PSIMGCB* self, jint op, void* a, IL_Pixmap* b);

extern JMC_PUBLIC_API(void)
_PSIMGCB_DisplayPixmap(struct PSIMGCB* self, jint op, void* a, IL_Pixmap* b, IL_Pixmap* c, jint d, jint e, jint f, jint g, jint h, jint i, jint j, jint k);

extern JMC_PUBLIC_API(void)
_PSIMGCB_DisplayIcon(struct PSIMGCB* self, jint op, void* a, jint b, jint c, jint d);

extern JMC_PUBLIC_API(void)
_PSIMGCB_GetIconDimensions(struct PSIMGCB* self, jint op, void* a, int* b, int* c, jint d);

/*******************************************************************************
 * Factory Operations
 ******************************************************************************/

JMC_PUBLIC_API(PSIMGCB*)
PSIMGCBFactory_Create(JMCException* *exception);

/******************************************************************************/

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* _MPSIMGCB_H_ */
