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

#ifndef nsHierarchicalDataItem_h___
#define nsHierarchicalDataItem_h___

class nsHierarchicalDataModel;

//------------------------------------------------------------
// A single item in a hierarchical data model.
//------------------------------------------------------------

class nsHierarchicalDataItem
                  
{
protected:
    nsHierarchicalDataItem() {}; // Disallow instantiation of abstract class.

public:
    virtual ~nsHierarchicalDataItem() {};

	virtual PRBool IsExpanded() const = 0;
	
	virtual PRUint32 GetIndentationLevel() const = 0;
	virtual void SetIndentationLevel(PRUint32 n) = 0;

public:
	void* GetImplData() { return mImplData; }
	void SetImplData(void* pData) { mImplData = pData; }

protected:
	void* mImplData;
};

#endif /* nsHierarchicalDataItem_h___ */
