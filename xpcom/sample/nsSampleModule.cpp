/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *	Suresh Duddu <dp@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIGenericFactory.h"

#include "nsSample.h"

////////////////////////////////////////////////////////////////////////
// NOTE this file supercedes nsSampleFactory.cpp.  nsSampleFactory has
// extensive comments, but it has been CVS removed to reduce clutter
// in this sample.  It's available to the interested via CVS history:
// cvs up nsSampleFactory.cpp -r 1.10
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// With the below sample, you can define an implementation glue
// that talks with xpcom for creation of component nsSampleImpl
// that implement the interface nsISample. This can be extended for
// any number of components.
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the object nsSampleImpl
//
// What this does is defines a function nsSampleImplConstructor which we
// will specific in the nsModuleComponentInfo table. This function will
// be used by the generic factory to create an instance of nsSampleImpl.
//
// NOTE: This creates an instance of nsSampleImpl by using the default
//		 constructor nsSampleImpl::nsSampleImpl()
//
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSampleImpl)

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
// The Registration and Unregistration proc are optional in the structure.
//
static NS_METHOD nsSampleRegistrationProc(nsIComponentManager *aCompMgr,
                                          nsIFile *aPath,
                                          const char *registryLocation,
                                          const char *componentType,
                                          const nsModuleComponentInfo *info)
{
    // Do any registration specific activity like adding yourself to a
    // category. The Generic Module will take care of registering your
    // component with xpcom. You dont need to do that. Only any component
    // specific additional activity needs to be done here.

    // This functions is optional. If you dont need it, dont add it to the structure.

    return NS_OK;
}

static NS_METHOD nsSampleUnregistrationProc(nsIComponentManager *aCompMgr,
                                            nsIFile *aPath,
                                            const char *registryLocation,
                                            const nsModuleComponentInfo *info)
{
    // Undo any component specific registration like adding yourself to a
    // category here. The Generic Module will take care of unregistering your
    // component from xpcom. You dont need to do that. Only any component
    // specific additional activity needs to be done here.

    // This functions is optional. If you dont need it, dont add it to the structure.

    // Return value is not used from this function.
    return NS_OK;
}

// For each class that wishes to support nsIClassInfo, add a line like this
NS_DECL_CLASSINFO(nsSampleImpl)

static nsModuleComponentInfo components[] =
{
  { "Sample Component", NS_SAMPLE_CID, NS_SAMPLE_CONTRACTID, nsSampleImplConstructor,
    nsSampleRegistrationProc /* NULL if you dont need one */,
    nsSampleUnregistrationProc /* NULL if you dont need one */,
    NULL /* no factory destructor */,
    NS_CI_INTERFACE_GETTER_NAME(nsSampleImpl),
    NULL /* no language helper */,
    &NS_CLASSINFO_NAME(nsSampleImpl)
  }
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
// NOTE: If you want to use the module shutdown to release any
//		module specific resources, use the macro
//		NS_IMPL_NSGETMODULE_WITH_DTOR() instead of the vanilla
//		NS_IMPL_NSGETMODULE()
//
NS_IMPL_NSGETMODULE(nsSampleModule, components)
