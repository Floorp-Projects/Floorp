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

#ifndef nsHTItem_h___
#define nsHTItem_h___

class nsHierarchicalDataModel;
class nsIContent;

class nsHTItem 
                  
{
public:
    nsHTItem(nsIContent* pContent, nsHierarchicalDataModel* pDataModel);
    virtual ~nsHTItem();

	virtual PRBool IsExpandedDelegate() const;
	virtual PRUint32 GetIndentationLevelDelegate() const;
	virtual void SetIndentationLevelDelegate(PRUint32 n);

public:
	nsIContent* FindChildWithName(const nsString& name) const; // Caller must release the content ptr.
	
protected:
	nsHierarchicalDataModel* mDataModel;
	nsIContent* mContentNode;
	PRUint32 mIndentationLevel;
};

#endif /* nsHTItem_h___ */
