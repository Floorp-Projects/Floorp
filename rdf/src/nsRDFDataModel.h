/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsRDFDataModel_h__
#define nsRDFDataModel_h__

#include "nsIDataModel.h"
#include "rdf.h" // XXX

class nsString;
class nsRDFDataModelItem;

enum nsRDFArcType {
    eRDFArcType_Outbound, // follow outbound arcs (e.g., children-of)
    eRDFArcType_Inbound   // follow inbound arcs (e.g., parent-of)
};

////////////////////////////////////////////////////////////////////////

class nsRDFDataModel : public nsIDataModel {
private:
    // XXX eventually, when we XPCOM the back-end
    //nsIRDFDataBase& mDB;

    RDF                 mDB;
    nsRDFDataModelItem* mRoot;
    nsIDMWidget*        mWidget;

    RDF_Resource        mArcProperty;
    nsRDFArcType        mArcType;

public:
    nsRDFDataModel(void);
    virtual ~nsRDFDataModel(void);

    ////////////////////////////////////////////////////////////////////////
    // nsISupports

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////
    // nsIDataModel interface

    // Initializers
    NS_IMETHOD InitFromURL(const nsString& url);
    NS_IMETHOD InitFromResource(nsIDMItem* pResource);

    // Inspectors
    NS_IMETHOD GetDMWidget(nsIDMWidget*& pWidget) const;
	
    // Setters
    NS_IMETHOD SetDMWidget(nsIDMWidget* pWidget);

    // Methods to query the data model for property values for an entire widget.
    NS_IMETHOD GetStringPropertyValue(nsString& value, const nsString& property) const;
    NS_IMETHOD GetIntPropertyValue(PRInt32& value, const nsString& property) const;

    ////////////////////////////////////////////////////////////////////////
    // Implementation methods

    RDF GetDB(void) const {
        return mDB;
    }

    nsRDFDataModelItem* GetRoot(void) const {
        return mRoot;
    }

    nsIDMWidget* GetWidget(void) const {
        return mWidget;
    }

    void SetWidget(nsIDMWidget* widget) {
        mWidget = widget;
    }

    RDF_Resource GetArcProperty(void) const {
        return mArcProperty;
    }

    nsRDFArcType GetArcType(void) const {
        return mArcType;
    }

    virtual NS_METHOD
    CreateItem(RDF_Resource r, nsRDFDataModelItem*& result) = 0;
};

#endif // nsRDFDataModel_h__
