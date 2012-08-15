/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A catch-all header file for miscellaneous RDF stuff. Currently
  contains error codes and vocabulary macros.

 */

#ifndef rdf_h___
#define rdf_h___

#include "nsError.h"

/**
 * The following macros are to aid in vocabulary definition.  They
 * creates const char*'s for "kURI[prefix]_[name]", appropriate
 * complete namespace qualification on the URI, e.g.,
 *
 * #define RDF_NAMESPACE_URI "http://www.w3.org/TR/WD-rdf-syntax#"
 * DEFINE_RDF_ELEMENT(RDF_NAMESPACE_URI, RDF, ID);
 *
 * will define:
 *
 * kURIRDF_ID to be "http://www.w3.org/TR/WD-rdf-syntax#ID"
 */

#define DEFINE_RDF_VOCAB(ns, prefix, name) \
static const char kURI##prefix##_##name[] = ns #name

/**
 * Core RDF vocabularies that we use to define semantics
 */

#define RDF_NAMESPACE_URI  "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define WEB_NAMESPACE_URI  "http://home.netscape.com/WEB-rdf#"
#define NC_NAMESPACE_URI   "http://home.netscape.com/NC-rdf#"
#define DEVMO_NAMESPACE_URI_PREFIX "http://developer.mozilla.org/rdf/vocabulary/"


/* ContractID prefixes for RDF DLL registration. */
#define NS_RDF_CONTRACTID                           "@mozilla.org/rdf"
#define NS_RDF_DATASOURCE_CONTRACTID                NS_RDF_CONTRACTID "/datasource;1"
#define NS_RDF_DATASOURCE_CONTRACTID_PREFIX         NS_RDF_DATASOURCE_CONTRACTID "?name="
#define NS_RDF_RESOURCE_FACTORY_CONTRACTID          "@mozilla.org/rdf/resource-factory;1"
#define NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX   NS_RDF_RESOURCE_FACTORY_CONTRACTID "?name="
#define NS_RDF_INFER_DATASOURCE_CONTRACTID_PREFIX   NS_RDF_CONTRACTID "/infer-datasource;1?engine="

#define NS_RDF_SERIALIZER                            NS_RDF_CONTRACTID "/serializer;1?format="

// contract ID is in the form
// @mozilla.org/rdf/delegate-factory;1?key=<key>&scheme=<scheme>
#define NS_RDF_DELEGATEFACTORY_CONTRACTID    "@mozilla.org/rdf/delegate-factory;1"
#define NS_RDF_DELEGATEFACTORY_CONTRACTID_PREFIX    NS_RDF_DELEGATEFACTORY_CONTRACTID "?key="

/*@}*/

#endif /* rdf_h___ */
