/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#ifndef IMAPHIER
#define IMAPHIER

#include "xp_list.h"


class TNavigatorImapConnection;

struct HierarchyNode {
	char    *fPresentName;
	char    *fNodeToken;	// leaf name of node
	XP_List *fSubFolders;
};

class ImapHierarchyMover {
public:
	ImapHierarchyMover(const char *destinationNodeName,
					   const char *sourceNodeName,
					   TNavigatorImapConnection &connection);
	virtual ~ImapHierarchyMover();
	
	virtual void   AddToHierarchy(const char *nodeName);
	virtual int    DoMove();
protected:
	virtual void   AddToSubTree(char *nodeTokens, const char *nodeName, XP_List *subTree);
	virtual void   CreateNewFolderTree(const char *destinationNodeName, XP_List *subTree);
	virtual void   DestroyOldFolderTree(XP_List *subTree);

private:
	XP_List *fMailboxHierarchy;
	
	TNavigatorImapConnection &fIMAPConnection;
	char *fDestinationNodeName;
	char *fSourceNodeName;
};

#endif

