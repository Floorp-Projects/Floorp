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


/*---------------------------------------------------------------------

	Patricia
	
	D.R. Morrison's "Practical Algorithm To Retrieve Information
	Coded in Alphanumeric"
	
	This implementation geared towards the storage of data that
	is indexed through arbitrary-length binary keys (e.g. sequence
	data). It also only deals with the case where every key is unique;
	duplicate keys are not entered into the tree, although you may
	do some processing on duplicates in the ReplaceFunction passed
	to PatriciaInsert.
	
	There is no function to remove nodes from the tree at present.
	
	The implementation is modular, in the sense that all tree data structures
	are declared in this file, and are not intended to be accessed externally.
	You should only use the functions prototyped in patricia.h, all of which
	start with 'Patricia', and should consider the tree as an opaque data 
	structure.
	
	Ref: Donald R. Morrison. PATRICIA -- practical algorithm to retrieve
	information coded in alphanumeric. Journal of the ACM, 15(4):514-534,
	October 1968

	See Sedgewick, R. Algorithms, 2nd Ed. (1988) for implemenation details.

-----------------------------------------------------------------------*/

#include <string.h>
#include <stdlib.h>

//#include "utils.h"
#include "patricia.h"

#include "nsAEDefs.h"       // for AE_ASSERT

/* Uncomment to print verbose logging on tree activity */
//#define VERBOSE

/* Data structures */

typedef struct TNodeTag {
	unsigned char		*key;			/* The node's key value in an array of chars */
	short			bit;				/* The bit that is examined at this node */
	long				nodeID;			/* Unique ID for each node, used for debugging */
	struct TNodeTag	*left;			/* Pointer to left child node */
	struct TNodeTag	*right;			/* Pointer to right child node */
	void				*data;			/* Pointer to user-defined data structure */
} TNode;


typedef struct {
	TNode*			headNode;			/* Pointer to dummy head node */
	long				numKeyBits;		/* Size of keys, in bits */
	long				keySize;			/* Size of keys, in chars (assumed to be 8 bits each) */
	long				numNodes;		/* Number of nodes in tree */
} TPatriciaTree;


/* These macros assume that key is a *[unsigned] char, and that a char is 8 bits big */

#define TestKeyBit(key, bit)				( (key[bit >> 3] & (1 << (bit & 7))) != 0 )
#define CompareKeyBits(key1, key2, bit)		( (key1[bit >> 3] & (1 << (bit & 7))) == (key2[bit >> 3] & (1 << (bit & 7))) )

/*---------------------------------------------------------------------

	MakeNewNode
	
	Allocates a new node as an object on the heap, and initializes
	the key and bit values to those supplied.
	
	Entry:	key		binary key data
			bit		bit value
			nodeData	user data for this node, if any.
			
	Exit:		return value	pointer to the new node, or NULL on error
						
-----------------------------------------------------------------------*/

static TNode *MakeNewNode(TPatriciaTree *tree, const unsigned char *key, short bit, void *nodeData)
{
	static long		nodeID = 0;
	TNode		*newNode;
	
	newNode = (TNode *)calloc(1, sizeof(TNode));
	if (newNode == NULL) return NULL;

	newNode->key = (unsigned char *)calloc(tree->keySize + 1, 1);		//last bit must be zero
	if (newNode->key == NULL) {
		free(newNode);
		return NULL;
	}
	memcpy(newNode->key, key, tree->keySize);
	newNode->bit = bit;
	newNode->data = nodeData;
	newNode->nodeID = nodeID;
	nodeID ++;
	
	return newNode;
}


/*---------------------------------------------------------------------

	MakeHeadNode
	
	Allocates the dummy head node of the tree, which is initialized to
			key field = 000000...
			bits = numKeyBits
			left & right = point to self
			data = NULL
			
	Exit:		return value	pointer to the new node, or NULL on error
						
-----------------------------------------------------------------------*/

static TNode *MakeHeadNode(TPatriciaTree *tree)
{
	TNode		*newNode;
	
	newNode = (TNode *)calloc(1, sizeof(TNode));
	if (newNode == NULL) return NULL;

	newNode->key = (unsigned char *)calloc(tree->keySize + 1, 1);		//last bit must be zero
	if (newNode->key == NULL) {
		free(newNode);
		return NULL;
	}
	memset(newNode->key, 0, tree->keySize);
	newNode->bit = tree->numKeyBits;
	newNode->data = NULL;
	newNode->nodeID = -1;
	
	/* Both self-pointers for the head node */
	newNode->left = newNode;
	newNode->right = newNode;
	
	return newNode;
}


