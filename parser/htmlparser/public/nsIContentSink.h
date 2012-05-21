/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIContentSink_h___
#define nsIContentSink_h___

/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 * This pure virtual interface is used as the "glue" that connects the parsing 
 * process to the content model construction process.
 *
 * The icontentsink interface is a very lightweight wrapper that represents the
 * content-sink model building process. There is another one that you may care 
 * about more, which is the IHTMLContentSink interface. (See that file for details).
 */
#include "nsISupports.h"
#include "nsStringGlue.h"
#include "mozFlushType.h"
#include "nsIDTD.h"

class nsParserBase;

#define NS_ICONTENT_SINK_IID \
{ 0xcf9a7cbb, 0xfcbc, 0x4e13, \
  { 0x8e, 0xf5, 0x18, 0xef, 0x2d, 0x3d, 0x58, 0x29 } }

class nsIContentSink : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENT_SINK_IID)

  /**
   * This method is called by the parser when it is entered from
   * the event loop. The content sink wants to know how long the
   * parser has been active since we last processed events on the
   * main event loop and this call calibrates that measurement.
   */
  NS_IMETHOD WillParse(void)=0;

  /**
   * This method gets called when the parser begins the process
   * of building the content model via the content sink.
   *
   * Default implementation provided since the sink should have the option of
   * doing nothing in response to this call.
   *
   * @update 5/7/98 gess
   */
  NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode) {
    return NS_OK;
  }

  /**
   * This method gets called when the parser concludes the process
   * of building the content model via the content sink.
   *
   * Default implementation provided since the sink should have the option of
   * doing nothing in response to this call.
   *
   * @update 5/7/98 gess
   */
  NS_IMETHOD DidBuildModel(bool aTerminated) {
    return NS_OK;
  }

  /**
   * This method gets called when the parser gets i/o blocked,
   * and wants to notify the sink that it may be a while before
   * more data is available.
   *
   * @update 5/7/98 gess
   */
  NS_IMETHOD WillInterrupt(void)=0;

  /**
   * This method gets called when the parser i/o gets unblocked,
   * and we're about to start dumping content again to the sink.
   *
   * @update 5/7/98 gess
   */
  NS_IMETHOD WillResume(void)=0;

  /**
   * This method gets called by the parser so that the content
   * sink can retain a reference to the parser. The expectation
   * is that the content sink will drop the reference when it
   * gets the DidBuildModel notification i.e. when parsing is done.
   */
  NS_IMETHOD SetParser(nsParserBase* aParser)=0;

  /**
   * Flush content so that the content model is in sync with the state
   * of the sink.
   *
   * @param aType the type of flush to perform
   */
  virtual void FlushPendingNotifications(mozFlushType aType)=0;

  /**
   * Set the document character set. This should be passed on to the
   * document itself.
   */
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset)=0;

  /**
   * Returns the target object (often a document object) into which
   * the content built by this content sink is being added, if any
   * (IOW, may return null).
   */
  virtual nsISupports *GetTarget()=0;
  
  /**
   * Returns true if there's currently script executing that we need to hold
   * parsing for.
   */
  virtual bool IsScriptExecuting()
  {
    return false;
  }
  
  /**
   * Posts a runnable that continues parsing.
   */
  virtual void ContinueInterruptedParsingAsync() {};
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentSink, NS_ICONTENT_SINK_IID)

#endif /* nsIContentSink_h___ */
