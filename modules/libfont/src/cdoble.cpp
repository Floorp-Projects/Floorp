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
 * cdoble.cpp (CDoneObservable.cpp)
 *
 * Implements a link between the (cdoble) CDoneObservable Interface and
 * its C++ implementation viz (doble) DoneObservableObject.
 *
 * dp Suresh <dp@netscape.com>
 */


// The C implementation definition of the interface.
// This links it to the C++ implementation
#include "cdoble.h"

#include "doble.h"		// The actual C++ object implementation

/****************************************************************************
 *				 Implementation of common interface methods					*
 ****************************************************************************/


/****************************************************************************
 *				 Implementation of Object specific methods					*
 ****************************************************************************/

extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cdoble_addObserver(cdoble* o, jint op,
					struct nfdoer *observer,
					struct JMCException * *exceptionThrown)
{
	cdobleImpl *oimpl = cdoble2cdobleImpl(o);
    DoneObservableObject* dob = (DoneObservableObject *)oimpl->object;
    dob->addObserver(observer);
}

extern "C" JMC_PUBLIC_API(jint)
/*ARGSUSED*/
_cdoble_countObservers(cdoble* o, jint op,
					   struct JMCException * *exceptionThrown)
{
	cdobleImpl *oimpl = cdoble2cdobleImpl(o);
    DoneObservableObject* dob = (DoneObservableObject *)oimpl->object;
    return(dob->countObservers());
}

extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cdoble_deleteObserver(cdoble* o, jint op,
					   struct nfdoer *observer,
					   struct JMCException * *exceptionThrown)
{
	cdobleImpl *oimpl = cdoble2cdobleImpl(o);
    DoneObservableObject* dob = (DoneObservableObject *)oimpl->object;
    dob->deleteObserver(observer);
}

extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cdoble_deleteObservers(cdoble* o, jint op,
						struct JMCException * *exceptionThrown)
{
	cdobleImpl *oimpl = cdoble2cdobleImpl(o);
    DoneObservableObject* dob = (DoneObservableObject *)oimpl->object;
    dob->deleteObservers();
}

extern "C" JMC_PUBLIC_API(void)
/*ARGSUSED*/
_cdoble_notifyObservers(cdoble* o, jint op, void *arg,
						struct JMCException * *exceptionThrown)
{
	cdobleImpl *oimpl = cdoble2cdobleImpl(o);
    DoneObservableObject* dob = (DoneObservableObject *)oimpl->object;
    dob->notifyObservers(o, arg);
}

