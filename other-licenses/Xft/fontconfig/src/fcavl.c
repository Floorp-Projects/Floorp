/*
 * $XFree86: xc/lib/fontconfig/src/fcavl.c,v 1.1 2002/03/03 18:39:05 keithp Exp $
 *
 * Copyright © 2002 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Semi-Balanced trees (avl).  This only contains two
 * routines - insert and delete.  Searching is
 * reserved for the client to write.
 */

#include	"fcint.h"
#include	"fcavl.h"

static FcBool
FcAvlRebalanceRight (FcAvlNode **);

static FcBool
FcAvlRebalanceLeft (FcAvlNode **);

/*
 * insert a new node
 *
 * this routine returns FcTrue if the tree has grown
 * taller
 */

FcBool
FcAvlInsert (FcAvlMore more, FcAvlNode **treep, FcAvlNode *new)
{
    if (!(*treep)) 
    {
	new->left = 0;
	new->right = 0;
	new->balance = 0;
	*treep = new;
	return FcTrue;
    } 
    else 
    {
	if ((*more) (new, *treep))
	{
	    if (FcAvlInsert (more, &(*treep)->right, new))
		switch (++(*treep)->balance) {
		case 0:
		    return FcFalse;
		case 1:
		    return FcTrue;
		case 2:
		    (void) FcAvlRebalanceRight (treep);
		}
	}
	else
	{
	    if (FcAvlInsert (more, &(*treep)->left, new))
		switch (--(*treep)->balance) {
		case 0:
		    return FcFalse;
		case -1:
		    return FcTrue;
		case -2:
		    (void) FcAvlRebalanceLeft (treep);
		}
	}
    }
    return 0;
}

/*
 * delete a node from a tree
 *
 * this routine return FcTrue if the tree has been shortened
 */

FcBool
FcAvlDelete (FcAvlMore more, FcAvlNode **treep, FcAvlNode *old)
{
    if (!*treep)
	return FcFalse;	/* node not found */
    if (old == *treep)
    {
	FcAvlNode	*replacement;
	FcAvlNode	*replacement_parent;
	int		replacement_direction;
	int		delete_direction;
	FcAvlNode	*swap_temp;
	int		balance_temp;

	/*
	 * find an empty down pointer (if any)
	 * and rehook the tree
	 */
	if (!old->right) {
	    (*treep) = old->left;
	    return FcTrue;
	} else if (!old->left) {
	    (*treep) = old->right;
	    return FcTrue;
	} else {
	    /* 
	     * if both down pointers are full, then
	     * move a node from the bottom of the tree up here.
	     *
	     * This builds an incorrect tree -- the replacement
	     * node and the old node will not
	     * be in correct order.  This doesn't matter as
	     * the old node will obviously not leave
	     * this routine alive.
	     */
	    /*
	     * if the tree is left heavy, then go left
	     * else go right
	     */
	    replacement_parent = old;
	    if (old->balance == -1) {
		delete_direction = -1;
		replacement_direction = -1;
		replacement = old->left;
		while (replacement->right) {
		    replacement_parent = replacement;
		    replacement_direction = 1;
		    replacement = replacement->right;
		}
	    } else {
		delete_direction = 1;
		replacement_direction = 1;
		replacement = old->right;
		while (replacement->left) {
		    replacement_parent = replacement;
		    replacement_direction = -1;
		    replacement = replacement->left;
		}
	    }
	    /*
	     * swap the replacement node into
	     * the tree where the node is to be removed
	     * 
	     * this would be faster if only the data
	     * element was swapped -- but that
	     * won't work for kalypso.  The alternate
	     * code would be:
	     data_temp = old->data;
	     to _be_deleted->data = replacement->data;
	     replacement->data = data_temp;
	     */
	    swap_temp = old->left;
	    old->left = replacement->left;
	    replacement->left = swap_temp;
	    
	    swap_temp = old->right;
	    old->right = replacement->right;
	    replacement->right = swap_temp;
	    
	    balance_temp = old->balance;
	    old->balance = replacement->balance;
	    replacement->balance = balance_temp;
	    /*
	     * if the replacement node is directly below
	     * the to-be-removed node, hook the old
	     * node below it (instead of below itself!)
	     */
	    if (replacement_parent == old)
		replacement_parent = replacement;
	    if (replacement_direction == -1)
		replacement_parent->left = old;
	    else
		replacement_parent->right = old;
	    (*treep) = replacement;
	    /*
	     * delete the node from the sub-tree
	     */
	    if (delete_direction == -1) 
	    {
		if (FcAvlDelete (more, &(*treep)->left, old)) 
		{
		    switch (++(*treep)->balance) {
		    case 2:
			abort ();
		    case 1:
			return FcFalse;
		    case 0:
			return FcTrue;
		    }
		}
		return 0;
	    }
	    else 
	    {
		if (FcAvlDelete (more, &(*treep)->right, old)) 
		{
		    switch (--(*treep)->balance) {
		    case -2:
			abort ();
		    case -1:
			return FcFalse;
		    case 0:
			return FcTrue;
		    }
		}
		return 0;
	    }
	}
    }
    else if ((*more) (old, *treep))
    {
	if (FcAvlDelete (more, &(*treep)->right, old))
	{
	    /*
	     * check the balance factors
	     * Note that the conditions are
	     * inverted from the insertion case
	     */
	    switch (--(*treep)->balance) {
	    case 0:
		return FcFalse;
	    case -1:
		return FcTrue;
	    case -2:
		return FcAvlRebalanceLeft (treep);
	    }
	}
	return 0;
    }
    else 
    {
	if (FcAvlDelete (more, &(*treep)->left, old))
	{
	    switch (++(*treep)->balance) {
	    case 0:
		return FcTrue;
	    case 1:
		return FcFalse;
	    case 2:
		return FcAvlRebalanceRight (treep);
	    }
	}
	return 0;
    }
    /*NOTREACHED*/
}

