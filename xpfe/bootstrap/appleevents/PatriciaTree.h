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


#ifndef PatricaTree_h_
#define PatricaTree_h_

#include "patricia.h"

// node base class. Subclass for your nodes.

class CPatriciaNode
{
public:
						CPatriciaNode()		{}
	virtual				~CPatriciaNode()	{}
};


// tree base class. Subclass for your tree.

class CPatriciaTree
{
public:
	
	typedef	const unsigned char*		TPatriciaKey;	
	
						CPatriciaTree(long keyBitsLen);
	virtual				~CPatriciaTree();
				
	// override if you want variable replace functionality
	// returns true if replaced
	virtual Boolean			InsertNode(TPatriciaKey key, CPatriciaNode* nodeData);
	
	// returns true if found
	Boolean				SeekNode(TPatriciaKey key, CPatriciaNode**outNodeData);
	
	// returns true if entire tree traversed
	Boolean				Traverse(NodeTraverseFunction traverseFcn, void *arg, void *refCon);
	
	long					GetNumNodes();
	
protected:

	// your implementation should override these.
	virtual int				ReplaceNode(CPatriciaNode**nodeDataPtr, TPatriciaKey key, CPatriciaNode *replaceData);
	virtual int				FreeNode(CPatriciaNode *nodeData, TPatriciaKey key);

private:

	// static callbacks.
	static int				NodeReplaceCallback(void**nodeDataPtr, unsigned char *key, void *replaceData, void *refCon);
	static int				NodeFreeCallback(void *nodeData, unsigned char *key, void *refCon);

protected:

	PatriciaTreeRef		mTree;
	long					mKeyBits;

};




#endif // PatricaTree_h_

