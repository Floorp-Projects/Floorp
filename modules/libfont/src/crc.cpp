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
 * crc.cpp
 *
 * Implements a link between the (crc) CRenderingContext Interface
 * implementation and its C++ implementation viz (rc) RenderingContextObject.
 *
 * dp Suresh <dp@netscape.com>
 */

#include "libfont.h"

#include "Mcrc.h"
#include "Pcrc.h"
#include "rc.h"

/****************************************************************************
 *				 Implementation of common interface methods					*
 ****************************************************************************/

#ifdef OVERRIDE_crc_getInterface
#include "Mnfrc.h"

extern "C" JMC_PUBLIC_API(void*)
/* ARGSUSED */
_crc_getInterface(struct crc* self, jint op, const JMCInterfaceID* iid, JMCException* *exc)
{
	if (memcmp(iid, &nfrc_ID, sizeof(JMCInterfaceID)) == 0)
		return crcImpl2crc(crc2crcImpl(self));
	return _crc_getBackwardCompatibleInterface(self, iid, exc);
}
#endif /* OVERRIDE_crc_getInterface */

extern "C" JMC_PUBLIC_API(void*)
/*ARGSUSED*/
_crc_getBackwardCompatibleInterface(struct crc* self,
									const JMCInterfaceID* iid,
									struct JMCException* *exceptionThrown)
{
	return(NULL);
}

extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_crc_init(struct crc* self, struct JMCException* *exceptionThrown,
		  jint majorType, jint minorType, void** args, jsize args_length)
{
	crcImpl *oimpl = crc2crcImpl(self);
	RenderingContextObject *rcob = (RenderingContextObject *)oimpl->object;
	rcob = new RenderingContextObject(majorType, minorType, args, args_length);
	if (rcob == NULL)
	  {
		JMC_EXCEPTION(exceptionThrown, JMCEXCEPTION_OUT_OF_MEMORY);
	  }
	else
	  {
		oimpl->object = rcob;
	  }
}

#ifdef OVERRIDE_crc_finalize
extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_crc_finalize(struct crc* self, jint op, JMCException* *exception)
{
	struct crcImpl *oimpl = crc2crcImpl(self);
	RenderingContextObject *rcob = (RenderingContextObject *)oimpl->object;
	delete rcob;
	
	/* Finally, free the memory for the object containter. */
	XP_FREEIF(self);
}
#endif /* OVERRIDE_crc_finalize */


/****************************************************************************
 *				 Implementation of Object specific methods					*
 ****************************************************************************/

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_crc_GetMajorType(struct crc* self, jint op,
				  struct JMCException* *exceptionThrown)
{
	crcImpl *oimpl = crc2crcImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	RenderingContextObject *rcob = (RenderingContextObject *)oimpl->object;
	return(rcob->GetMajorType());
}

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_crc_GetMinorType(struct crc* self, jint op,
				  struct JMCException* *exceptionThrown)
{
	crcImpl *oimpl = crc2crcImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	RenderingContextObject *rcob = (RenderingContextObject *)oimpl->object;
	return(rcob->GetMinorType());
}

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_crc_IsEquivalent(struct crc* self, jint op, jint majorType, jint minorType,
				  struct JMCException* *exceptionThrown)
{
	crcImpl *oimpl = crc2crcImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	RenderingContextObject *rcob = (RenderingContextObject *)oimpl->object;
	return(rcob->IsEquivalent(majorType, minorType));
}

extern "C" JMC_PUBLIC_API(struct rc_data)
/*ARGSUSED*/
_crc_GetPlatformData(struct crc* self, jint op,
					 struct JMCException* *exceptionThrown)
{
	crcImpl *oimpl = crc2crcImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	RenderingContextObject *rcob = (RenderingContextObject *)oimpl->object;
	return(rcob->GetPlatformData());
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_crc_SetPlatformData(struct crc* self, jint op, struct rc_data *newRcData,
					 struct JMCException* *exceptionThrown)
{
	crcImpl *oimpl = crc2crcImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	RenderingContextObject *rcob = (RenderingContextObject *)oimpl->object;
	return(rcob->SetPlatformData(newRcData));
}

#ifndef NO_PERFORMANCE_HACK
NF_PUBLIC_API_IMPLEMENT(struct rc_data *)
/*ARGSUSED*/
NF_GetRCNativeData(struct nfrc* rc)
{
	crcImpl *oimpl = nfrc2crcImpl(rc);
	RenderingContextObject *rcob = (RenderingContextObject *)oimpl->object;
	return (&rcob->rcData);
}
#endif /* NO_PERFORMANCE_HACK */
