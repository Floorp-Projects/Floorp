/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
 *
 * Implements a link between the (cf) CFont Interface
 * implementation and its C++ implementation viz (f) FontObject.
 *
 * dp Suresh <dp@netscape.com>
 */


#include "libfont.h"

// The C implementation definition of the interface.
// This links it to the C++ implementation
#include "Pcf.h"

// The actual C++ object
#include "f.h"

// Uses:	The nff interface
#include "Mnff.h"

// The finalize methods removes the font from the fontbrokers
// font object cache. Hence it needs to access the fontbroker.
#include "Pcfb.h"
#include "fb.h"

/****************************************************************************
 *				 Implementation of common interface methods					*
 ****************************************************************************/
#ifdef OVERRIDE_cf_getInterface
extern "C" JMC_PUBLIC_API(void*)
/*ARGSUSED*/
_cf_getInterface(struct cf* self, jint op, const JMCInterfaceID* iid, JMCException* *exc)
{
	if (memcmp(iid, &nff_ID, sizeof(JMCInterfaceID)) == 0)
		return cfImpl2cf(cf2cfImpl(self));
	return _cf_getBackwardCompatibleInterface(self, iid, exc);
}
#endif /* OVERRIDE_cf_getInterface */

extern "C" JMC_PUBLIC_API(void*)
/*ARGSUSED*/
_cf_getBackwardCompatibleInterface(struct cf* self, const JMCInterfaceID* iid,
								   struct JMCException* *exceptionThrown)
{
	return(NULL);
}

extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cf_init(struct cf* self, struct JMCException* *exceptionThrown,
		 struct nfrc *rc)
{
	cfImpl *oimpl = cf2cfImpl(self);
	FontObject *fobj = new FontObject((struct nff *)self, rc);
	if (fobj == NULL)
	  {
		JMC_EXCEPTION(exceptionThrown, JMCEXCEPTION_OUT_OF_MEMORY);
	  }
	else
	  {
		oimpl->object = fobj;
	  }
	return;
}


#ifdef OVERRIDE_cf_finalize
extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cf_finalize(struct cf* self, jint op, JMCException* *exception)
{
	/* Release this font object from the fontObjectCache in the Broker */
	cfbImpl *fbimpl = cfb2cfbImpl(WF_fbc);
	FontBrokerObject *fbobj = (FontBrokerObject *)fbimpl->object;

	WF_TRACEMSG( ("NF: Deleting font object [0x%x].", fbobj) );
	fbobj->fontObjectCache.releaseFont((struct nff *)self);
	
	/* Delete the font object */
	struct cfImpl *oimpl = cf2cfImpl(self);
	FontObject *fob = (FontObject *)oimpl->object;
	delete fob;
	
	/* Finally, free the memory for the object containter. */
	XP_FREEIF(self);
}
#endif /* OVERRIDE_cf_finalize */


#ifdef OVERRIDE_cf_release
JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cf_release(struct cf* self, jint op, JMCException* *exc)
{
	cfImplHeader* impl = (cfImplHeader*)cf2cfImpl(self);
	--impl->refcount;

	// Run the GC
	cfImpl *oimpl = cf2cfImpl(self);
	FontObject *fob = (FontObject *)oimpl->object;
	int ret = fob->GC();
	
	if (impl->refcount == 0)
	{
		cf_finalize(self, exc);
	}
}
#endif


/****************************************************************************
 *				 Implementation of Object specific methods					*
 ****************************************************************************/

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cf_GetState(struct cf* self, jint op, struct JMCException* *exceptionThrown)
{
	cfImpl *oimpl = cf2cfImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontObject *fob = (FontObject *)oimpl->object;
	return((jint)fob->GetState());
}


extern "C" JMC_PUBLIC_API(void*)
/*ARGSUSED*/
_cf_EnumerateSizes(struct cf* self, jint op, struct nfrc *rc,
				   struct JMCException* *exceptionThrown)
{
	cfImpl *oimpl = cf2cfImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontObject *fob = (FontObject *)oimpl->object;
	return(fob->EnumerateSizes(rc));
}


extern "C" JMC_PUBLIC_API(struct nfrf*)
/*ARGSUSED*/
_cf_GetRenderableFont(struct cf* self, jint op, struct nfrc *rc,
					  jdouble pointsize, struct JMCException* *exceptionThrown)
{
	cfImpl *oimpl = cf2cfImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontObject *fob = (FontObject *)oimpl->object;
	return(fob->GetRenderableFont(rc, pointsize));
}


extern "C" JMC_PUBLIC_API(struct nffmi*)
/*ARGSUSED*/
_cf_GetMatchInfo(struct cf* self, jint op, struct nfrc *rc,
				 jint pointsize, struct JMCException* *exceptionThrown)
{
	cfImpl *oimpl = cf2cfImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontObject *fob = (FontObject *)oimpl->object;
	return(fob->GetMatchInfo(rc, pointsize));
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cf_GetRcMajorType(struct cf* self, jint op,
				   struct JMCException* *exceptionThrown)
{
	cfImpl *oimpl = cf2cfImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontObject *fob = (FontObject *)oimpl->object;
	return(fob->GetRcMajorType());
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cf_GetRcMinorType(struct cf* self, jint op,
				   struct JMCException* *exceptionThrown)
{
	cfImpl *oimpl = cf2cfImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontObject *fob = (FontObject *)oimpl->object;
	return(fob->GetRcMinorType());
}
