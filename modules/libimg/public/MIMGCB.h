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
 * Source date: 16 Jan 1997 01:24:04 GMT
 * netscape/libimg/IMGCB module C header file
 ******************************************************************************/

#ifndef _MIMGCB_H_
#define _MIMGCB_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "jmc.h"

/*******************************************************************************
 * IMGCB
 ******************************************************************************/

/* The type of the IMGCB interface. */
struct IMGCBInterface;

/* The public type of a IMGCB instance. */
typedef struct IMGCB {
	const struct IMGCBInterface*	vtable;
} IMGCB;

/* The inteface ID of the IMGCB interface. */
#ifndef JMC_INIT_IMGCB_ID
extern EXTERN_C_WITHOUT_EXTERN const JMCInterfaceID IMGCB_ID;
#else
EXTERN_C const JMCInterfaceID IMGCB_ID = { 0x38775d44, 0x23525963, 0x7f763562, 0x625c5b2a };
#endif /* JMC_INIT_IMGCB_ID */
/*******************************************************************************
 * IMGCB Operations
 ******************************************************************************/

#define IMGCB_getInterface(self, a, exception)	\
	(((self)->vtable->getInterface)(self, IMGCB_getInterface_op, a, exception))

#define IMGCB_addRef(self, exception)	\
	(((self)->vtable->addRef)(self, IMGCB_addRef_op, exception))

#define IMGCB_release(self, exception)	\
	(((self)->vtable->release)(self, IMGCB_release_op, exception))

#define IMGCB_hashCode(self, exception)	\
	(((self)->vtable->hashCode)(self, IMGCB_hashCode_op, exception))

#define IMGCB_equals(self, obj, exception)	\
	(((self)->vtable->equals)(self, IMGCB_equals_op, obj, exception))

#define IMGCB_clone(self, exception)	\
	(((self)->vtable->clone)(self, IMGCB_clone_op, exception))

#define IMGCB_toString(self, exception)	\
	(((self)->vtable->toString)(self, IMGCB_toString_op, exception))

#define IMGCB_finalize(self, exception)	\
	(((self)->vtable->finalize)(self, IMGCB_finalize_op, exception))

#define IMGCB_NewPixmap(self, a, b, c, d, e)	\
	(((self)->vtable->NewPixmap)(self, IMGCB_NewPixmap_op, a, b, c, d, e))

#define IMGCB_UpdatePixmap(self, a, b, c, d, e, f)	\
	(((self)->vtable->UpdatePixmap)(self, IMGCB_UpdatePixmap_op, a, b, c, d, e, f))

#define IMGCB_ControlPixmapBits(self, a, b, c)	\
	(((self)->vtable->ControlPixmapBits)(self, IMGCB_ControlPixmapBits_op, a, b, c))

#define IMGCB_DestroyPixmap(self, a, b)	\
	(((self)->vtable->DestroyPixmap)(self, IMGCB_DestroyPixmap_op, a, b))

#define IMGCB_DisplayPixmap(self, a, b, c, d, e, f, g, h, i, j, k)	\
	(((self)->vtable->DisplayPixmap)(self, IMGCB_DisplayPixmap_op, a, b, c, d, e, f, g, h, i, j, k))

#define IMGCB_DisplayIcon(self, a, b, c, d)	\
	(((self)->vtable->DisplayIcon)(self, IMGCB_DisplayIcon_op, a, b, c, d))

#define IMGCB_GetIconDimensions(self, a, b, c, d)	\
	(((self)->vtable->GetIconDimensions)(self, IMGCB_GetIconDimensions_op, a, b, c, d))

/*******************************************************************************
 * IMGCB Interface
 ******************************************************************************/

struct netscape_jmc_JMCInterfaceID;
struct java_lang_Object;
struct java_lang_String;
struct netscape_jmc_CType;
#include "il_types.h"
struct netscape_libimg_int_t;

struct IMGCBInterface {
	void*	(*getInterface)(struct IMGCB* self, jint op, const JMCInterfaceID* a, JMCException* *exception);
	void	(*addRef)(struct IMGCB* self, jint op, JMCException* *exception);
	void	(*release)(struct IMGCB* self, jint op, JMCException* *exception);
	jint	(*hashCode)(struct IMGCB* self, jint op, JMCException* *exception);
	jbool	(*equals)(struct IMGCB* self, jint op, void* obj, JMCException* *exception);
	void*	(*clone)(struct IMGCB* self, jint op, JMCException* *exception);
	const char*	(*toString)(struct IMGCB* self, jint op, JMCException* *exception);
	void	(*finalize)(struct IMGCB* self, jint op, JMCException* *exception);
	void	(*NewPixmap)(struct IMGCB* self, jint op, void* a, jint b, jint c, IL_Pixmap* d, IL_Pixmap* e);
	void	(*UpdatePixmap)(struct IMGCB* self, jint op, void* a, IL_Pixmap* b, jint c, jint d, jint e, jint f);
	void	(*ControlPixmapBits)(struct IMGCB* self, jint op, void* a, IL_Pixmap* b, IL_PixmapControl c);
	void	(*DestroyPixmap)(struct IMGCB* self, jint op, void* a, IL_Pixmap* b);
	void	(*DisplayPixmap)(struct IMGCB* self, jint op, void* a, IL_Pixmap* b, IL_Pixmap* c, jint d, jint e, jint f, jint g, jint h, jint i, jint j, jint k);
	void	(*DisplayIcon)(struct IMGCB* self, jint op, void* a, jint b, jint c, jint d);
	void	(*GetIconDimensions)(struct IMGCB* self, jint op, void* a, int* b, int* c, jint d);
};

