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


#ifndef __PATRICIA__
#define __PATRICIA__

/*---------------------------------------------------------------------

	Patricia
	
	D.R. Morrison's "Practical Algorithm To Retrieve Information
	Coded in Alphanumeric"
	
	See patricia.c for more information.
	
	Ref: Donald R. Morrison. PATRICIA -- practical algorithm to retrieve
	information coded in alphanumeric. Journal of the ACM, 15(4):514-534,
	October 1968

	See SedgeWick, R. Algorithms, 2nd Ed. (1988) for implemenation details.

-----------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/*	A type which you should use externally to refer to a Patricia tree. You
	application should treat the tree as an opaque data structure, accessed
	only through those routines whose prototypes are declared below.
	
	All externally-visible routines start with 'Patricia'. Internal routines
	are declared static in the .c file, and should not be used elsewhere.
*/
typedef void *PatriciaTreeRef;


/*	Declarations for user-supplied functions, which are called on replacing
	nodes (i.e. when inserting a node with a duplicate key, on traversing
	the tree, and on freeing nodes. A non-zero return value indicates failure.
*/
typedef int (*NodeReplaceFunction) (void* *nodeDataPtr, unsigned char *key, void *replaceData, void *refCon);
typedef int (*NodeTraverseFunction) (void *nodeData, unsigned char *key, void *arg, void *refCon);
typedef int (*NodeFreeFunction) (void *nodeData, unsigned char *key, void *refCon);

/*	The creation and destruction functions for allocating and freeing a tree.
	PatriciaInitTree() allocates space for a tree and initializes it.
	PatriciaFreeTree() frees the tree nodes and the tree itself. It does *NOT* free
	the data associated with each node, unless you pass a NodeFreeFunction that
	does this.
*/
PatriciaTreeRef PatriciaInitTree(long numKeyBits);
void	PatriciaFreeTree(PatriciaTreeRef treeRef, NodeFreeFunction freeFunc, void *refCon);

/*	Functions for inserting nodes, searching for a node with a given key,
	and traversing the tree. Traversal involves visiting every node in the tree,
	but does not guarantee that the nodes are visited in a particular order.
*/
int 	PatriciaInsert(PatriciaTreeRef treeRef, NodeReplaceFunction replaceFunc, const unsigned char *key, void *data, void *refCon);
int 	PatriciaSearch(PatriciaTreeRef treeRef, const unsigned char *key, void **data);
int 	PatriciaTraverse(PatriciaTreeRef treeRef, NodeTraverseFunction traverseFunc, void *arg, void *refCon);

/*	An accessor function that returns the total number of nodes in the tree
*/
long PatriciaGetNumNodes(PatriciaTreeRef treeRef);

#ifdef __cplusplus
}
#endif

#endif /* __PATRICIA__ */

