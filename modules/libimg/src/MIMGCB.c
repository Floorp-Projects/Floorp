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
 * netscape/libimg/IMGCB module C stub file
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "xp_mem.h"

/* Include the implementation-specific header: */
#include "PIMGCB.h"

/* Include other interface headers: */

/*******************************************************************************
 * IMGCB Methods
 ******************************************************************************/

#ifndef OVERRIDE_IMGCB_getInterface
JMC_PUBLIC_API(void*)
_IMGCB_getInterface(struct IMGCB* self, jint op, const JMCInterfaceID* iid, JMCException* *exc)
{
	if (memcmp(iid, &IMGCB_ID, sizeof(JMCInterfaceID)) == 0)
		return IMGCBImpl2IMGCB(IMGCB2IMGCBImpl(self));
	return _IMGCB_getBackwardCompatibleInterface(self, iid, exc);
}
#endif

#ifndef OVERRIDE_IMGCB_addRef
JMC_PUBLIC_API(void)
_IMGCB_addRef(struct IMGCB* self, jint op, JMCException* *exc)
{
	IMGCBImplHeader* impl = (IMGCBImplHeader*)IMGCB2IMGCBImpl(self);
	impl->refcount++;
}
#endif

#ifndef OVERRIDE_IMGCB_release
JMC_PUBLIC_API(void)
_IMGCB_release(struct IMGCB* self, jint op, JMCException* *exc)
{
	IMGCBImplHeader* impl = (IMGCBImplHeader*)IMGCB2IMGCBImpl(self);
	if (--impl->refcount == 0) {
		IMGCB_finalize(self, exc);
	}
}
#endif

#ifndef OVERRIDE_IMGCB_hashCode
JMC_PUBLIC_API(jint)
_IMGCB_hashCode(struct IMGCB* self, jint op, JMCException* *exc)
{
	return (jint)self;
}
#endif

#ifndef OVERRIDE_IMGCB_equals
JMC_PUBLIC_API(jbool)
_IMGCB_equals(struct IMGCB* self, jint op, void* obj, JMCException* *exc)
{
	return self == obj;
}
#endif

#ifndef OVERRIDE_IMGCB_clone
JMC_PUBLIC_API(void*)
_IMGCB_clone(struct IMGCB* self, jint op, JMCException* *exc)
{
	IMGCBImpl* impl = IMGCB2IMGCBImpl(self);
	IMGCBImpl* newImpl = (IMGCBImpl*)malloc(sizeof(IMGCBImpl));
	if (newImpl == NULL) return NULL;
	memcpy(newImpl, impl, sizeof(IMGCBImpl));
	((IMGCBImplHeader*)newImpl)->refcount = 1;
	return newImpl;
}
#endif

#ifndef OVERRIDE_IMGCB_toString
JMC_PUBLIC_API(const char*)
_IMGCB_toString(struct IMGCB* self, jint op, JMCException* *exc)
{
	return NULL;
}
#endif

#ifndef OVERRIDE_IMGCB_finalize
JMC_PUBLIC_API(void)
_IMGCB_finalize(struct IMGCB* self, jint op, JMCException* *exc)
{
	/* Override this method and add your own finalization here. */
	XP_FREEIF(self);
}
#endif

/*******************************************************************************
 * Jump Tables
 ******************************************************************************/

const struct IMGCBInterface IMGCBVtable = {
	_IMGCB_getInterface,
	_IMGCB_addRef,
	_IMGCB_release,
	_IMGCB_hashCode,
	_IMGCB_equals,
	_IMGCB_clone,
	_IMGCB_toString,
	_IMGCB_finalize,
	_IMGCB_NewPixmap,
	_IMGCB_UpdatePixmap,
	_IMGCB_ControlPixmapBits,
	_IMGCB_DestroyPixmap,
	_IMGCB_DisplayPixmap,
	_IMGCB_DisplayIcon,
	_IMGCB_GetIconDimensions
};

/*******************************************************************************
 * Factory Operations
 ******************************************************************************/

JMC_PUBLIC_API(IMGCB*)
IMGCBFactory_Create(JMCException* *exception)
{
	IMGCBImplHeader* impl = (IMGCBImplHeader*)XP_NEW_ZAP(IMGCBImpl);
	IMGCB* self;
	if (impl == NULL) {
		JMC_EXCEPTION(exception, JMCEXCEPTION_OUT_OF_MEMORY);
		return NULL;
	}
	self = IMGCBImpl2IMGCB(impl);
	impl->vtableIMGCB = &IMGCBVtable;
	impl->refcount = 1;
	_IMGCB_init(self, exception);
	if (JMC_EXCEPTION_RETURNED(exception)) {
		XP_FREE(impl);
		return NULL;
	}
	return self;
}

