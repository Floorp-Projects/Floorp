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
 * netscape/libimg/IMGCBIF module C header file
 ******************************************************************************/

#ifndef _MIMGCBIF_H_
#define _MIMGCBIF_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "jmc.h"

/*******************************************************************************
 * IMGCBIF
 ******************************************************************************/

/* The type of the IMGCBIF interface. */
struct IMGCBIFInterface;

/* The public type of a IMGCBIF instance. */
typedef struct IMGCBIF {
	const struct IMGCBIFInterface*	vtable;
} IMGCBIF;

/* The inteface ID of the IMGCBIF interface. */
#ifndef JMC_INIT_IMGCBIF_ID
extern EXTERN_C_WITHOUT_EXTERN const JMCInterfaceID IMGCBIF_ID;
#else
EXTERN_C const JMCInterfaceID IMGCBIF_ID = { 0x38775d44, 0x23525963, 0x7f763562, 0x625c5b2a };
#endif /* JMC_INIT_IMGCBIF_ID */
/*******************************************************************************
 * IMGCBIF Operations
 ******************************************************************************/

#define IMGCBIF_getInterface(self, a, exception)	\
	(((self)->vtable->getInterface)(self, IMGCBIF_getInterface_op, a, exception))

#define IMGCBIF_addRef(self, exception)	\
	(((self)->vtable->addRef)(self, IMGCBIF_addRef_op, exception))

#define IMGCBIF_release(self, exception)	\
	(((self)->vtable->release)(self, IMGCBIF_release_op, exception))

#define IMGCBIF_hashCode(self, exception)	\
	(((self)->vtable->hashCode)(self, IMGCBIF_hashCode_op, exception))

#define IMGCBIF_equals(self, obj, exception)	\
	(((self)->vtable->equals)(self, IMGCBIF_equals_op, obj, exception))

#define IMGCBIF_clone(self, exception)	\
	(((self)->vtable->clone)(self, IMGCBIF_clone_op, exception))

#define IMGCBIF_toString(self, exception)	\
	(((self)->vtable->toString)(self, IMGCBIF_toString_op, exception))

#define IMGCBIF_finalize(self, exception)	\
	(((self)->vtable->finalize)(self, IMGCBIF_finalize_op, exception))

#define IMGCBIF_NewPixmap(self, a, b, c, d, e)	\
	(((self)->vtable->NewPixmap)(self, IMGCBIF_NewPixmap_op, a, b, c, d, e))

#define IMGCBIF_UpdatePixmap(self, a, b, c, d, e, f)	\
	(((self)->vtable->UpdatePixmap)(self, IMGCBIF_UpdatePixmap_op, a, b, c, d, e, f))

#define IMGCBIF_ControlPixmapBits(self, a, b, c)	\
	(((self)->vtable->ControlPixmapBits)(self, IMGCBIF_ControlPixmapBits_op, a, b, c))

#define IMGCBIF_DestroyPixmap(self, a, b)	\
	(((self)->vtable->DestroyPixmap)(self, IMGCBIF_DestroyPixmap_op, a, b))

#define IMGCBIF_DisplayPixmap(self, a, b, c, d, e, f, g, h, i, j, k)	\
	(((self)->vtable->DisplayPixmap)(self, IMGCBIF_DisplayPixmap_op, a, b, c, d, e, f, g, h, i, j, k))

#define IMGCBIF_DisplayIcon(self, a, b, c, d)	\
	(((self)->vtable->DisplayIcon)(self, IMGCBIF_DisplayIcon_op, a, b, c, d))

#define IMGCBIF_GetIconDimensions(self, a, b, c, d)	\
	(((self)->vtable->GetIconDimensions)(self, IMGCBIF_GetIconDimensions_op, a, b, c, d))

/*******************************************************************************
 * IMGCBIF Interface
 ******************************************************************************/

struct netscape_jmc_JMCInterfaceID;
struct java_lang_Object;
struct java_lang_String;
struct netscape_jmc_CType;
#include "il_types.h"
struct netscape_libimg_int_t;

struct IMGCBIFInterface {
	void*	(*getInterface)(struct IMGCBIF* self, jint op, const JMCInterfaceID* a, JMCException* *exception);
	void	(*addRef)(struct IMGCBIF* self, jint op, JMCException* *exception);
	void	(*release)(struct IMGCBIF* self, jint op, JMCException* *exception);
	jint	(*hashCode)(struct IMGCBIF* self, jint op, JMCException* *exception);
	jbool	(*equals)(struct IMGCBIF* self, jint op, void* obj, JMCException* *exception);
	void*	(*clone)(struct IMGCBIF* self, jint op, JMCException* *exception);
	const char*	(*toString)(struct IMGCBIF* self, jint op, JMCException* *exception);
	void	(*finalize)(struct IMGCBIF* self, jint op, JMCException* *exception);
	void	(*NewPixmap)(struct IMGCBIF* self, jint op, void* a, jint b, jint c, IL_Pixmap* d, IL_Pixmap* e);
	void	(*UpdatePixmap)(struct IMGCBIF* self, jint op, void* a, IL_Pixmap* b, jint c, jint d, jint e, jint f);
	void	(*ControlPixmapBits)(struct IMGCBIF* self, jint op, void* a, IL_Pixmap* b, IL_PixmapControl c);
	void	(*DestroyPixmap)(struct IMGCBIF* self, jint op, void* a, IL_Pixmap* b);
	void	(*DisplayPixmap)(struct IMGCBIF* self, jint op, void* a, IL_Pixmap* b, IL_Pixmap* c, jint d, jint e, jint f, jint g, jint h, jint i, jint j, jint k);
	void	(*DisplayIcon)(struct IMGCBIF* self, jint op, void* a, jint b, jint c, jint d);
	void	(*GetIconDimensions)(struct IMGCBIF* self, jint op, void* a, int* b, int* c, jint d);
};

