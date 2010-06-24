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
#ifndef nsIFragmentContentSink_h___
#define nsIFragmentContentSink_h___

#include "nsISupports.h"

class nsIDOMDocumentFragment;
class nsIDocument;

#define NS_I_FRAGMENT_CONTENT_SINK_IID \
  { 0x1ecdb30d, 0x1f10, 0x45d2, \
    { 0xa4, 0xf4, 0xec, 0xbc, 0x03, 0x52, 0x9a, 0x7e } }

#define NS_I_PARANOID_FRAGMENT_CONTENT_SINK_IID \
  { 0x86b5390d, 0xd80e, 0x4a86, \
    { 0x83, 0xec, 0xda, 0x44, 0xac, 0x5b, 0x8c, 0x5f } }

/**
 * The fragment sink allows a client to parse a fragment of sink, possibly
 * surrounded in context. Also see nsIParser::ParseFragment().
 * Note: once you've parsed a fragment, the fragment sink must be re-set on
 * the parser in order to parse another fragment.
 */
class nsIFragmentContentSink : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_I_FRAGMENT_CONTENT_SINK_IID)
  /**
   * This method is used to obtain the fragment created by
   * a fragment content sink. The value returned will be null
   * if the content sink hasn't yet received parser notifications.
   *
   * If aWillOwnFragment is PR_TRUE then the sink should drop its
   * ownership of the fragment.
   */
  NS_IMETHOD GetFragment(PRBool aWillOwnFragment,
                         nsIDOMDocumentFragment** aFragment) = 0;

  /**
   * This method is used to set the target document for this fragment
   * sink.  This document's nodeinfo manager will be used to create
   * the content objects.  This MUST be called before the sink is used.
   *
   * @param aDocument the document the new nodes will belong to
   * (should not be null)
   */
  NS_IMETHOD SetTargetDocument(nsIDocument* aDocument) = 0;

  /**
   * This method is used to indicate to the sink that we're done building
   * the context and should start paying attention to the incoming content
   */
  NS_IMETHOD WillBuildContent() = 0;

  /**
   * This method is used to indicate to the sink that we're done building
   * The real content. This is useful if you want to parse additional context
   * (such as an end context).
   */
  NS_IMETHOD DidBuildContent() = 0;

  /**
   * This method is a total hack to help with parsing fragments. It is called to
   * tell the fragment sink that a container from the context will be delivered
   * after the call to WillBuildContent(). This is only relevent for HTML
   * fragments that use nsHTMLTokenizer/CNavDTD.
   */
  NS_IMETHOD IgnoreFirstContainer() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIFragmentContentSink,
                              NS_I_FRAGMENT_CONTENT_SINK_IID)

/**
 * This interface is implemented by paranoid content sinks, and allows consumers
 * to add tags and attributes to the default white-list set.
 */
class nsIParanoidFragmentContentSink : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_I_PARANOID_FRAGMENT_CONTENT_SINK_IID)

  /**
   * Allow the content sink to accept style elements and attributes.
   */
  virtual void AllowStyles() = 0;

  /**
   * Allow the content sink to accept comments.
   */
  virtual void AllowComments() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIParanoidFragmentContentSink,
                              NS_I_PARANOID_FRAGMENT_CONTENT_SINK_IID)

/**
 * Base version takes string nested in context, content surrounded by
 * WillBuildContent()/DidBuildContent() calls. The 2nd version just loads
 * the whole string.
 */

#define NS_HTMLFRAGMENTSINK_CONTRACTID "@mozilla.org/layout/htmlfragmentsink;1"
#define NS_HTMLFRAGMENTSINK2_CONTRACTID "@mozilla.org/layout/htmlfragmentsink;2"
#define NS_HTMLPARANOIDFRAGMENTSINK_CONTRACTID \
"@mozilla.org/htmlparanoidfragmentsink;1"
#define NS_HTMLPARANOIDFRAGMENTSINK2_CONTRACTID \
"@mozilla.org/htmlparanoidfragmentsink;2"

#define NS_XMLFRAGMENTSINK_CONTRACTID "@mozilla.org/layout/xmlfragmentsink;1"
#define NS_XMLFRAGMENTSINK2_CONTRACTID "@mozilla.org/layout/xmlfragmentsink;2"
#define NS_XHTMLPARANOIDFRAGMENTSINK_CONTRACTID \
"@mozilla.org/xhtmlparanoidfragmentsink;1"
#define NS_XHTMLPARANOIDFRAGMENTSINK2_CONTRACTID \
"@mozilla.org/xhtmlparanoidfragmentsink;2"


// the HTML versions are in nsHTMLParts.h
nsresult
NS_NewXMLFragmentContentSink(nsIFragmentContentSink** aInstancePtrResult);
nsresult
NS_NewXMLFragmentContentSink2(nsIFragmentContentSink** aInstancePtrResult);

// This strips all but a whitelist of elements and attributes defined
// in nsContentSink.h
nsresult
NS_NewXHTMLParanoidFragmentSink(nsIFragmentContentSink** aInstancePtrResult);
nsresult
NS_NewXHTMLParanoidFragmentSink2(nsIFragmentContentSink** aInstancePtrResult);
void
NS_XHTMLParanoidFragmentSinkShutdown();
#endif