/*---------------------------------------------------------------------

	InternalSearch
	
	Search the tree for a node with the given key, starting at
	startNode.
	
	Entry:	tree		pointer to a tree
			key		binary key data
			startNode	node to start the search (must NOT be NULL)
			
	Exit:		return value:	pointer to found node, or node with upward
						link. The caller must verify whether this
						is the node wanted by comparing keys.
						
-----------------------------------------------------------------------*/

static TNode *InternalSearch(TPatriciaTree *tree, TNode *x, const unsigned char *key)
{
	TNode	*p;

	AE_ASSERT(x, "No node");

	do {
		p = x;
		
		if (TestKeyBit(key, x->bit))
			x = x->right;
		else
			x = x->left;
		
	} while (p->bit > x->bit);
	
	return x;
}



/*---------------------------------------------------------------------

	InternalTraverse
	
	A traverse routine used internally.
	
	Entry:	tree			pointer to a tree
			key			binary key data
			x			node to start the search (must NOT be NULL)
			traverseFunc	function called on each node, like this:
			
						(*traverseFunc)(x->data, x->key, arg1, arg2);
			
	Exit:		return value:	0 if everything OK
						Non-zero value on error
						
-----------------------------------------------------------------------*/

static int InternalTraverse(TPatriciaTree *tree, TNode *x, NodeTraverseFunction traverseFunc, void *arg1, void *arg2)
{
	TNode	*p;
	int		err = 0;
	
	AE_ASSERT(x, "No node");
	AE_ASSERT(x->left && x->right, "Left or right child missing");

#ifdef VERBOSE
	printf("Visiting node %ld with left %ld and right %ld\n", x->nodeID, x->left->nodeID, x->right->nodeID);
#endif

	if (x != tree->headNode) {
		err = (*traverseFunc)(x->data, x->key, arg1, arg2);
		if (err != 0) return err;
	}
	
	p = x->left;
	if (p->bit < x->bit)
		err = InternalTraverse(tree, p, traverseFunc, arg1, arg2);

	if (err != 0) return err;
	
	p = x->right;
	if (p->bit < x->bit)
		err = InternalTraverse(tree, p, traverseFunc, arg1, arg2);
	
	return err;
}



/*---------------------------------------------------------------------

	TraverseAndFree
	
	A traverse routine used internall.
	
	Entry:	tree			pointer to a tree
			key			binary key data
			x			node to start the search (must NOT be NULL)
			traverseFunc	function called on each node, like this:
			
						(*traverseFunc)(x->data, x->key, arg1, arg2);
			
	Exit:		return value:	0 if everything OK
						Non-zero value on error
						
-----------------------------------------------------------------------*/

static int TraverseAndFree(TPatriciaTree *tree, TNode *x, NodeFreeFunction freeFunc, void *refCon)
{
	TNode	*p;
	int		err = 0;
	
	AE_ASSERT(x, "No node");
	AE_ASSERT(x->left && x->right, "Left or right child missing");
		
	p = x->left;
	if (p->bit < x->bit) {
		err = TraverseAndFree(tree, p, freeFunc, refCon);
		if (err != 0) return err;
	}
	
	p = x->right;
	if (p->bit < x->bit) {
		err = TraverseAndFree(tree, p, freeFunc, refCon);
		if (err != 0) return err;
	}

	err = (*freeFunc)(x->data, x->key, refCon);
	
#ifdef VERBOSE
	printf("Freeing node %ld\n", x->nodeID);
#endif

	free(x->key);
	free(x);
	
	return err;
}


#pragma mark -


/*---------------------------------------------------------------------

	InitPatriciaTree
	
	Allocate and initialize a new, empty PatriciaTree.
	
	Entry:	keySize		length of the keys, in bits
			
	Exit:		return value	A reference to the created PatriciaTree, 
						or NULL on error.
						
-----------------------------------------------------------------------*/

PatriciaTreeRef PatriciaInitTree(long numKeyBits)
{
	TPatriciaTree		*tree = NULL;

	tree = (TPatriciaTree *)calloc(1, sizeof(TPatriciaTree));
	if (tree == NULL) return NULL;
	
	tree->numKeyBits = numKeyBits;
	tree->keySize = (numKeyBits >> 3) + ((numKeyBits & 7) != 0);
	tree->numNodes = 0;
	
	tree->headNode = MakeHeadNode(tree);
	if (tree->headNode == NULL) {
		free(tree);
		return NULL;
	}
	
	return (PatriciaTreeRef)tree;
}



/*---------------------------------------------------------------------

	PatriciaFreeTree
	
	Free a Patricia tree and all associate data structures. freeFunc is
	called on each node's node->data. If no freeFunc is supplied, then
	the node->data will NOT be freed.
	
	Entry:	treeRef		reference to a previously allocated PatriciaTree
			freeFunc		a function that is called on each node->data
			arg			pointer to data that is also passed to the free function.
			
	The free function is called like this:		(*freeFunc)(node->data, node->key, arg)
									
-----------------------------------------------------------------------*/

