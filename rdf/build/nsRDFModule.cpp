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
#include "nsCOMPtr.h"
#include "nsIGenericFactory.h"
#include "nsIModule.h"

#include "nsRDFModule.h"
#include "nsIFactory.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentSink.h"
#include "nsIRDFService.h"
#include "nsISupports.h"
#include "nsRDFBaseDataSources.h"
#include "nsRDFBuiltInDataSources.h"
#include "nsIRDFFileSystem.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsILocalStore.h"
#include "nsRDFXMLParser.h"
#include "nsRDFXMLSerializer.h"

//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.

#define MAKE_CTOR(_func,_new,_ifname)                                \
static NS_IMETHODIMP                                                 \
CreateNew##_func(nsISupports* aOuter, REFNSIID aIID, void **aResult) \
{                                                                    \
    if (!aResult) {                                                  \
        return NS_ERROR_INVALID_POINTER;                             \
    }                                                                \
    if (aOuter) {                                                    \
        *aResult = nsnull;                                           \
        return NS_ERROR_NO_AGGREGATION;                              \
    }                                                                \
    nsI##_ifname* inst;                                              \
    nsresult rv = NS_New##_new(&inst);                               \
    if (NS_FAILED(rv)) {                                             \
        *aResult = nsnull;                                           \
        return rv;                                                   \
    }                                                                \
    rv = inst->QueryInterface(aIID, aResult);                        \
    if (NS_FAILED(rv)) {                                             \
        *aResult = nsnull;                                           \
    }                                                                \
    NS_RELEASE(inst);             /* get rid of extra refcnt */      \
    return rv;                                                       \
}

extern nsresult
NS_NewDefaultResource(nsIRDFResource** aResult);

MAKE_CTOR(RDFService,RDFService,RDFService)
MAKE_CTOR(RDFXMLDataSource,RDFXMLDataSource,RDFDataSource)
MAKE_CTOR(RDFFileSystemDataSource,RDFFileSystemDataSource,RDFDataSource)
MAKE_CTOR(RDFCompositeDataSource,RDFCompositeDataSource,RDFCompositeDataSource)
MAKE_CTOR(RDFContainer,RDFContainer,RDFContainer)

MAKE_CTOR(RDFContainerUtils,RDFContainerUtils,RDFContainerUtils)

MAKE_CTOR(RDFContentSink,RDFContentSink,RDFContentSink)
MAKE_CTOR(RDFDefaultResource,DefaultResource,RDFResource)

// The list of components we register
static nsModuleComponentInfo components[] = 
{   // register our build-in datasources:
    { 
     "RDF Composite Data Source", 
     NS_RDFCOMPOSITEDATASOURCE_CID,
     NS_RDF_DATASOURCE_CONTRACTID_PREFIX "composite-datasource", 
     CreateNewRDFCompositeDataSource
    },

    { 
     "RDF File System Data Source", 
     NS_RDFFILESYSTEMDATASOURCE_CID,
     NS_RDF_DATASOURCE_CONTRACTID_PREFIX "files", 
     CreateNewRDFFileSystemDataSource
    },
    
    { "RDF In-Memory Data Source", 
      NS_RDFINMEMORYDATASOURCE_CID,
      NS_RDF_DATASOURCE_CONTRACTID_PREFIX "in-memory-datasource", 
      NS_NewRDFInMemoryDataSource
    },

    { "RDF XML Data Source", 
      NS_RDFXMLDATASOURCE_CID,
      NS_RDF_DATASOURCE_CONTRACTID_PREFIX "xml-datasource", 
      CreateNewRDFXMLDataSource
    },

    // register our built-in resource factories:
    { "RDF Default Resource Factory", 
      NS_RDFDEFAULTRESOURCE_CID,
      // Note: default resource factory has no name= part
      NS_RDF_RESOURCE_FACTORY_CONTRACTID, 
      CreateNewRDFDefaultResource
    },

    // register all the other rdf components:
    { "RDF Content Sink", 
      NS_RDFCONTENTSINK_CID,
      NS_RDF_CONTRACTID "/content-sink;1", 
      CreateNewRDFContentSink
    },
    
    { "RDF Container", 
      NS_RDFCONTAINER_CID,
      NS_RDF_CONTRACTID "/container;1", 
      CreateNewRDFContainer
    },
    
    { "RDF Container Utilities", 
      NS_RDFCONTAINERUTILS_CID,
      NS_RDF_CONTRACTID "/container-utils;1", 
      CreateNewRDFContainerUtils
    },
    
    { "RDF Service", 
      NS_RDFSERVICE_CID,
      NS_RDF_CONTRACTID "/rdf-service;1",
      CreateNewRDFService 
    },

    { "RDF/XML Parser",
      NS_RDFXMLPARSER_CID,
      NS_RDF_CONTRACTID "/xml-parser;1",
      nsRDFXMLParser::Create
    },

    { "RDF/XML Serializer",
      NS_RDFXMLSERIALIZER_CID,
      NS_RDF_CONTRACTID "/xml-serializer;1",
      nsRDFXMLSerializer::Create
    },

    // XXX move this to XPFE at some point.
    { "Local Store", NS_LOCALSTORE_CID,
      NS_LOCALSTORE_CONTRACTID, NS_NewLocalStore },
};

NS_IMPL_NSGETMODULE(nsRDFModule, components);
