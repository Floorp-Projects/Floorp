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

class nsIRDFDataBase;

class nsRDFDataModel : public nsIDataModel {
public:
    nsRDFDataModel(nsIRDFDataBase& db);
    virtual ~nsRDFDataModel(void);

    ////////////////////////////////////////////////////////////////////////
    // nsISupports interface

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////
    // nsIDataModel interface

    // Inspectors
    NS_IMETHOD GetDMWidget(nsIDMWidget*& pWidget) const;
	
    // Setters
    NS_IMETHOD SetDMWidget(nsIDMWidget* pWidget);

    // Methods to query the data model for property values for an entire widget.
    NS_IMETHOD GetStringPropertyValue(nsString& value, const nsString& property) const;
    NS_IMETHOD GetIntPropertyValue(PRInt32& value, const nsString& property) const;

private:
    nsIRDFDataBase& mDB;
    nsIDMWidget* mWidget;
};

#endif // nsRDFDataModel_h__
