/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIRDFDataSource_h__
#define nsIRDFDataSource_h__

/*
  This file contains the interface definition for an RDF data source.

  Classes which implement this interface for a particular type of URL
  are registered with the RDF singleton via nsIRDF::RegisterHandler(...)
*/

#include "nsISupports.h"
#include "rdf.h"

// 852666b0-2cce-11d2-bc14-00805f912fe7
#define NS_IRDFDATASOURCE_IID \
{ \
  0x852666b0, \
  0x2cce, \
  0x11d2, \
  { 0xb, 0xc14,0x00, 0x80, 0x5f, 0x91 0x2f, 0xe7 } \
}

class nsIRDFCursor;
class nsIRDFObserver;
class nsIRDFDataBase;

class nsIRDFDataSource : public nsISupports {
public:

#ifdef RDF_NOT_IMPLEMENTED
  /**
   * Initialize this data source
   */
  NS_IMETHOD Initialize(RDF_String url,
                        nsIRDFResourceManager* m);
#endif

  NS_IMETHOD GetResource(RDF_String id,
                         RDF_Resource * r) = 0;

  NS_IMETHOD CreateResource(RDF_String id,
                            RDF_Resource* r) = 0;

  NS_IMETHOD ReleaseResource(RDF_Resource r) = 0;

  /**
   * Get the name of this data source.
   *
   * For regular data sources, this will be the URL of the source.
   *
   * For aggregated sources, it generally will not be a valid RDF URL.
   */
  NS_IMETHOD GetName(const RDF_String* name /* out */ ) = 0;

  /**
   * Find an RDF resource that points to a given node over the
   * specified arc & truth value (defaults to "PR_TRUE").
   */
  NS_IMETHOD GetSource(RDF_Node target,
                       RDF_Resource arcLabel,
                       RDF_Resource *source /* out */) = 0;

  NS_IMETHOD GetSource(RDF_Node target,
                       RDF_Resource arcLabel,
                       PRBool tv,
                       RDF_Resource *source /* out */) = 0;

  /**
   * Find all RDF resources that point to a given node over the
   * specified arc & truth value (defaults to "PR_TRUE").
   */
  NS_IMETHOD GetSources(RDF_Resource target,
                        RDF_Resource arcLabel,
                        nsIRDFCursor **sources /* out */) = 0;

  NS_IMETHOD GetSources(RDF_Resource target,
                        RDF_Resource arcLabel,
                        PRBool tv,
                        nsIRDFCursor **sources /* out */) = 0;

  /**
   * Find a child of that is related to the source by the given arc
   * arc and truth value (defaults to PR_TRUE).
   */
  NS_IMETHOD GetTarget(RDF_Resource source,
                       RDF_Resource arcLabel,
                       RDF_ValueType targetType,
                       RDF_NodeStruct& target /* in/out */) = 0;

  NS_IMETHOD GetTarget(RDF_Resource source,
                       RDF_Resource arcLabel,
                       RDF_ValueType targetType,
                       PRBool tv,
                       RDF_NodeStruct& target /* in/out */) = 0;

  /**
   * Find all children of that are related to the source by the given arc
   * arc and truth value (defaults to PR_TRUE).
   */
  NS_IMETHOD GetTargets(RDF_Resource source,
                        RDF_Resource arcLabel,
                        RDF_ValueType targetType,
                        nsIRDFCursor **targets /* out */) = 0;

  NS_IMETHOD GetTargets(RDF_Resource source,
                        RDF_Resource arcLabel,
                        PRBool tv,
                        RDF_ValueType targetType,
                        nsIRDFCursor **targets /* out */) = 0;

#ifdef RDF_NOT_IMPLEMENTED
  /**
   * Find all parents that point to a node over a given arc label,
   * regardless of truth value.
   */
  NS_IMETHOD GetAllSources(RDF_Node target,
                           RDF_Resource arcLabel,
                           nsIRDFCursor2 **sources /* out */) = 0;

  /**
   * Find all children of a resource that are related by the 
   * given arc label, regardless of the truth value.
   */ 
  NS_IMETHOD GetAllTargets(RDF_Resource source,
                           RDF_Resource arcLabel,
                           RDF_ValueType targetType,
                           nsIRDFCursor2 **targets /* out */);
#endif /* RDF_NOT_IMPLEMENTED */


  /**
   * Add an assertion to the graph.
   */
  NS_IMETHOD Assert(RDF_Resource source, 
                    RDF_Resource arcLabel, 
                    RDF_Node target,
                    PRBool tv = PR_TRUE) = 0;

  /**
   * Remove an assertion from the graph.
   */
  NS_IMETHOD Unassert(RDF_Resource source,
                      RDF_Resource arcLabel,
                      RDF_Node target) = 0;

  /**
   * Query whether an assertion exists in this graph.
   *
   */
  NS_IMETHOD HasAssertion(RDF_Resource source,
                          RDF_Resource arcLabel,
                          RDF_Node target,
                          PRBool truthValue,
                          PRBool* hasAssertion /* out */) = 0;

  /**
   * Add an observer to this data source.
   */
  NS_IMETHOD AddObserver(nsIRDFObserver *n,
                         RDF_EventMask type = RDF_ANY_NOTIFY) = 0;

  /**
   * Remove an observer from this data source
   */
  NS_IMETHOD RemoveObserver(nsIRDFObserver *n,
                            RDF_EventMask = RDF_ANY_NOTIFY) = 0;

  /**
   * Get a cursor to iterate over all the arcs that point into a node.
   */
  NS_IMETHOD ArcLabelsIn(RDF_Node node,
                         nsIRDFCursor **labels /* out */) = 0;

  /**
   * Get a cursor to iterate over all the arcs that originate in
   * a resource.
   */
  NS_IMETHOD ArcLabelsOut(RDF_Resource source,
                          nsIRDFCursor **labels /* out */) = 0;

#ifdef RDF_NOT_IMPLEMENTED
  /**
   * Notify this data source that it is a child of a database.
   *
   * The datasource must send notifications to the parent when 
   * changes to it's graph are made, in case the parent has observers
   * interested in the events generated.
   */
  NS_IMETHOD AddParent(nsIRDFDataBase* parent) = 0;

  /**
   * Notify this data source that it has been disconnected from a
   * parent.
   */
  NS_IMETHOD RemoveParent(nsIRDFDataBase* parent) = 0;

  /**
   * Request that a data source obtain updates if applicable.
   */
  // XXX move this to an nsIRDFRemoteDataStore interface?
  NS_IMETHOD Update(RDF_Resource hint) = 0;
#endif /* RDF_NOT_IMPLEMENTED */

  /**
   * Request that a data source write it's contents out to 
   * permanent storage if applicable.
   */
  NS_IMETHOD Flush() = 0;

};


#endif /* nsIRDFDataSource_h__ */
