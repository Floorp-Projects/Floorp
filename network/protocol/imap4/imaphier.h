/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