void PatriciaFreeTree(PatriciaTreeRef treeRef, NodeFreeFunction freeFunc, void *refCon)
{
	TPatriciaTree	*tree = (TPatriciaTree *)treeRef;

	if (tree == NULL) return;
	
	/* Traverse the tree and free the data */
	TraverseAndFree(tree, tree->headNode, freeFunc, refCon);
	
	free(tree);
}



/*---------------------------------------------------------------------

	PatriciaSearch
	
	Search the tree for the node with the given key.
	
	Entry:	key		pointer to binary key data
			data		pointer to placeholder for returned data
					If you pass NULL in this argument, no value
					will be returned (i.e. I don't deference 0).
			
	Exit:		return value	1 if key found (data returned in *data)
						0 if key not found (NULL returned in *data)
	
-----------------------------------------------------------------------*/

int PatriciaSearch(PatriciaTreeRef treeRef, const unsigned char *key, void **data)
{
	TPatriciaTree	*tree = (TPatriciaTree *)treeRef;
	TNode		*foundNode;
	
	AE_ASSERT(tree, "Where is my tree?");

	foundNode = InternalSearch(tree, tree->headNode, key);
	AE_ASSERT(foundNode, "Should have found node");

	if (memcmp(foundNode->key, key, tree->keySize) == 0) {
		if (data != NULL)
			*data = foundNode->data;
		return 1;
	} else
		return 0;
}



/*---------------------------------------------------------------------

	PatriciaInsert
	
	Insert a node into the tree with the given key and data.
		
	Entry:	key			pointer to binary key data
			replaceFunc	function executed when a node with the key we
						are inserting is found in the tree.
			data			pointer to node data structure
			
	Exit:		return value	0 if insertion successful
						1 if key already exists
						-1 if some other error

-----------------------------------------------------------------------*/

int PatriciaInsert(PatriciaTreeRef treeRef, NodeReplaceFunction replaceFunc, const unsigned char *key, void *data, void *refCon)
{
	TPatriciaTree	*tree = (TPatriciaTree *)treeRef;
	TNode		*x, *t, *p;
	short		i;

	x = tree->headNode;
	t = InternalSearch(tree, x, key);

	AE_ASSERT(t, "Should have found node");

	if (memcmp(t->key, key, tree->keySize) == 0) {
		if (replaceFunc) (*replaceFunc)(&t->data, t->key, data, refCon);
		return 1;			/* It's already there */
	}
	
	i = tree->numKeyBits - 1;
	
	while (CompareKeyBits(key, t->key, i))
		i --;	
		
	do {
		p = x;
		x = (TestKeyBit(key, x->bit)) ? x->right : x->left;
	} while (x->bit > i && p->bit > x->bit);
	
	t = MakeNewNode(tree, key, i, data);
	if (t == NULL) return -1;
	
	if (TestKeyBit(key, t->bit)) {
		t->right = t;
		t->left = x;
	} else {
		t->right = x;
		t->left = t;
	}
		
	if (TestKeyBit(key, p->bit))
		p->right = t;
	else
		p->left = t;
	
#ifdef VERBOSE
	printf("Inserted node %ld with left %ld and right %ld\n", t->nodeID, t->left->nodeID, t->right->nodeID);
#endif

	tree->numNodes ++;
	
	return 0;
}


/*---------------------------------------------------------------------

	PatriciaTraverse
	
	Traverse the tree, executing traverseFunc for each node. It's called like
	this:
		(*traverseFunc)(node->data, node->key, arg1, arg2);
		
	traverseFunc should return 0 if it completes successfully, or any other
	value if there is an error. In this case, the traverse is abruptly terminated.
	
	Entry:	treeRef		reference to a Patricia tree
			traverseFunc	function to be called on each node
			arg1, arg2	arguments passed to traverseFunc
		
	Exit:		return value	0 if traverse competed successfully
						-1 if something caused the traverse to
							terminate.

-----------------------------------------------------------------------*/

int PatriciaTraverse(PatriciaTreeRef treeRef, NodeTraverseFunction traverseFunc, void *arg, void *refCon)
{
	TPatriciaTree	*tree = (TPatriciaTree *)treeRef;
	
	return InternalTraverse(tree, tree->headNode, traverseFunc, arg, refCon);
}


/*---------------------------------------------------------------------

	PatriciaGetNumNodes
	
	Return the number of nodes in the tree
	
-----------------------------------------------------------------------*/

long PatriciaGetNumNodes(PatriciaTreeRef treeRef)
{
	return ((TPatriciaTree *)treeRef)->numNodes;
}
