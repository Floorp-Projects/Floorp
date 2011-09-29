/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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

class nsIParser;

// 57b395ad-4276-408c-9f98-7044b5025c3d
#define NS_ICONTENT_SINK_IID \
{ 0x57b395ad, 0x4276, 0x408c, \
  { 0x9f, 0x98, 0x70, 0x44, 0xb5, 0x02, 0x5c, 0x3d } }

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
  NS_IMETHOD SetParser(nsIParser* aParser)=0;

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
    return PR_FALSE;
  }
  
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentSink, NS_ICONTENT_SINK_IID)

#endif /* nsIContentSink_h___ */
