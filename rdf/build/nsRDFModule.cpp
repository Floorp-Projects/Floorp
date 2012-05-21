/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "mozilla/ModuleUtils.h"

#include "nsIFactory.h"
#include "nsRDFService.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentSink.h"
#include "nsISupports.h"
#include "nsRDFBaseDataSources.h"
#include "nsRDFBuiltInDataSources.h"
#include "nsFileSystemDataSource.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsILocalStore.h"
#include "nsRDFXMLParser.h"
#include "nsRDFXMLSerializer.h"

#include "rdfISerializer.h"

//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.

#define MAKE_CTOR(_func,_new,_ifname)                                \
static nsresult                                                      \
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

MAKE_CTOR(RDFXMLDataSource,RDFXMLDataSource,RDFDataSource)
MAKE_CTOR(RDFCompositeDataSource,RDFCompositeDataSource,RDFCompositeDataSource)
MAKE_CTOR(RDFContainer,RDFContainer,RDFContainer)

MAKE_CTOR(RDFContainerUtils,RDFContainerUtils,RDFContainerUtils)

MAKE_CTOR(RDFContentSink,RDFContentSink,RDFContentSink)
MAKE_CTOR(RDFDefaultResource,DefaultResource,RDFResource)

#define MAKE_RDF_CTOR(_func,_new,_ifname)                            \
static nsresult                                                      \
CreateNew##_func(nsISupports* aOuter, REFNSIID aIID, void **aResult) \
{                                                                    \
    if (!aResult) {                                                  \
        return NS_ERROR_INVALID_POINTER;                             \
    }                                                                \
    if (aOuter) {                                                    \
        *aResult = nsnull;                                           \
        return NS_ERROR_NO_AGGREGATION;                              \
    }                                                                \
    rdfI##_ifname* inst;                                             \
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
NS_NewTriplesSerializer(rdfISerializer** aResult);

MAKE_RDF_CTOR(TriplesSerializer, TriplesSerializer, Serializer)

NS_DEFINE_NAMED_CID(NS_RDFCOMPOSITEDATASOURCE_CID);
NS_DEFINE_NAMED_CID(NS_RDFFILESYSTEMDATASOURCE_CID);
NS_DEFINE_NAMED_CID(NS_RDFINMEMORYDATASOURCE_CID);
NS_DEFINE_NAMED_CID(NS_RDFXMLDATASOURCE_CID);
NS_DEFINE_NAMED_CID(NS_RDFDEFAULTRESOURCE_CID);
NS_DEFINE_NAMED_CID(NS_RDFCONTENTSINK_CID);
NS_DEFINE_NAMED_CID(NS_RDFCONTAINER_CID);
NS_DEFINE_NAMED_CID(NS_RDFCONTAINERUTILS_CID);
NS_DEFINE_NAMED_CID(NS_RDFSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_RDFXMLPARSER_CID);
NS_DEFINE_NAMED_CID(NS_RDFXMLSERIALIZER_CID);
NS_DEFINE_NAMED_CID(NS_RDFNTRIPLES_SERIALIZER_CID);
NS_DEFINE_NAMED_CID(NS_LOCALSTORE_CID);


static const mozilla::Module::CIDEntry kRDFCIDs[] = {
    { &kNS_RDFCOMPOSITEDATASOURCE_CID, false, NULL, CreateNewRDFCompositeDataSource },
    { &kNS_RDFFILESYSTEMDATASOURCE_CID, false, NULL, FileSystemDataSource::Create },
    { &kNS_RDFINMEMORYDATASOURCE_CID, false, NULL, NS_NewRDFInMemoryDataSource },
    { &kNS_RDFXMLDATASOURCE_CID, false, NULL, CreateNewRDFXMLDataSource },
    { &kNS_RDFDEFAULTRESOURCE_CID, false, NULL, CreateNewRDFDefaultResource },
    { &kNS_RDFCONTENTSINK_CID, false, NULL, CreateNewRDFContentSink },
    { &kNS_RDFCONTAINER_CID, false, NULL, CreateNewRDFContainer },
    { &kNS_RDFCONTAINERUTILS_CID, false, NULL, CreateNewRDFContainerUtils },
    { &kNS_RDFSERVICE_CID, false, NULL, RDFServiceImpl::CreateSingleton },
    { &kNS_RDFXMLPARSER_CID, false, NULL, nsRDFXMLParser::Create },
    { &kNS_RDFXMLSERIALIZER_CID, false, NULL, nsRDFXMLSerializer::Create },
    { &kNS_RDFNTRIPLES_SERIALIZER_CID, false, NULL, CreateNewTriplesSerializer },
    { &kNS_LOCALSTORE_CID, false, NULL, NS_NewLocalStore },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kRDFContracts[] = {
    { NS_RDF_DATASOURCE_CONTRACTID_PREFIX "composite-datasource", &kNS_RDFCOMPOSITEDATASOURCE_CID },
    { NS_RDF_DATASOURCE_CONTRACTID_PREFIX "files", &kNS_RDFFILESYSTEMDATASOURCE_CID },
    { NS_RDF_DATASOURCE_CONTRACTID_PREFIX "in-memory-datasource", &kNS_RDFINMEMORYDATASOURCE_CID },
    { NS_RDF_DATASOURCE_CONTRACTID_PREFIX "xml-datasource", &kNS_RDFXMLDATASOURCE_CID },
    { NS_RDF_RESOURCE_FACTORY_CONTRACTID, &kNS_RDFDEFAULTRESOURCE_CID },
    { NS_RDF_CONTRACTID "/content-sink;1", &kNS_RDFCONTENTSINK_CID },
    { NS_RDF_CONTRACTID "/container;1", &kNS_RDFCONTAINER_CID },
    { NS_RDF_CONTRACTID "/container-utils;1", &kNS_RDFCONTAINERUTILS_CID },
    { NS_RDF_CONTRACTID "/rdf-service;1", &kNS_RDFSERVICE_CID },
    { NS_RDF_CONTRACTID "/xml-parser;1", &kNS_RDFXMLPARSER_CID },
    { NS_RDF_CONTRACTID "/xml-serializer;1", &kNS_RDFXMLSERIALIZER_CID },
    { NS_RDF_SERIALIZER "ntriples", &kNS_RDFNTRIPLES_SERIALIZER_CID },
    { NS_LOCALSTORE_CONTRACTID, &kNS_LOCALSTORE_CID },
    { NULL }
};

static nsresult
StartupRDFModule()
{
    if (RDFServiceImpl::gRDFService) {
        NS_ERROR("Leaked the RDF service from a previous startup.");
        RDFServiceImpl::gRDFService = nsnull;
    }

    return NS_OK;
}

static void
ShutdownRDFModule()
{
    if (RDFServiceImpl::gRDFService) {
        // XXX make this an assertion!
        NS_WARNING("Leaking the RDF Service.");
    }
}

static const mozilla::Module kRDFModule = {
    mozilla::Module::kVersion,
    kRDFCIDs,
    kRDFContracts,
    NULL,
    NULL,
    StartupRDFModule,
    ShutdownRDFModule
};

NSMODULE_DEFN(nsRDFModule) = &kRDFModule;
