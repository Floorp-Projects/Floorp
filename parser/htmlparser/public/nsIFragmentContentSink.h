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
 { 0x2b23c1fb, 0xb83c, 0x436b, \
 { 0xb7, 0x2a, 0x9c, 0xbe, 0xf1, 0xe9, 0x9b, 0x20 } };

class nsIFragmentContentSink : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_I_FRAGMENT_CONTENT_SINK_IID)
  /**
   * This method is used to obtain the fragment created by
   * a fragment content sink. The value returned will be null
   * if the content sink hasn't yet received parser notifications.
   *
   */
  NS_IMETHOD GetFragment(nsIDOMDocumentFragment** aFragment) = 0;

  /**
   * This method is used to set the target document for this fragment
   * sink.  This document's nodeinfo manager will be used to create
   * the content objects.  This MUST be called before the sink is used.
   *
   * @param aDocument the document the new nodes will belong to
   * (should not be null)
   */
  NS_IMETHOD SetTargetDocument(nsIDocument* aDocument) = 0;
};

/**
 * Base version takes string nested in context, context ends with <endnote>.
 * 2 version just loads whole string.
 */

#define NS_HTMLFRAGMENTSINK_CONTRACTID "@mozilla.org/layout/htmlfragmentsink;1"
#define NS_HTMLFRAGMENTSINK2_CONTRACTID "@mozilla.org/layout/htmlfragmentsink;2"

#define NS_XMLFRAGMENTSINK_CONTRACTID "@mozilla.org/layout/xmlfragmentsink;1"
#define NS_XMLFRAGMENTSINK2_CONTRACTID "@mozilla.org/layout/xmlfragmentsink;2"

// the HTML versions are in nsHTMLParts.h
nsresult
NS_NewXMLFragmentContentSink(nsIFragmentContentSink** aInstancePtrResult);
nsresult
NS_NewXMLFragmentContentSink2(nsIFragmentContentSink** aInstancePtrResult);

#endif
