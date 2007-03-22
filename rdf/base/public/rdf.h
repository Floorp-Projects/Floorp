/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/**
 * @name Standard RDF error codes
 */

/*@{*/

/* Returned from nsIRDFCursor::Advance() if the cursor has no more
   elements to enuemrate */
#define NS_RDF_CURSOR_EMPTY       NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_RDF, 1)

/* Returned from nsIRDFDataSource::GetSource() and GetTarget() if the source/target
   has no value */
#define NS_RDF_NO_VALUE           NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_RDF, 2)

/* Returned from nsIRDFDataSource::Assert() and Unassert() if the assertion (or
   unassertion was accepted by the datasource*/
#define NS_RDF_ASSERTION_ACCEPTED NS_OK

/* Returned from nsIRDFDataSource::Assert() and Unassert() if the assertion (or
   unassertion) was rejected by the datasource; i.e., the datasource was not
   willing to record the statement. */
#define NS_RDF_ASSERTION_REJECTED NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_RDF, 3)

/* Return this from rdfITripleVisitor to stop cycling */
#define NS_RDF_STOP_VISIT         NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_RDF, 4)



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
