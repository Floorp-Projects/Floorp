/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsRDFDOMDataSource.h"
#include "nsRDFDOMResourceFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "rdf.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsRDFDOMDataSource);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRDFDOMViewerElement);

static nsModuleComponentInfo gRDFDOMViewerModuleComponents[] =
{
    { "DOM Data Source", 
      NS_RDF_DOMDATASOURCE_CID,
      NS_RDF_DATASOURCE_CONTRACTID_PREFIX "domds",
      nsRDFDOMDataSourceConstructor },
    { "DOM Resource Factory",
      NS_RDF_DOMRESOURCEFACTORY_CID,
      NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX "dom",
      nsRDFDOMViewerElementConstructor }
};

NS_IMPL_NSGETMODULE("nsRDFDOMViewerModule", gRDFDOMViewerModuleComponents)
