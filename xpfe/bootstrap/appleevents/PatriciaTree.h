/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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

