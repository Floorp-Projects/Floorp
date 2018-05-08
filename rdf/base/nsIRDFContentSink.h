/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  An RDF-specific content sink. The content sink is targeted by the
  parser for building the RDF content model.

 */

#ifndef nsIRDFContentSink_h___
#define nsIRDFContentSink_h___

#include "nsIXMLContentSink.h"
class nsIRDFDataSource;
class nsIURI;

// {3a7459d7-d723-483c-aef0-404fc48e09b8}
#define NS_IRDFCONTENTSINK_IID \
{ 0x3a7459d7, 0xd723, 0x483c, { 0xae, 0xf0, 0x40, 0x4f, 0xc4, 0x8e, 0x09, 0xb8 } }

/**
 * This interface represents a content sink for RDF files.
 */

class nsIRDFContentSink : public nsIXMLContentSink {
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IRDFCONTENTSINK_IID)

    /**
     * Initialize the content sink.
     */
    NS_IMETHOD Init(nsIURI* aURL) = 0;

    /**
     * Set the content sink's RDF Data source
     */
    NS_IMETHOD SetDataSource(nsIRDFDataSource* aDataSource) = 0;

    /**
     * Retrieve the content sink's RDF data source.
     */
    NS_IMETHOD GetDataSource(nsIRDFDataSource*& rDataSource) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIRDFContentSink, NS_IRDFCONTENTSINK_IID)

/**
 * This constructs a content sink that can be used without a
 * document, say, to create a stand-alone in-memory graph.
 */
nsresult
NS_NewRDFContentSink(nsIRDFContentSink** aResult);

#endif // nsIRDFContentSink_h___
