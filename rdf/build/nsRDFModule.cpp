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
#include "nsIGenericFactory.h"
#include "nsIModule.h"

#include "nsRDFModule.h"
#include "nsIFactory.h"
#include "nsILocalStore.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFContentSink.h"
#include "nsIRDFService.h"
#include "nsIXULContentSink.h"
#include "nsIXULPrototypeCache.h"
#include "nsISupports.h"
#include "nsRDFBaseDataSources.h"
#include "nsRDFBuiltInDataSources.h"
#include "nsIRDFFileSystem.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "rdf.h"
#include "nsIXULContentUtils.h"
#include "nsIXULDocument.h"
#include "nsIXULSortService.h"
#include "nsIXULPopupListener.h"
#include "nsIServiceManager.h"
#include "nsIElementFactory.h"
#include "nsIControllerCommand.h"

extern nsresult NS_NewXULElementFactory(nsIElementFactory** aResult);

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

extern NS_IMETHODIMP
NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult);


MAKE_CTOR(RDFService,RDFService,RDFService)
MAKE_CTOR(XULSortService,XULSortService,XULSortService)
MAKE_CTOR(XULPopupListener,XULPopupListener,XULPopupListener)
MAKE_CTOR(XULElementFactory, XULElementFactory, ElementFactory)
MAKE_CTOR(RDFXMLDataSource,RDFXMLDataSource,RDFDataSource)
MAKE_CTOR(RDFFileSystemDataSource,RDFFileSystemDataSource,RDFDataSource)
MAKE_CTOR(RDFCompositeDataSource,RDFCompositeDataSource,RDFCompositeDataSource)
MAKE_CTOR(RDFContainer,RDFContainer,RDFContainer)

MAKE_CTOR(RDFContainerUtils,RDFContainerUtils,RDFContainerUtils)
MAKE_CTOR(XULDocument,XULDocument,XULDocument)
MAKE_CTOR(XULTemplateBuilder,XULTemplateBuilder,RDFContentModelBuilder)

MAKE_CTOR(RDFContentSink,RDFContentSink,RDFContentSink)
MAKE_CTOR(XULContentSink,XULContentSink,XULContentSink)
MAKE_CTOR(RDFDefaultResource,DefaultResource,RDFResource)
MAKE_CTOR(LocalStore,LocalStore,LocalStore)

MAKE_CTOR(ControllerCommandManager,ControllerCommandManager,ControllerCommandManager)

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

    { "Local Store", 
      NS_LOCALSTORE_CID,
      NS_RDF_DATASOURCE_CONTRACTID_PREFIX "local-store", 
      CreateNewLocalStore
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
    
    { "XUL Sort Service", 
      NS_XULSORTSERVICE_CID,
      NS_RDF_CONTRACTID "/xul-sort-service;1", 
      CreateNewXULSortService
    },
        
    { "XUL Template Builder", 
      NS_XULTEMPLATEBUILDER_CID,
      NS_RDF_CONTRACTID "/xul-template-builder;1", 
      CreateNewXULTemplateBuilder      
    },
    
    { "XUL Content Sink", 
      NS_XULCONTENTSINK_CID,
      NS_RDF_CONTRACTID "/xul-content-sink;1", 
      CreateNewXULContentSink
    },
    
    { "XUL Document", 
      NS_XULDOCUMENT_CID,
      NS_RDF_CONTRACTID "/xul-document;1", 
      CreateNewXULDocument
    },
    
    { "XUL PopupListener", 
      NS_XULPOPUPLISTENER_CID,
      NS_RDF_CONTRACTID "/xul-popup-listener;1", 
      CreateNewXULPopupListener
    },
    
    { "XUL Controllers", 
      NS_XULCONTROLLERS_CID,
      NS_RDF_CONTRACTID "/xul-controllers;1", 
      NS_NewXULControllers
    },

    { "XUL Content Utilities", 
      NS_XULCONTENTUTILS_CID ,
      NS_RDF_CONTRACTID "/xul-content-utils;1", 
      NS_NewXULContentUtils
    },
    
    { "XUL Prototype Cache", 
      NS_XULPROTOTYPECACHE_CID,
      NS_RDF_CONTRACTID "/xul-prototype-cache;1", 
      NS_NewXULPrototypeCache
    },

    { "XUL Element Factory",
      NS_XULELEMENTFACTORY_CID,
      NS_ELEMENT_FACTORY_CONTRACTID_PREFIX "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
      CreateNewXULElementFactory
    },

    { "Controller Command Manager",
      NS_CONTROLLERCOMMANDMANAGER_CID,
      NS_RDF_CONTRACTID "/controller-command-manager;1",
      CreateNewControllerCommandManager
    },
};

NS_IMPL_NSGETMODULE("nsRDFModule", components);
