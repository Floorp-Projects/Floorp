/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsHTColumn_h___
#define nsHTColumn_h___

#include "nsTreeColumn.h"

//------------------------------------------------------------
// This class functions as the data source for column information (things like
// width, desired percentage, and sorting).

class nsHTColumn : public nsTreeColumn
                  
{
public:
    nsHTColumn();
    virtual ~nsHTColumn();

	// Inspectors
	virtual int GetPixelWidth() const;
	virtual double GetDesiredPercentage() const;
	virtual PRBool IsSortColumn() const;
	virtual void GetColumnName(nsString& name) const;

	// Setters
	virtual void SetPixelWidth(int newWidth);
	virtual void SetDesiredPercentage(double newPercentage);

protected:
	int mPixelWidth;
	double mDesiredPercentage;
};

#endif /* nsHTColumn_h___ */
