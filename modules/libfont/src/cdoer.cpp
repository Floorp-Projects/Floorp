/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
 *
 * Implements the Observer
 *
 * dp Suresh <dp@netscape.com>
 */


#include "libfont.h"

#include "Pcdoer.h"
#include "nf.h"
#include "coremem.h"

/****************************************************************************
 *				 Implementation of common interface methods
 ****************************************************************************/

#ifdef OVERRIDE_cdoer_getInterface
#include "Mnfdoer.h"

extern "C" JMC_PUBLIC_API(void*)
/* ARGSUSED */
_cdoer_getInterface(struct cdoer* self, jint op, const JMCInterfaceID* iid, JMCException* *exc)
{
	if (memcmp(iid, &nfdoer_ID, sizeof(JMCInterfaceID)) == 0)
		return cdoerImpl2cdoer(cdoer2cdoerImpl(self));
	return _cdoer_getBackwardCompatibleInterface(self, iid, exc);
}
#endif /* OVERRIDE_cdoer_getInterface */

extern "C" JMC_PUBLIC_API(void*)
/* ARGSUSED */
_cdoer_getBackwardCompatibleInterface(struct cdoer* self,
									const JMCInterfaceID* iid,
									struct JMCException* *exceptionThrown)
{
	return (NULL);
}

extern "C" JMC_PUBLIC_API(void)
/* ARGSUSED */
_cdoer_init(struct cdoer* self, JMCException* *exception,
			nfFontObserverCallback callback, void *client_data)

{
	struct cdoerImpl *oimpl = cdoer2cdoerImpl(self);
	oimpl->update_callback = callback;
	oimpl->client_data = client_data;
}

#ifdef OVERRIDE_cdoer_finalize
extern "C" JMC_PUBLIC_API(void)
/* ARGSUSED */
_cdoer_finalize(struct cdoer* self, jint op, JMCException* *exc)
{
	/* Finally, free the memory for the object containter. */
	XP_FREEIF(self);
}
#endif /* OVERRIDE_cdoer_finalize */



/****************************************************************************
 *				 Implementation of Object specific methods
 ****************************************************************************/


extern "C" JMC_PUBLIC_API(void)
/* ARGSUSED */
_cdoer_Update(struct cdoer* self, jint op, struct nff *f,
			  JMCException* *exception)
{
	struct cdoerImpl *oimpl = cdoer2cdoerImpl(self);
	if (oimpl->update_callback)
	{
		(*oimpl->update_callback)(f, oimpl->client_data);
	}
}