/*
 * two routines to rebalance the tree.
 *
 * rebalance_right -- the right sub-tree is too long
 * rebalance_left --  the left sub-tree is too long
 *
 * These routines are the heart of avl trees, I've tried
 * to make their operation reasonably clear with comments,
 * but some study will be necessary to understand the
 * algorithm.
 *
 * these routines return FcTrue if the resultant
 * tree is shorter than the un-balanced version.  This
 * is only of interest to the delete routine as the
 * balance after insertion can never actually shorten
 * the tree.
 */

static FcBool
FcAvlRebalanceRight (FcAvlNode **treep)
{
    FcAvlNode	*temp;
    /*
     * rebalance the tree
     */
    if ((*treep)->right->balance == -1) {
	/* 
	 * double whammy -- the inner sub-sub tree
	 * is longer than the outer sub-sub tree
	 *
	 * this is the "double rotation" from
	 * knuth.  Scheme:  replace the tree top node
	 * with the inner sub-tree top node and
	 * adjust the maze of pointers and balance
	 * factors accordingly.
	 */
	temp = (*treep)->right->left;
	(*treep)->right->left = temp->right;
	temp->right = (*treep)->right;
	switch (temp->balance) {
	case -1:
	    temp->right->balance = 1;
	    (*treep)->balance = 0;
	    break;
	case 0:
	    temp->right->balance = 0;
	    (*treep)->balance = 0;
	    break;
	case 1:
	    temp->right->balance = 0;
	    (*treep)->balance = -1;
	    break;
	}
	temp->balance = 0;
	(*treep)->right = temp->left;
	temp->left = (*treep);
	(*treep) = temp;
	return FcTrue;
    } else {
	/*
	 * a simple single rotation
	 *
	 * Scheme:  replace the tree top node
	 * with the sub-tree top node 
	 */
	temp = (*treep)->right->left;
	(*treep)->right->left = (*treep);
	(*treep) = (*treep)->right;
	(*treep)->left->right = temp;
	/*
	 * only two possible configurations --
	 * if the right sub-tree was balanced, then
	 * *both* sides of it were longer than the
	 * left side, so the resultant tree will
	 * have a long leg (the left inner leg being
			    * the same length as the right leg)
	 */
	if ((*treep)->balance == 0) {
	    (*treep)->balance = -1;
	    (*treep)->left->balance = 1;
	    return FcFalse;
	} else {
	    (*treep)->balance = 0;
	    (*treep)->left->balance = 0;
	    return FcTrue;
	}
    }
}

static FcBool
FcAvlRebalanceLeft (FcAvlNode **treep)
{
    FcAvlNode	*temp;
    /*
     * rebalance the tree
     */
    if ((*treep)->left->balance == 1) {
	/* 
	 * double whammy -- the inner sub-sub tree
	 * is longer than the outer sub-sub tree
	 *
	 * this is the "double rotation" from
	 * knuth.  Scheme:  replace the tree top node
	 * with the inner sub-tree top node and
	 * adjust the maze of pointers and balance
	 * factors accordingly.
	 */
	temp = (*treep)->left->right;
	(*treep)->left->right = temp->left;
	temp->left = (*treep)->left;
	switch (temp->balance) {
	case 1:
	    temp->left->balance = -1;
	    (*treep)->balance = 0;
	    break;
	case 0:
	    temp->left->balance = 0;
	    (*treep)->balance = 0;
	    break;
	case -1:
	    temp->left->balance = 0;
	    (*treep)->balance = 1;
	    break;
	}
	temp->balance = 0;
	(*treep)->left = temp->right;
	temp->right = (*treep);
	(*treep) = temp;
	return FcTrue;
    } else {
	/*
	 * a simple single rotation
	 *
	 * Scheme:  replace the tree top node
	 * with the sub-tree top node 
	 */
	temp = (*treep)->left->right;
	(*treep)->left->right = (*treep);
	(*treep) = (*treep)->left;
	(*treep)->right->left = temp;
	/*
	 * only two possible configurations --
	 * if the left sub-tree was balanced, then
	 * *both* sides of it were longer than the
	 * right side, so the resultant tree will
	 * have a long leg (the right inner leg being
         * the same length as the left leg)
	 */
	if ((*treep)->balance == 0) {
	    (*treep)->balance = 1;
	    (*treep)->right->balance = -1;
	    return FcTrue;
	} else {
	    (*treep)->balance = 0;
	    (*treep)->right->balance = 0;
	    return FcFalse;
	}
    }
}
