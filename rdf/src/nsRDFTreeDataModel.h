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

#ifndef nsRDFTreeDataModel_h__
#define nsRDFTreeDataModel_h__

#include "nsRDFDataModel.h"
#include "nsITreeDataModel.h"
#include "nsVector.h"
#include "rdf.h"

////////////////////////////////////////////////////////////////////////

/**
 * An implementation for the tree widget model.
 */
class nsRDFTreeDataModel : public nsITreeDataModel, public nsRDFDataModel {
public:
    nsRDFTreeDataModel(void);
    virtual ~nsRDFTreeDataModel(void);

    ////////////////////////////////////////////////////////////////////////
    // nsISupports interface -- delgates to superclass

    NS_IMETHOD_(nsrefcnt) AddRef(void);
    NS_IMETHOD_(nsrefcnt) Release(void);
    NS_IMETHOD QueryInterface(const nsIID& iid, void** result);


    ////////////////////////////////////////////////////////////////////////
    // nsIDataModel interface -- delgates to superclass

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
    // nsITreeDataModel interface

    // Column APIs
    NS_IMETHOD GetVisibleColumnCount(PRUint32& count) const;
    NS_IMETHOD GetColumnCount(PRUint32& count) const;
    NS_IMETHOD GetNthColumn(nsITreeColumn*& pColumn, PRUint32 n) const;
	
    // TreeItem APIs
    NS_IMETHOD GetFirstVisibleItemIndex(PRUint32& index) const;
    NS_IMETHOD GetNthTreeItem(nsITreeDMItem*& pItem, PRUint32 n) const;
    NS_IMETHOD GetItemTextForColumn(nsString& nodeText, nsITreeDMItem* pItem, nsITreeColumn* pColumn) const;

    ////////////////////////////////////////////////////////////////////////
    // Implemantation methods

    void AddColumn(const nsString& name, RDF_Resource property);

    virtual NS_METHOD
    CreateItem(RDF_Resource r, nsRDFDataModelItem*& result);

private:
    nsVector       mColumns;

    nsresult CreateColumns(void);
};



#endif // nsRDFTreeDataModel_h__