/*******************************************************************************
 * IMGCBIF Operation IDs
 ******************************************************************************/

typedef enum IMGCBIFOperations {
	IMGCBIF_getInterface_op,
	IMGCBIF_addRef_op,
	IMGCBIF_release_op,
	IMGCBIF_hashCode_op,
	IMGCBIF_equals_op,
	IMGCBIF_clone_op,
	IMGCBIF_toString_op,
	IMGCBIF_finalize_op,
	IMGCBIF_NewPixmap_op,
	IMGCBIF_UpdatePixmap_op,
	IMGCBIF_ControlPixmapBits_op,
	IMGCBIF_DestroyPixmap_op,
	IMGCBIF_DisplayPixmap_op,
	IMGCBIF_DisplayIcon_op,
	IMGCBIF_GetIconDimensions_op
} IMGCBIFOperations;

/*******************************************************************************
 * Writing your C implementation: "IMGCBIF.h"
 * *****************************************************************************
 * You must create a header file named "IMGCBIF.h" that implements
 * the struct IMGCBIFImpl, including the struct IMGCBIFImplHeader
 * as it's first field:
 * 
 * 		#include "MIMGCBIF.h" // generated header
 * 
 * 		struct IMGCBIFImpl {
 * 			IMGCBIFImplHeader	header;
 * 			<your instance data>
 * 		};
 * 
 * This header file will get included by the generated module implementation.
 ******************************************************************************/

/* Forward reference to the user-defined instance struct: */
typedef struct IMGCBIFImpl	IMGCBIFImpl;


/* This struct must be included as the first field of your instance struct: */
typedef struct IMGCBIFImplHeader {
	const struct IMGCBIFInterface*	vtableIMGCBIF;
	jint		refcount;
} IMGCBIFImplHeader;

/*******************************************************************************
 * Instance Casting Macros
 * These macros get your back to the top of your instance, IMGCBIF,
 * given a pointer to one of its interfaces.
 ******************************************************************************/

#define IMGCBIFImpl2Object(IMGCBIFImplPtr) \
	((Object*)((char*)(IMGCBIFImplPtr) + offsetof(IMGCBIFImplHeader, vtableIMGCBIF)))

#define Object2IMGCBIFImpl(ObjectPtr) \
	((IMGCBIFImpl*)((char*)(ObjectPtr) - offsetof(IMGCBIFImplHeader, vtableIMGCBIF)))

#define IMGCBIFImpl2IMGCBIF(IMGCBIFImplPtr) \
	((IMGCBIF*)((char*)(IMGCBIFImplPtr) + offsetof(IMGCBIFImplHeader, vtableIMGCBIF)))

#define IMGCBIF2IMGCBIFImpl(IMGCBIFPtr) \
	((IMGCBIFImpl*)((char*)(IMGCBIFPtr) - offsetof(IMGCBIFImplHeader, vtableIMGCBIF)))

/*******************************************************************************
 * Operations you must implement
 ******************************************************************************/


extern JMC_PUBLIC_API(void*)
_IMGCBIF_getBackwardCompatibleInterface(struct IMGCBIF* self, const JMCInterfaceID* iid,
	JMCException* *exception);

extern JMC_PUBLIC_API(void*)
_IMGCBIF_getInterface(struct IMGCBIF* self, jint op, const JMCInterfaceID* a, JMCException* *exception);

extern JMC_PUBLIC_API(void)
_IMGCBIF_addRef(struct IMGCBIF* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(void)
_IMGCBIF_release(struct IMGCBIF* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(jint)
_IMGCBIF_hashCode(struct IMGCBIF* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(jbool)
_IMGCBIF_equals(struct IMGCBIF* self, jint op, void* obj, JMCException* *exception);

extern JMC_PUBLIC_API(void*)
_IMGCBIF_clone(struct IMGCBIF* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(const char*)
_IMGCBIF_toString(struct IMGCBIF* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(void)
_IMGCBIF_finalize(struct IMGCBIF* self, jint op, JMCException* *exception);

extern JMC_PUBLIC_API(void)
_IMGCBIF_NewPixmap(struct IMGCBIF* self, jint op, void* a, jint b, jint c, IL_Pixmap* d, IL_Pixmap* e);

extern JMC_PUBLIC_API(void)
_IMGCBIF_UpdatePixmap(struct IMGCBIF* self, jint op, void* a, IL_Pixmap* b, jint c, jint d, jint e, jint f);

extern JMC_PUBLIC_API(void)
_IMGCBIF_ControlPixmapBits(struct IMGCBIF* self, jint op, void* a, IL_Pixmap* b, IL_PixmapControl c);

extern JMC_PUBLIC_API(void)
_IMGCBIF_DestroyPixmap(struct IMGCBIF* self, jint op, void* a, IL_Pixmap* b);

extern JMC_PUBLIC_API(void)
_IMGCBIF_DisplayPixmap(struct IMGCBIF* self, jint op, void* a, IL_Pixmap* b, IL_Pixmap* c, jint d, jint e, jint f, jint g, jint h, jint i, jint j, jint k);

extern JMC_PUBLIC_API(void)
_IMGCBIF_DisplayIcon(struct IMGCBIF* self, jint op, void* a, jint b, jint c, jint d);

extern JMC_PUBLIC_API(void)
_IMGCBIF_GetIconDimensions(struct IMGCBIF* self, jint op, void* a, int* b, int* c, jint d);

/*******************************************************************************
 * Factory Operations
 ******************************************************************************/

/******************************************************************************/

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* _MIMGCBIF_H_ */
