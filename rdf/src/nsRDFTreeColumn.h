/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsRDFTreeColumn_h__
#define nsRDFTreeColumn_h__

#include "nsString.h"
#include "nsITreeColumn.h"

class nsRDFTreeColumn : public nsITreeColumn {
public:
    nsRDFTreeColumn(const nsString& name);
    virtual ~nsRDFTreeColumn(void);

    ////////////////////////////////////////////////////////////////////////
    // nsISupports interface

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////
    // nsITreeColumn interface

    // Inspectors
    NS_IMETHOD GetPixelWidth(PRUint32& width) const;
    NS_IMETHOD GetDesiredPercentage(double& percentage) const;
    NS_IMETHOD GetSortState(nsColumnSortState& answer) const;
    NS_IMETHOD GetColumnName(nsString& name) const;

    // Setters
    NS_IMETHOD SetPixelWidth(PRUint32 newWidth);

private:
    nsString          mName;
    PRUint32          mWidth;
    nsColumnSortState mSortState;
    double            mDesiredPercentage;

    static const PRUint32 kDefaultWidth;
};

#endif // nsRDFTreeColumn_h__
