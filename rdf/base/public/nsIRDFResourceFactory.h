/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

  The RDF resource factory interface. A resource factory produces
  nsIRDFResource objects for a specified URI prefix.

 */

#ifndef nsIRDFResourceFactory_h__
#define nsIRDFResourceFactory_h__

#include "nsISupports.h"
class nsIRDFResource;

// {8CE57A20-A02C-11d2-8EBF-00805F29F370}
#define NS_IRDFRESOURCEFACTORY_IID \
{ 0x8ce57a20, 0xa02c, 0x11d2, { 0x8e, 0xbf, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }


/**
 * A resource factory can be registered with <tt>nsIRDFService</tt> to produce
 * resources with a certain <i>URI prefix</i>. The resource factory will be called
 * upon to create a new resource, which the resource manager will cache.
 *
 * @see nsIRDFService::RegisterResourceFactory
 * @see nsIRDFService::UnRegisterResourceFactory
 */
class nsIRDFResourceFactory : public nsISupports
{
public:
    /**
     * This method is called by the RDF service to create a new
     * resource.
     *
     * NOTE. After constructing a new resource via a call to
     * nsIRDFResourceFactory::CreateResource(), the implementation of
     * the RDF service calls nsIRDFResource::GetValue() on the
     * resulting resource. The resulting <tt>const char*</tt> is used
     * as a key for the resource cache. (The assumption is that the
     * custom resource implementation needs to store this information,
     * anyway.)
     *
     * This has important implications for a custom resource's
     * destructor; namely, that you must call
     * nsIRDFService::UnCacheResource() <b>before</b> releasing the
     * storage for the resource's URI. See
     * nsIRDFService::UnCacheResource() for more information.
     */
    NS_IMETHOD CreateResource(const char* aURI, nsIRDFResource** aResult) = 0;
};


#endif // nsIRDFResourceFactory_h__

