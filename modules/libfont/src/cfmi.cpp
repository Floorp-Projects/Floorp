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
