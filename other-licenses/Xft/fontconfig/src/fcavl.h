/*
 * $XFree86: xc/lib/fontconfig/src/fcavl.h,v 1.1 2002/03/03 18:39:05 keithp Exp $
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

#ifndef _FCAVL_H_
#define _FCAVL_H_

/*
 * fcavl.h
 *
 * balanced binary tree
 */

typedef struct _FcAvlNode FcAvlNode;

struct _FcAvlNode {
    FcAvlNode	    *left, *right;
    short	    balance;
};

typedef FcBool (*FcAvlMore) (FcAvlNode *a, FcAvlNode *b);

FcBool	FcAvlInsert (FcAvlMore more, FcAvlNode **tree, FcAvlNode *leaf);
int	FcAvlDelete (FcAvlMore more, FcAvlNode **tree, FcAvlNode *leaf);

#endif /* _FCAVL_H_ */
