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
 * Implements a link between the (cfmi) CFontMatchInfo Interface
 * implementation and its C++ implementation viz (fmi) FontMatchInfoObject.
 *
 * dp Suresh <dp@netscape.com>
 */


#include "libfont.h"

// The C implementation definition of the interface.
// This links it to the C++ implementation
#include "Pcfmi.h"

// The actual C++ object
#include "fmi.h"

// Uses: The nffmi interface
#include "Mnffmi.h"

/****************************************************************************
 *				 Implementation of common interface methods					*
 ****************************************************************************/

#ifdef OVERRIDE_cfmi_getInterface
extern "C" JMC_PUBLIC_API(void*)
/*ARGSUSED*/
_cfmi_getInterface(struct cfmi* self, jint op, const JMCInterfaceID* iid, JMCException* *exc)
{
	if (memcmp(iid, &nffmi_ID, sizeof(JMCInterfaceID)) == 0)
		return cfmiImpl2cfmi(cfmi2cfmiImpl(self));
	return _cfmi_getBackwardCompatibleInterface(self, iid, exc);
}
#endif /* OVERRIDE_cfmi_getInterface */

extern "C" JMC_PUBLIC_API(void*)
/*ARGSUSED*/
_cfmi_getBackwardCompatibleInterface(struct cfmi* self,
									 const JMCInterfaceID* iid,
									 struct JMCException* *exceptionThrown)
{
	return(NULL);
}

extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cfmi_init(struct cfmi* self, struct JMCException* *exceptionThrown,
		   const char* name, const char* charset, const char* encoding,
		   jint weight, jint pitch, jint style, jint underline, jint strikeOut,
		   jint resolutionX, jint resolutionY)
{
	cfmiImpl *oimpl = cfmi2cfmiImpl(self);
	FontMatchInfoObject *fmiob = (FontMatchInfoObject *)oimpl->object;
	fmiob = new FontMatchInfoObject(name, charset, encoding, weight,
									pitch, style, underline, strikeOut,
									resolutionX, resolutionY);
	if (fmiob == NULL)
	  {
		JMC_EXCEPTION(exceptionThrown, JMCEXCEPTION_OUT_OF_MEMORY);
	  }
	else
	  {
		oimpl->object = fmiob;
	  }
}


#ifdef OVERRIDE_cfmi_finalize
extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cfmi_finalize(struct cfmi* self, jint op, JMCException* *exception)
{
	struct cfmiImpl *oimpl = cfmi2cfmiImpl(self);
	FontMatchInfoObject *fmiob = (FontMatchInfoObject *)oimpl->object;
	delete fmiob;
	
	/* Finally, free the memory for the object containter. */
	XP_FREEIF(self);
}
#endif /* OVERRIDE_cfmi_finalize */

#ifdef OVERRIDE_cfmi_toString
JMC_PUBLIC_API(const char*)
/*ARGSUSED*/
_cfmi_toString(struct cfmi* self, jint op, JMCException* *exc)
{
	cfmiImpl *oimpl = cfmi2cfmiImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontMatchInfoObject *fmiob = (FontMatchInfoObject *)oimpl->object;
	return(fmiob->describe());
}
#endif

#ifdef OVERRIDE_cfmi_equals
JMC_PUBLIC_API(jbool)
/*ARGSUSED*/
_cfmi_equals(struct cfmi* self, jint op, void* obj, JMCException* *exc)
{
	cfmiImpl *oimpl = cfmi2cfmiImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontMatchInfoObject *fmiob = (FontMatchInfoObject *)oimpl->object;
	return(fmiob->IsEquivalent((struct nffmi *)obj) > 0);
}
#endif


/****************************************************************************
 *				 Implementation of Object specific methods					*
 ****************************************************************************/

extern "C" JMC_PUBLIC_API(void *)
/*ARGSUSED*/
_cfmi_GetValue(struct cfmi* self, jint op, const char *attribute,
			   struct JMCException* *exceptionThrown)
{
	cfmiImpl *oimpl = cfmi2cfmiImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontMatchInfoObject *fmiob = (FontMatchInfoObject *)oimpl->object;
	return(fmiob->GetValue(attribute));
}


extern "C" JMC_PUBLIC_API(void *)
/*ARGSUSED*/
_cfmi_ListAttributes(struct cfmi* self, jint op,
					 struct JMCException* *exceptionThrown)
{
	cfmiImpl *oimpl = cfmi2cfmiImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontMatchInfoObject *fmiob = (FontMatchInfoObject *)oimpl->object;
	return((void *)fmiob->ListAttributes());
}


extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cfmi_IsEquivalent(struct cfmi* self, jint op, struct nffmi *fmi,
				   struct JMCException* *exceptionThrown)
{
	cfmiImpl *oimpl = cfmi2cfmiImpl(self);
	XP_ASSERT(oimpl->header.refcount);
	FontMatchInfoObject *fmiob = (FontMatchInfoObject *)oimpl->object;
	return(fmiob->IsEquivalent(fmi));
}
