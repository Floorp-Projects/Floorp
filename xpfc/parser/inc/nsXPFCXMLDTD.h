/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef NS_XPFCXMLDTD__
#define NS_XPFCXMLDTD__

#include "nsxpfc.h"
#include "CNavDTD.h"
#include "nsxpfcstrings.h"

enum eXPFCXMLTags
{
  eXPFCXMLTag_unknown=0,   
  eXPFCXMLTag_doctype,
  eXPFCXMLTag_xml,

  eXPFCXMLTag_application,
  eXPFCXMLTag_button,
  eXPFCXMLTag_canvas,
  eXPFCXMLTag_dialog,
  eXPFCXMLTag_editfield,
  eXPFCXMLTag_menubar,
  eXPFCXMLTag_menucontainer,
  eXPFCXMLTag_menuitem,
  eXPFCXMLTag_object,
  eXPFCXMLTag_separator,
  eXPFCXMLTag_set,
  eXPFCXMLTag_tabwidget,
  eXPFCXMLTag_toolbar,
  eXPFCXMLTag_ui,
  eXPFCXMLTag_userdefined,
  eXPFCXMLTag_xpbutton
};

enum eXPFCXMLAttributes 
{
  eXPFCXMLAttr_unknown=0,

  eXPFCXMLAttr_key,
  eXPFCXMLAttr_tag,
  eXPFCXMLAttr_value,

  eXPFCXMLAttr_userdefined  
};


class nsXPFCXMLDTD : public CNavDTD {
            
  public:

    NS_DECL_ISUPPORTS

    nsXPFCXMLDTD();
    virtual ~nsXPFCXMLDTD();

    virtual PRBool CanParse(nsString& aContentType, PRInt32 aVersion);

    virtual eAutoDetectResult AutoDetectContentType(nsString& aBuffer,nsString& aType);
    
    NS_IMETHOD HandleToken(CToken* aToken);

    virtual nsresult CreateNewInstance(nsIDTD** aInstancePtrResult);

    nsresult HandleStartToken(CToken* aToken);
    nsresult HandleEndToken(CToken* aToken);

private:
    NS_IMETHOD_(eXPFCXMLTags)  TagTypeFromObject(const nsIParserNode& aNode) ;

};

#endif 
