/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Simon Fraser <sfraser@netscape.com>
 */


#include "AEUtils.h"

#include "PatriciaTree.h"


/*----------------------------------------------------------------------------
	CPatriciaTree
	
----------------------------------------------------------------------------*/
CPatriciaTree::CPatriciaTree(long keyBitsLen)
:	mTree(nil)
,	mKeyBits(keyBitsLen)
{

	mTree = PatriciaInitTree(mKeyBits);
	ThrowErrIfNil(mTree, paramErr);
}

/*----------------------------------------------------------------------------
	~CPatriciaTree
	
----------------------------------------------------------------------------*/
CPatriciaTree::~CPatriciaTree()
{
	if (mTree)
	{
		PatriciaFreeTree(mTree, NodeFreeCallback, (void *)this);
	}
}


#pragma mark -

/*----------------------------------------------------------------------------
	InsertNode
	
	Insert a node. Call the class's replace function by default.
	
	Returns true if replaced
----------------------------------------------------------------------------*/
Boolean CPatriciaTree::InsertNode(TPatriciaKey key, CPatriciaNode* nodeData)
{
	int	result = PatriciaInsert(mTree, NodeReplaceCallback, key, (void *)nodeData, (void *)this);
	return (result == 1);
}

/*----------------------------------------------------------------------------
	SeekNode
	
	Look for the node with the given key. Returns true if found
----------------------------------------------------------------------------*/
Boolean CPatriciaTree::SeekNode(TPatriciaKey key, CPatriciaNode**outNodeData)
{
	int	result = PatriciaSearch(mTree, key, (void **)outNodeData);
	return (result == 1);
}

/*----------------------------------------------------------------------------
	Traverse
	
	Traverse over every node in the tree. Returns true if traversed the entire
	tree without halting.
----------------------------------------------------------------------------*/
Boolean CPatriciaTree::Traverse(NodeTraverseFunction traverseFcn, void *arg, void *refCon)
{
	int	result = PatriciaTraverse(mTree, traverseFcn, arg, refCon);
	return (result == 0);
}


/*----------------------------------------------------------------------------
	GetNumNodes
	
	Get the number of entries in the tree
----------------------------------------------------------------------------*/
long CPatriciaTree::GetNumNodes()
{
	return (mTree) ? PatriciaGetNumNodes(mTree) : 0;
}


#pragma mark -

/*----------------------------------------------------------------------------
	ReplaceNode
	
	Detault replace node implementation. Does nothing.
	
----------------------------------------------------------------------------*/
int CPatriciaTree::ReplaceNode(CPatriciaNode**nodeDataPtr, TPatriciaKey key, CPatriciaNode *replaceData)
{
	return 0;
}

/*----------------------------------------------------------------------------
	FreeNode
	
	Detault free node implementation. Does nothing.
	
----------------------------------------------------------------------------*/
int CPatriciaTree::FreeNode(CPatriciaNode *nodeData, TPatriciaKey key)
{
	return 0;
}


#pragma mark -

/*----------------------------------------------------------------------------
	NodeReplaceCallback
	
	[static]
	
----------------------------------------------------------------------------*/
int CPatriciaTree::NodeReplaceCallback(void**nodeDataPtr, unsigned char *key, void *replaceData, void *refCon)
{
	CPatriciaTree*		theTree = reinterpret_cast<CPatriciaTree *>(refCon);
	Assert(theTree);
	return theTree->ReplaceNode((CPatriciaNode**)nodeDataPtr, key, static_cast<CPatriciaNode *>(replaceData));
}

/*----------------------------------------------------------------------------
	NodeTraverseCallback
	
	[static]
	
----------------------------------------------------------------------------*/
int CPatriciaTree::NodeFreeCallback(void *nodeData, unsigned char *key, void *refCon)
{
	CPatriciaTree*		theTree = reinterpret_cast<CPatriciaTree *>(refCon);
	Assert(theTree);
	return theTree->FreeNode(static_cast<CPatriciaNode *>(nodeData), key);
}