/*******************************************************************************
 * IMGCB Operation IDs
 ******************************************************************************/

typedef enum IMGCBOperations {
	IMGCB_getInterface_op,
	IMGCB_addRef_op,
	IMGCB_release_op,
	IMGCB_hashCode_op,
	IMGCB_equals_op,
	IMGCB_clone_op,
	IMGCB_toString_op,
	IMGCB_finalize_op,
	IMGCB_NewPixmap_op,
	IMGCB_UpdatePixmap_op,
	IMGCB_ControlPixmapBits_op,
	IMGCB_DestroyPixmap_op,
	IMGCB_DisplayPixmap_op,
	IMGCB_DisplayIcon_op,
	IMGCB_GetIconDimensions_op
} IMGCBOperations;

/*******************************************************************************
 * Writing your C implementation: "IMGCB.h"
 * *****************************************************************************
 * You must create a header file named "IMGCB.h" that implements
 * the struct IMGCBImpl, including the struct IMGCBImplHeader
 * as it's first field:
 * 
 * 		#include "MIMGCB.h" // generated header
 * 
 * 		struct IMGCBImpl {
 * 			IMGCBImplHeader	header;
 * 			<your instance data>
 * 		};
 * 
 * This header file will get included by the generated module implementation.
 ******************************************************************************/

/* Forward reference to the user-defined instance struct: */
typedef struct IMGCBImpl	IMGCBImpl;


/* This struct must be included as the first field of your instance struct: */
typedef struct IMGCBImplHeader {
	const struct IMGCBInterface*	vtableIMGCB;
	jint		refcount;
} IMGCBImplHeader;

/*******************************************************************************
 * Instance Casting Macros
 * These macros get your back to the top of your instance, IMGCB,
 * given a pointer to one of its interfaces.
 ******************************************************************************/

#define IMGCBImpl2IMGCBIF(IMGCBImplPtr) \
	((IMGCBIF*)((char*)(IMGCBImplPtr) + offsetof(IMGCBImplHeader, vtableIMGCB)))

#define IMGCBIF2IMGCBImpl(IMGCBIFPtr) \
	((IMGCBImpl*)((char*)(IMGCBIFPtr) - offsetof(IMGCBImplHeader, vtableIMGCB)))

#define IMGCBImpl2IMGCB(IMGCBImplPtr) \
	((IMGCB*)((char*)(IMGCBImplPtr) + offsetof(IMGCBImplHeader, vtableIMGCB)))

#define IMGCB2IMGCBImpl(IMGCBPtr) \
	((IMGCBImpl*)((char*)(IMGCBPtr) - offsetof(IMGCBImplHeader, vtableIMGCB)))

/*******************************************************************************
 * Operations you must implement
 ******************************************************************************/


extern JMC_PUBLIC_API(void*)
_IMGCB_getBackwardCompatibleInterface(struct IMGCB* self, const JMCInterfaceID* iid,
	JMCException* *exception);

extern JMC_PUBLIC_API(void)
_IMGCB_init(struct IMGCB* self, JMCException* *exception);

extern JMC_PUBLIC_API(void*)
_IMGCB_getInterface(struct IMGCB* self, jint op, const JMCInterfaceID* a, JMCException* *exception);

extern JMC_PUBLIC_API(void)
_IMGCB_addRef(struct IMGCB* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(void)
_IMGCB_release(struct IMGCB* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(jint)
_IMGCB_hashCode(struct IMGCB* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(jbool)
_IMGCB_equals(struct IMGCB* self, jint op, void* obj, JMCException* *exception);

extern JMC_PUBLIC_API(void*)
_IMGCB_clone(struct IMGCB* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(const char*)
_IMGCB_toString(struct IMGCB* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(void)
_IMGCB_finalize(struct IMGCB* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(void)
_IMGCB_NewPixmap(struct IMGCB* self, jint op, void* a, jint b, jint c, IL_Pixmap* d, IL_Pixmap* e);

extern JMC_PUBLIC_API(void)
_IMGCB_UpdatePixmap(struct IMGCB* self, jint op, void* a, IL_Pixmap* b, jint c, jint d, jint e, jint f);

extern JMC_PUBLIC_API(void)
_IMGCB_ControlPixmapBits(struct IMGCB* self, jint op, void* a, IL_Pixmap* b, IL_PixmapControl c);

extern JMC_PUBLIC_API(void)
_IMGCB_DestroyPixmap(struct IMGCB* self, jint op, void* a, IL_Pixmap* b);

extern JMC_PUBLIC_API(void)
_IMGCB_DisplayPixmap(struct IMGCB* self, jint op, void* a, IL_Pixmap* b, IL_Pixmap* c, jint d, jint e, jint f, jint g, jint h, jint i, jint j, jint k);

extern JMC_PUBLIC_API(void)
_IMGCB_DisplayIcon(struct IMGCB* self, jint op, void* a, jint b, jint c, jint d);

extern JMC_PUBLIC_API(void)
_IMGCB_GetIconDimensions(struct IMGCB* self, jint op, void* a, int* b, int* c, jint d);

/*******************************************************************************
 * Factory Operations
 ******************************************************************************/

JMC_PUBLIC_API(IMGCB*)
IMGCBFactory_Create(JMCException* *exception);

/******************************************************************************/

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* _MIMGCB_H_ */
