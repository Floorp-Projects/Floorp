/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
