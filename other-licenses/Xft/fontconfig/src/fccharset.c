/*
 * $XFree86: xc/lib/fontconfig/src/fccharset.c,v 1.4 2002/02/19 07:50:43 keithp Exp $
 *
 * Copyright © 2001 Keith Packard, member of The XFree86 Project, Inc.
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

#include <stdlib.h>
#include "fcint.h"

/* #define CHECK */

static int
FcCharSetLevels (FcChar32 ucs4)
{
    if (ucs4 <= 0xff)
	return 1;
    if (ucs4 <= 0xffff)
	return 2;
    if (ucs4 <= 0xffffff)
	return 3;
    return 4;
}

static FcBool
FcCharSetCheckLevel (FcCharSet *fcs, FcChar32 ucs4)
{
    int	level = FcCharSetLevels (ucs4);
    
    if (level <= fcs->levels)
	return FcTrue;
    while (fcs->levels < level)
    {
	if (fcs->levels == 0)
	{
	    FcCharLeaf	    *leaf;

	    leaf = (FcCharLeaf *) calloc (1, sizeof (FcCharLeaf));
	    if (!leaf)
		return FcFalse;
	    FcMemAlloc (FC_MEM_CHARNODE, sizeof (FcCharLeaf));
	    fcs->node.leaf = leaf;
	}
	else
	{
	    FcCharBranch    *branch;
    
	    branch = (FcCharBranch *) calloc (1, sizeof (FcCharBranch));
	    if (!branch)
		return FcFalse;
	    FcMemAlloc (FC_MEM_CHARNODE, sizeof (FcCharBranch));
	    branch->nodes[0] = fcs->node;
	    fcs->node.branch = branch;
	}
	++fcs->levels;
    }
    return FcTrue;
}

FcCharSet *
FcCharSetCreate (void)
{
    FcCharSet	*fcs;

    fcs = (FcCharSet *) malloc (sizeof (FcCharSet));
    if (!fcs)
	return 0;
    FcMemAlloc (FC_MEM_CHARSET, sizeof (FcCharSet));
    fcs->ref = 1;
    fcs->levels = 0;
    fcs->node.leaf = 0;
    fcs->constant = FcFalse;
    return fcs;
}

FcCharSet *
FcCharSetNew (void);
    
FcCharSet *
FcCharSetNew (void)
{
    return FcCharSetCreate ();
}

static void
FcCharNodeDestroy (FcCharNode node, int level)
{
    int	i;

    switch (level) {
    case 0:
	break;
    case 1:
	FcMemFree (FC_MEM_CHARNODE, sizeof (FcCharLeaf));
	free (node.leaf);
	break;
    default:
	for (i = 0; i < 256; i++)
	    if (node.branch->nodes[i].branch)
		FcCharNodeDestroy (node.branch->nodes[i], level - 1);
        FcMemFree (FC_MEM_CHARNODE, sizeof (FcCharBranch));
	free (node.branch);
    }
}

void
FcCharSetDestroy (FcCharSet *fcs)
{
    if (fcs->constant)
	return;
    if (--fcs->ref <= 0)
    {
	FcCharNodeDestroy (fcs->node, fcs->levels);
	FcMemFree (FC_MEM_CHARSET, sizeof (FcCharSet));
	free (fcs);
    }
}

/*
 * Locate the leaf containing the specified char, returning
 * null if it doesn't exist
 */

static FcCharLeaf *
FcCharSetFindLeaf (const FcCharSet *fcs, FcChar32 ucs4)
{
    int			l;
    const FcCharNode	*prev;
    FcCharNode		node;
    FcChar32    	i;

    prev = &fcs->node;
    l = fcs->levels;
    while (--l > 0)
    {
	node = *prev;
	if (!node.branch)
	    return 0;
	i = (ucs4 >> (l << 3)) & 0xff;
	prev = &node.branch->nodes[i];
    }
    return prev->leaf;
}

/*
 * Locate the leaf containing the specified char, creating it
 * if desired
 */

static FcCharLeaf *
FcCharSetFindLeafCreate (FcCharSet *fcs, FcChar32 ucs4)
{
    int		    l;
    FcCharNode	    *prev, node;
    FcChar32	    i;

    if (!FcCharSetCheckLevel (fcs, ucs4))
	return FcFalse;
    prev = &fcs->node;
    l = fcs->levels;
    while (--l > 0)
    {
	node = *prev;
	if (!node.branch)
	{
	    node.branch = calloc (1, sizeof (FcCharBranch));
	    if (!node.branch)
		return 0;
	    FcMemAlloc (FC_MEM_CHARNODE, sizeof (FcCharBranch));
	    *prev = node;
	}
	i = (ucs4 >> (l << 3)) & 0xff;
	prev = &node.branch->nodes[i];
    }
    node = *prev;
    if (!node.leaf)
    {
	node.leaf = calloc (1, sizeof (FcCharLeaf));
	if (!node.leaf)
	    return 0;
	FcMemAlloc (FC_MEM_CHARNODE, sizeof (FcCharLeaf));
	*prev = node;
    }
    return node.leaf;
}

FcBool
FcCharSetAddChar (FcCharSet *fcs, FcChar32 ucs4)
{
    FcCharLeaf	*leaf;
    FcChar32	*b;
    
    if (fcs->constant)
	return FcFalse;
    leaf = FcCharSetFindLeafCreate (fcs, ucs4);
    if (!leaf)
	return FcFalse;
    b = &leaf->map[(ucs4 & 0xff) >> 5];
    *b |= (1 << (ucs4 & 0x1f));
    return FcTrue;
}

/*
 * An iterator for the leaves of a charset
 */

typedef struct _fcCharSetIter {
    FcCharLeaf	    *leaf;
    FcChar32	    ucs4;
} FcCharSetIter;

/*
 * Find the nearest leaf at or beyond *ucs4, return 0 if no leaf
 * exists
 */
static FcCharLeaf *
FcCharSetIterLeaf (FcCharNode node, int level, FcChar32 *ucs4)
{
    if (level <= 1)
        return node.leaf;
    else if (!node.branch)
	return 0;
    else
    {
	int	    shift = ((level - 1) << 3);
	FcChar32    inc = 1 << shift;
	FcChar32    mask = ~(inc - 1);
	FcChar32    byte = (*ucs4 >> shift) & 0xff;
	FcCharLeaf  *leaf;

	for (;;)
	{
	    leaf = FcCharSetIterLeaf (node.branch->nodes[byte], 
				      level - 1, 
				      ucs4);
	    if (leaf)
		break;
	    /* step to next branch, resetting lower indices */
	    *ucs4 = (*ucs4 & mask) + inc;
	    byte = (byte + 1) & 0xff;
	    if (byte == 0)
		break;
	}
	return leaf;
    }
}

static void
FcCharSetIterSet (const FcCharSet *fcs, FcCharSetIter *iter)
{
    if (FcCharSetLevels (iter->ucs4) > fcs->levels)
	iter->leaf = 0;
    else
	iter->leaf = FcCharSetIterLeaf (fcs->node, fcs->levels,
					&iter->ucs4);
    if (!iter->leaf)
	iter->ucs4 = ~0;
}

static void
FcCharSetIterNext (const FcCharSet *fcs, FcCharSetIter *iter)
{
    iter->ucs4 += 0x100;
    FcCharSetIterSet (fcs, iter);
}

static void
FcCharSetIterStart (const FcCharSet *fcs, FcCharSetIter *iter)
{
    iter->ucs4 = 0;
    FcCharSetIterSet (fcs, iter);
}

FcCharSet *
FcCharSetCopy (FcCharSet *src)
{
    src->ref++;
    return src;
}

FcBool
FcCharSetEqual (const FcCharSet *a, const FcCharSet *b)
{
    FcCharSetIter   ai, bi;
    int		    i;
    
    if (a == b)
	return FcTrue;
    for (FcCharSetIterStart (a, &ai), FcCharSetIterStart (b, &bi);
	 ai.leaf && bi.leaf;
	 FcCharSetIterNext (a, &ai), FcCharSetIterNext (b, &bi))
    {
	if (ai.ucs4 != bi.ucs4)
	    return FcFalse;
	for (i = 0; i < 256/32; i++)
	    if (ai.leaf->map[i] != bi.leaf->map[i])
		return FcFalse;
    }
    return ai.leaf == bi.leaf;
}

static FcBool
FcCharSetAddLeaf (FcCharSet	*fcs,
		  FcChar32	ucs4,
		  FcCharLeaf	*leaf)
{
    FcCharLeaf   *new = FcCharSetFindLeafCreate (fcs, ucs4);
    if (!new)
	return FcFalse;
    *new = *leaf;
    return FcTrue;
}

static FcCharSet *
FcCharSetOperate (const FcCharSet   *a,
		  const FcCharSet   *b,
		  FcBool	    (*overlap) (FcCharLeaf	    *result,
						const FcCharLeaf    *al,
						const FcCharLeaf    *bl),
		  FcBool	aonly,
		  FcBool	bonly)
{
    FcCharSet	    *fcs;
    FcCharSetIter   ai, bi;

    fcs = FcCharSetCreate ();
    if (!fcs)
	goto bail0;
    FcCharSetIterStart (a, &ai);
    FcCharSetIterStart (b, &bi);
    while ((ai.leaf || bonly) && (bi.leaf || aonly))
    {
	if (ai.ucs4 < bi.ucs4)
	{
	    if (aonly)
	    {
		if (!FcCharSetAddLeaf (fcs, ai.ucs4, ai.leaf))
		    goto bail1;
		FcCharSetIterNext (a, &ai);
	    }
	    else
	    {
		ai.ucs4 = bi.ucs4;
		FcCharSetIterSet (a, &ai);
	    }
	}
	else if (bi.ucs4 < ai.ucs4 )
	{
	    if (bonly)
	    {
		if (!FcCharSetAddLeaf (fcs, bi.ucs4, bi.leaf))
		    goto bail1;
		FcCharSetIterNext (b, &bi);
	    }
	    else
	    {
		bi.ucs4 = ai.ucs4;
		FcCharSetIterSet (b, &bi);
	    }
	}
	else
	{
	    FcCharLeaf  leaf;

	    if ((*overlap) (&leaf, ai.leaf, bi.leaf))
	    {
		if (!FcCharSetAddLeaf (fcs, ai.ucs4, &leaf))
		    goto bail1;
	    }
	    FcCharSetIterNext (a, &ai);
	    FcCharSetIterNext (b, &bi);
	}
    }
    return fcs;
bail1:
    FcCharSetDestroy (fcs);
bail0:
    return 0;
}

static FcBool
FcCharSetIntersectLeaf (FcCharLeaf *result,
			const FcCharLeaf *al,
			const FcCharLeaf *bl)
{
    int	    i;
    FcBool  nonempty = FcFalse;

    for (i = 0; i < 256/32; i++)
	if ((result->map[i] = al->map[i] & bl->map[i]))
	    nonempty = FcTrue;
    return nonempty;
}

FcCharSet *
FcCharSetIntersect (const FcCharSet *a, const FcCharSet *b)
{
    return FcCharSetOperate (a, b, FcCharSetIntersectLeaf, FcFalse, FcFalse);
}

static FcBool
FcCharSetUnionLeaf (FcCharLeaf *result,
		    const FcCharLeaf *al,
		    const FcCharLeaf *bl)
{
    int	i;

    for (i = 0; i < 256/32; i++)
	result->map[i] = al->map[i] | bl->map[i];
    return FcTrue;
}

FcCharSet *
FcCharSetUnion (const FcCharSet *a, const FcCharSet *b)
{
    return FcCharSetOperate (a, b, FcCharSetUnionLeaf, FcTrue, FcTrue);
}

static FcBool
FcCharSetSubtractLeaf (FcCharLeaf *result,
		       const FcCharLeaf *al,
		       const FcCharLeaf *bl)
{
    int	    i;
    FcBool  nonempty = FcFalse;

    for (i = 0; i < 256/32; i++)
	if ((result->map[i] = al->map[i] & ~bl->map[i]))
	    nonempty = FcTrue;
    return nonempty;
}

FcCharSet *
FcCharSetSubtract (const FcCharSet *a, const FcCharSet *b)
{
    return FcCharSetOperate (a, b, FcCharSetSubtractLeaf, FcTrue, FcFalse);
}

FcBool
FcCharSetHasChar (const FcCharSet *fcs, FcChar32 ucs4)
{
    FcCharLeaf	*leaf = FcCharSetFindLeaf (fcs, ucs4);
    if (!leaf)
	return FcFalse;
    return (leaf->map[(ucs4 & 0xff) >> 5] & (1 << (ucs4 & 0x1f))) != 0;
}

static FcChar32
FcCharSetPopCount (FcChar32 c1)
{
    /* hackmem 169 */
    FcChar32	c2 = (c1 >> 1) & 033333333333;
    c2 = c1 - c2 - ((c2 >> 1) & 033333333333);
    return (((c2 + (c2 >> 3)) & 030707070707) % 077);
}

FcChar32
FcCharSetIntersectCount (const FcCharSet *a, const FcCharSet *b)
{
    FcCharSetIter   ai, bi;
    FcChar32	    count = 0;
    
    FcCharSetIterStart (a, &ai);
    FcCharSetIterStart (b, &bi);
    while (ai.leaf && bi.leaf)
    {
	if (ai.ucs4 == bi.ucs4)
	{
	    FcChar32	*am = ai.leaf->map;
	    FcChar32	*bm = bi.leaf->map;
	    int		i = 256/32;
	    while (i--)
		count += FcCharSetPopCount (*am++ & *bm++);
	    FcCharSetIterNext (a, &ai);
	} 
	else if (ai.ucs4 < bi.ucs4)
	{
	    ai.ucs4 = bi.ucs4;
	    FcCharSetIterSet (a, &ai);
	}
	if (bi.ucs4 < ai.ucs4)
	{
	    bi.ucs4 = ai.ucs4;
	    FcCharSetIterSet (b, &bi);
	}
    }
    return count;
}

FcChar32
FcCharSetCount (const FcCharSet *a)
{
    FcCharSetIter   ai;
    FcChar32	    count = 0;
    
    for (FcCharSetIterStart (a, &ai); ai.leaf; FcCharSetIterNext (a, &ai))
    {
	int		    i = 256/32;
	FcChar32	    *am = ai.leaf->map;

	while (i--)
	    count += FcCharSetPopCount (*am++);
    }
    return count;
}

FcChar32
FcCharSetSubtractCount (const FcCharSet *a, const FcCharSet *b)
{
    FcCharSetIter   ai, bi;
    FcChar32	    count = 0;
    
    FcCharSetIterStart (a, &ai);
    FcCharSetIterStart (b, &bi);
    while (ai.leaf)
    {
	if (ai.ucs4 <= bi.ucs4)
	{
	    FcChar32	*am = ai.leaf->map;
	    int		i = 256/32;
	    if (ai.ucs4 == bi.ucs4)
	    {
		FcChar32	*bm = bi.leaf->map;;
		while (i--)
		    count += FcCharSetPopCount (*am++ & ~*bm++);
	    }
	    else
	    {
		while (i--)
		    count += FcCharSetPopCount (*am++);
	    }
	    FcCharSetIterNext (a, &ai);
	}
	else if (bi.leaf)
	{
	    bi.ucs4 = ai.ucs4;
	    FcCharSetIterSet (b, &bi);
	}
    }
    return count;
}

FcChar32
FcCharSetCoverage (const FcCharSet *a, FcChar32 page, FcChar32 *result)
{
    FcCharSetIter   ai;

    ai.ucs4 = page;
    FcCharSetIterSet (a, &ai);
    if (!ai.leaf)
    {
	memset (result, '\0', 256 / 8);
	page = 0;
    }
    else
    {
	memcpy (result, ai.leaf->map, sizeof (ai.leaf->map));
	FcCharSetIterNext (a, &ai);
	page = ai.ucs4;
    }
    return page;
}

/*
 * ASCII representation of charsets.
 * 
 * Each leaf is represented as 9 32-bit values, the code of the first character followed
 * by 8 32 bit values for the leaf itself.  Each value is encoded as 5 ASCII characters,
 * only 85 different values are used to avoid control characters as well as the other
 * characters used to encode font names.  85**5 > 2^32 so things work out, but
 * it's not exactly human readable output.  As a special case, 0 is encoded as a space
 */

static unsigned char	charToValue[256] = {
    /*     "" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /*   "\b" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\020" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\030" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /*    " " */ 0xff,  0x00,  0xff,  0x01,  0x02,  0x03,  0x04,  0xff, 
    /*    "(" */ 0x05,  0x06,  0x07,  0x08,  0xff,  0xff,  0x09,  0x0a, 
    /*    "0" */ 0x0b,  0x0c,  0x0d,  0x0e,  0x0f,  0x10,  0x11,  0x12, 
    /*    "8" */ 0x13,  0x14,  0xff,  0x15,  0x16,  0xff,  0x17,  0x18, 
    /*    "@" */ 0x19,  0x1a,  0x1b,  0x1c,  0x1d,  0x1e,  0x1f,  0x20, 
    /*    "H" */ 0x21,  0x22,  0x23,  0x24,  0x25,  0x26,  0x27,  0x28, 
    /*    "P" */ 0x29,  0x2a,  0x2b,  0x2c,  0x2d,  0x2e,  0x2f,  0x30, 
    /*    "X" */ 0x31,  0x32,  0x33,  0x34,  0xff,  0x35,  0x36,  0xff, 
    /*    "`" */ 0xff,  0x37,  0x38,  0x39,  0x3a,  0x3b,  0x3c,  0x3d, 
    /*    "h" */ 0x3e,  0x3f,  0x40,  0x41,  0x42,  0x43,  0x44,  0x45, 
    /*    "p" */ 0x46,  0x47,  0x48,  0x49,  0x4a,  0x4b,  0x4c,  0x4d, 
    /*    "x" */ 0x4e,  0x4f,  0x50,  0x51,  0x52,  0x53,  0x54,  0xff, 
    /* "\200" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\210" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\220" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\230" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\240" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\250" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\260" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\270" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\300" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\310" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\320" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\330" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\340" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\350" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\360" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\370" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
};

static FcChar8 valueToChar[0x55] = {
    /* 0x00 */ '!', '#', '$', '%', '&', '(', ')', '*',
    /* 0x08 */ '+', '.', '/', '0', '1', '2', '3', '4',
    /* 0x10 */ '5', '6', '7', '8', '9', ';', '<', '>',
    /* 0x18 */ '?', '@', 'A', 'B', 'C', 'D', 'E', 'F',
    /* 0x20 */ 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    /* 0x28 */ 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    /* 0x30 */ 'W', 'X', 'Y', 'Z', '[', ']', '^', 'a',
    /* 0x38 */ 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
    /* 0x40 */ 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q',
    /* 0x48 */ 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
    /* 0x50 */ 'z', '{', '|', '}', '~',
};

static FcChar8 *
FcCharSetParseValue (FcChar8 *string, FcChar32 *value)
{
    int		i;
    FcChar32	v;
    FcChar32	c;
    
    if (*string == ' ')
    {
	v = 0;
	string++;
    }
    else
    {
	v = 0;
	for (i = 0; i < 5; i++)
	{
	    if (!(c = (FcChar32) (unsigned char) *string++))
		return 0;
	    c = charToValue[c];
	    if (c == 0xff)
		return 0;
	    v = v * 85 + c;
	}
    }
    *value = v;
    return string;
}

static FcBool
FcCharSetUnparseValue (FcStrBuf *buf, FcChar32 value)
{
    int	    i;
    if (value == 0)
    {
	return FcStrBufChar (buf, ' ');
    }
    else
    {
	FcChar8	string[6];
	FcChar8	*s = string + 5;
	string[5] = '\0';
	for (i = 0; i < 5; i++)
	{
	    *--s = valueToChar[value % 85];
	    value /= 85;
	}
	for (i = 0; i < 5; i++)
	    if (!FcStrBufChar (buf, *s++))
		return FcFalse;
    }
    return FcTrue;
}

FcCharSet *
FcNameParseCharSet (FcChar8 *string)
{
    FcCharSet	*c;
    FcChar32	ucs4;
    FcCharLeaf	*leaf;
    int		i;

    c = FcCharSetCreate ();
    if (!c)
	goto bail0;
    while (*string)
    {
	string = FcCharSetParseValue (string, &ucs4);
	if (!string)
	    goto bail1;
	leaf = FcCharSetFindLeafCreate (c, ucs4);
	if (!leaf)
	    goto bail1;
	for (i = 0; i < 256/32; i++)
	{
	    string = FcCharSetParseValue (string, &leaf->map[i]);
	    if (!string)
		goto bail1;
	}
    }
    return c;
bail1:
    FcCharSetDestroy (c);
bail0:
    return 0;
}

FcBool
FcNameUnparseCharSet (FcStrBuf *buf, const FcCharSet *c)
{
    FcCharSetIter   ci;
    int		    i;
#ifdef CHECK
    int		    len = buf->len;
#endif

    for (FcCharSetIterStart (c, &ci);
	 ci.leaf;
	 FcCharSetIterNext (c, &ci))
    {
	if (!FcCharSetUnparseValue (buf, ci.ucs4))
	    return FcFalse;
	for (i = 0; i < 256/32; i++)
	    if (!FcCharSetUnparseValue (buf, ci.leaf->map[i]))
		return FcFalse;
    }
#ifdef CHECK
    {
	FcCharSet	*check;
	FcChar32	missing;
	FcCharSetIter	ci, checki;
	
	/* null terminate for parser */
	FcStrBufChar (buf, '\0');
	/* step back over null for life after test */
	buf->len--;
	check = FcNameParseCharSet (buf->buf + len);
	FcCharSetIterStart (c, &ci);
	FcCharSetIterStart (check, &checki);
	while (ci.leaf || checki.leaf)
	{
	    if (ci.ucs4 < checki.ucs4)
	    {
		printf ("Missing leaf node at 0x%x\n", ci.ucs4);
		FcCharSetIterNext (c, &ci);
	    }
	    else if (checki.ucs4 < ci.ucs4)
	    {
		printf ("Extra leaf node at 0x%x\n", checki.ucs4);
		FcCharSetIterNext (check, &checki);
	    }
	    else
	    {
		int	    i = 256/32;
		FcChar32    *cm = ci.leaf->map;
		FcChar32    *checkm = checki.leaf->map;

		for (i = 0; i < 256; i += 32)
		{
		    if (*cm != *checkm)
			printf ("Mismatching sets at 0x%08x: 0x%08x != 0x%08x\n",
				ci.ucs4 + i, *cm, *checkm);
		    cm++;
		    checkm++;
		}
		FcCharSetIterNext (c, &ci);
		FcCharSetIterNext (check, &checki);
	    }
	}
	if ((missing = FcCharSetSubtractCount (c, check)))
	    printf ("%d missing in reparsed result\n", missing);
	if ((missing = FcCharSetSubtractCount (check, c)))
	    printf ("%d extra in reparsed result\n", missing);
	FcCharSetDestroy (check);
    }
#endif
    
    return FcTrue;
}

#include <freetype/freetype.h>
#include <fontconfig/fcfreetype.h>

/*
 * Figure out whether the available freetype has FT_Get_Next_Char
 */

#if FREETYPE_MAJOR > 2
# define HAS_NEXT_CHAR
#else
# if FREETYPE_MAJOR == 2
#  if FREETYPE_MINOR > 0
#   define HAS_NEXT_CHAR
#  else
#   if FREETYPE_MINOR == 0
#    if FREETYPE_PATCH >= 8
#     define HAS_NEXT_CHAR
#    endif
#   endif
#  endif
# endif
#endif

/*
 * For our purposes, this approximation is sufficient
 */
#ifndef HAS_NEXT_CHAR
#define FT_Get_Next_Char(face, ucs4) ((ucs4) >= 0xffffff ? 0 : (ucs4) + 1)
#warning "No FT_Get_Next_Char"
#endif

typedef struct _FcCharEnt {
    FcChar16	    bmp;
    unsigned char   encode;
} FcCharEnt;

typedef struct _FcCharMap {
    const FcCharEnt *ent;
    int		    nent;
} FcCharMap;

typedef struct _FcFontDecode {
    FT_Encoding	    encoding;
    const FcCharMap *map;
    FcChar32	    max;
} FcFontDecode;

static const FcCharMap AppleRoman;
static const FcCharMap AdobeSymbol;
    
static const FcFontDecode fcFontDecoders[] = {
    { ft_encoding_unicode,	0,		(1 << 21) - 1 },
    { ft_encoding_symbol,	&AdobeSymbol,	(1 << 16) - 1 },
    { ft_encoding_apple_roman,	&AppleRoman,	(1 << 16) - 1 },
};

#define NUM_DECODE  (sizeof (fcFontDecoders) / sizeof (fcFontDecoders[0]))

static FT_ULong
FcFreeTypeMapChar (FcChar32 ucs4, const FcCharMap *map)
{
    int		low, high, mid;
    FcChar16	bmp;

    low = 0;
    high = map->nent - 1;
    if (ucs4 < map->ent[low].bmp || map->ent[high].bmp < ucs4)
	return ~0;
    while (high - low > 1)
    {
	mid = (high + low) >> 1;
	bmp = map->ent[mid].bmp;
	if (ucs4 == bmp)
	    return (FT_ULong) map->ent[mid].encode;
	if (ucs4 < bmp)
	    high = mid;
	else
	    low = mid;
    }
    for (mid = low; mid <= high; mid++)
    {
	if (ucs4 == map->ent[mid].bmp)
	    return (FT_ULong) map->ent[mid].encode;
    }
    return ~0;
}

/*
 * Map a UCS4 glyph to a glyph index.  Use all available encoding
 * tables to try and find one that works.  This information is expected
 * to be cached by higher levels, so performance isn't critical
 */

FT_UInt
FcFreeTypeCharIndex (FT_Face face, FcChar32 ucs4)
{
    int		    initial, offset, decode;
    FT_UInt	    glyphindex;
    FT_ULong	    charcode;

    initial = 0;
    /*
     * Find the current encoding
     */
    if (face->charmap)
    {
	for (; initial < NUM_DECODE; initial++)
	    if (fcFontDecoders[initial].encoding == face->charmap->encoding)
		break;
	if (initial == NUM_DECODE)
	    initial = 0;
    }
    /*
     * Check each encoding for the glyph, starting with the current one
     */
    for (offset = 0; offset < NUM_DECODE; offset++)
    {
	decode = (initial + offset) % NUM_DECODE;
	if (!face->charmap || face->charmap->encoding != fcFontDecoders[decode].encoding)
	    if (FT_Select_Charmap (face, fcFontDecoders[decode].encoding) != 0)
		continue;
	if (fcFontDecoders[decode].map)
	{
	    charcode = FcFreeTypeMapChar (ucs4, fcFontDecoders[decode].map);
	    if (charcode == ~0)
		continue;
	}
	else
	    charcode = (FT_ULong) ucs4;
	glyphindex = FT_Get_Char_Index (face, charcode);
	if (glyphindex)
	    return glyphindex;
    }
    return 0;
}

static FcBool
FcFreeTypeCheckGlyph (FT_Face face, FcChar32 ucs4, 
		      FT_UInt glyph, FcBlanks *blanks)
{
    FT_Int	    load_flags = FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING;
    FT_GlyphSlot    slot;
    
    /*
     * When using scalable fonts, only report those glyphs
     * which can be scaled; otherwise those fonts will
     * only be available at some sizes, and never when
     * transformed.  Avoid this by simply reporting bitmap-only
     * glyphs as missing
     */
    if (face->face_flags & FT_FACE_FLAG_SCALABLE)
	load_flags |= FT_LOAD_NO_BITMAP;
    
    if (FT_Load_Glyph (face, glyph, load_flags))
	return FcFalse;
    
    slot = face->glyph;
    if (!glyph)
	return FcFalse;
    
    switch (slot->format) {
    case ft_glyph_format_bitmap:
	/*
	 * Bitmaps are assumed to be reasonable; if
	 * this proves to be a rash assumption, this
	 * code can be easily modified
	 */
	return FcTrue;
    case ft_glyph_format_outline:
	/*
	 * Glyphs with contours are always OK
	 */
	if (slot->outline.n_contours != 0)
	    return FcTrue;
	/*
	 * Glyphs with no contours are only OK if
	 * they're members of the Blanks set specified
	 * in the configuration.  If blanks isn't set,
	 * then allow any glyph to be blank
	 */
	if (!blanks || FcBlanksIsMember (blanks, ucs4))
	    return FcTrue;
	/* fall through ... */
    default:
	break;
    }
    return FcFalse;
}

FcCharSet *
FcFreeTypeCharSet (FT_Face face, FcBlanks *blanks)
{
    FcChar32	    page, off, max, ucs4;
#ifdef CHECK
    FcChar32	    font_max = 0;
#endif
    FcCharSet	    *fcs;
    FcCharLeaf	    *leaf;
    const FcCharMap *map;
    int		    o;
    int		    i;
    FT_UInt	    glyph;

    fcs = FcCharSetCreate ();
    if (!fcs)
	goto bail0;
    
    for (o = 0; o < NUM_DECODE; o++)
    {
	if (FT_Select_Charmap (face, fcFontDecoders[o].encoding) != 0)
	    continue;
	map = fcFontDecoders[o].map;
	if (map)
	{
	    /*
	     * Non-Unicode tables are easy; there's a list of all possible
	     * characters
	     */
	    for (i = 0; i < map->nent; i++)
	    {
		ucs4 = map->ent[i].bmp;
		glyph = FT_Get_Char_Index (face, map->ent[i].encode);
		if (glyph && FcFreeTypeCheckGlyph (face, ucs4, glyph, blanks))
		{
		    leaf = FcCharSetFindLeafCreate (fcs, ucs4);
		    if (!leaf)
			goto bail1;
		    leaf->map[(ucs4 & 0xff) >> 5] |= (1 << (ucs4 & 0x1f));
#ifdef CHECK
		    if (ucs4 > font_max)
			font_max = ucs4;
#endif
		}
	    }
	}
	else
	{
	    max = fcFontDecoders[o].max;
	  
	    /*
	     * Find the first encoded character in the font
	     */
	    ucs4 = 0;
	    if (FT_Get_Char_Index (face, 0))
		ucs4 = 0;
	    else
		ucs4 = FT_Get_Next_Char (face, 0);

	    for (;;)
	    {
		page = ucs4 >> 8;
		leaf = 0;
		while ((ucs4 >> 8) == page)
		{
		    glyph = FT_Get_Char_Index (face, ucs4);
		    if (glyph && FcFreeTypeCheckGlyph (face, ucs4, 
						       glyph, blanks))
		    {
			if (!leaf)
			{
			    leaf = FcCharSetFindLeafCreate (fcs, ucs4);
			    if (!leaf)
				goto bail1;
			}
			off = ucs4 & 0xff;
			leaf->map[off >> 5] |= (1 << (off & 0x1f));
#ifdef CHECK
			if (ucs4 > font_max)
			    font_max = ucs4;
#endif
		    }
		    ucs4++;
		}
		ucs4 = FT_Get_Next_Char (face, ucs4 - 1);
		if (!ucs4)
		    break;
	    }
#ifdef CHECK
	    for (ucs4 = 0; ucs4 < 0x10000; ucs4++)
	    {
		FcBool	    FT_Has, FC_Has;

		FT_Has = FT_Get_Char_Index (face, ucs4) != 0;
		FC_Has = FcCharSetHasChar (fcs, ucs4);
		if (FT_Has != FC_Has)
		{
		    printf ("0x%08x FT says %d FC says %d\n", ucs4, FT_Has, FC_Has);
		}
	    }
#endif
	}
    }
#ifdef CHECK
    printf ("%d glyphs %d encoded\n", (int) face->num_glyphs, FcCharSetCount (fcs));
    for (ucs4 = 0; ucs4 <= font_max; ucs4++)
    {
	FcBool	has_char = FcFreeTypeCharIndex (face, ucs4) != 0;
	FcBool	has_bit = FcCharSetHasChar (fcs, ucs4);

	if (has_char && !has_bit)
	    printf ("Bitmap missing char 0x%x\n", ucs4);
	else if (!has_char && has_bit)
	    printf ("Bitmap extra char 0x%x\n", ucs4);
    }
#endif
    return fcs;
bail1:
    FcCharSetDestroy (fcs);
bail0:
    return 0;
}

static const FcCharEnt AppleRomanEnt[] = {
    { 0x0020, 0x20 }, /* SPACE */
    { 0x0021, 0x21 }, /* EXCLAMATION MARK */
    { 0x0022, 0x22 }, /* QUOTATION MARK */
    { 0x0023, 0x23 }, /* NUMBER SIGN */
    { 0x0024, 0x24 }, /* DOLLAR SIGN */
    { 0x0025, 0x25 }, /* PERCENT SIGN */
    { 0x0026, 0x26 }, /* AMPERSAND */
    { 0x0027, 0x27 }, /* APOSTROPHE */
    { 0x0028, 0x28 }, /* LEFT PARENTHESIS */
    { 0x0029, 0x29 }, /* RIGHT PARENTHESIS */
    { 0x002A, 0x2A }, /* ASTERISK */
    { 0x002B, 0x2B }, /* PLUS SIGN */
    { 0x002C, 0x2C }, /* COMMA */
    { 0x002D, 0x2D }, /* HYPHEN-MINUS */
    { 0x002E, 0x2E }, /* FULL STOP */
    { 0x002F, 0x2F }, /* SOLIDUS */
    { 0x0030, 0x30 }, /* DIGIT ZERO */
    { 0x0031, 0x31 }, /* DIGIT ONE */
    { 0x0032, 0x32 }, /* DIGIT TWO */
    { 0x0033, 0x33 }, /* DIGIT THREE */
    { 0x0034, 0x34 }, /* DIGIT FOUR */
    { 0x0035, 0x35 }, /* DIGIT FIVE */
    { 0x0036, 0x36 }, /* DIGIT SIX */
    { 0x0037, 0x37 }, /* DIGIT SEVEN */
    { 0x0038, 0x38 }, /* DIGIT EIGHT */
    { 0x0039, 0x39 }, /* DIGIT NINE */
    { 0x003A, 0x3A }, /* COLON */
    { 0x003B, 0x3B }, /* SEMICOLON */
    { 0x003C, 0x3C }, /* LESS-THAN SIGN */
    { 0x003D, 0x3D }, /* EQUALS SIGN */
    { 0x003E, 0x3E }, /* GREATER-THAN SIGN */
    { 0x003F, 0x3F }, /* QUESTION MARK */
    { 0x0040, 0x40 }, /* COMMERCIAL AT */
    { 0x0041, 0x41 }, /* LATIN CAPITAL LETTER A */
    { 0x0042, 0x42 }, /* LATIN CAPITAL LETTER B */
    { 0x0043, 0x43 }, /* LATIN CAPITAL LETTER C */
    { 0x0044, 0x44 }, /* LATIN CAPITAL LETTER D */
    { 0x0045, 0x45 }, /* LATIN CAPITAL LETTER E */
    { 0x0046, 0x46 }, /* LATIN CAPITAL LETTER F */
    { 0x0047, 0x47 }, /* LATIN CAPITAL LETTER G */
    { 0x0048, 0x48 }, /* LATIN CAPITAL LETTER H */
    { 0x0049, 0x49 }, /* LATIN CAPITAL LETTER I */
    { 0x004A, 0x4A }, /* LATIN CAPITAL LETTER J */
    { 0x004B, 0x4B }, /* LATIN CAPITAL LETTER K */
    { 0x004C, 0x4C }, /* LATIN CAPITAL LETTER L */
    { 0x004D, 0x4D }, /* LATIN CAPITAL LETTER M */
    { 0x004E, 0x4E }, /* LATIN CAPITAL LETTER N */
    { 0x004F, 0x4F }, /* LATIN CAPITAL LETTER O */
    { 0x0050, 0x50 }, /* LATIN CAPITAL LETTER P */
    { 0x0051, 0x51 }, /* LATIN CAPITAL LETTER Q */
    { 0x0052, 0x52 }, /* LATIN CAPITAL LETTER R */
    { 0x0053, 0x53 }, /* LATIN CAPITAL LETTER S */
    { 0x0054, 0x54 }, /* LATIN CAPITAL LETTER T */
    { 0x0055, 0x55 }, /* LATIN CAPITAL LETTER U */
    { 0x0056, 0x56 }, /* LATIN CAPITAL LETTER V */
    { 0x0057, 0x57 }, /* LATIN CAPITAL LETTER W */
    { 0x0058, 0x58 }, /* LATIN CAPITAL LETTER X */
    { 0x0059, 0x59 }, /* LATIN CAPITAL LETTER Y */
    { 0x005A, 0x5A }, /* LATIN CAPITAL LETTER Z */
    { 0x005B, 0x5B }, /* LEFT SQUARE BRACKET */
    { 0x005C, 0x5C }, /* REVERSE SOLIDUS */
    { 0x005D, 0x5D }, /* RIGHT SQUARE BRACKET */
    { 0x005E, 0x5E }, /* CIRCUMFLEX ACCENT */
    { 0x005F, 0x5F }, /* LOW LINE */
    { 0x0060, 0x60 }, /* GRAVE ACCENT */
    { 0x0061, 0x61 }, /* LATIN SMALL LETTER A */
    { 0x0062, 0x62 }, /* LATIN SMALL LETTER B */
    { 0x0063, 0x63 }, /* LATIN SMALL LETTER C */
    { 0x0064, 0x64 }, /* LATIN SMALL LETTER D */
    { 0x0065, 0x65 }, /* LATIN SMALL LETTER E */
    { 0x0066, 0x66 }, /* LATIN SMALL LETTER F */
    { 0x0067, 0x67 }, /* LATIN SMALL LETTER G */
    { 0x0068, 0x68 }, /* LATIN SMALL LETTER H */
    { 0x0069, 0x69 }, /* LATIN SMALL LETTER I */
    { 0x006A, 0x6A }, /* LATIN SMALL LETTER J */
    { 0x006B, 0x6B }, /* LATIN SMALL LETTER K */
    { 0x006C, 0x6C }, /* LATIN SMALL LETTER L */
    { 0x006D, 0x6D }, /* LATIN SMALL LETTER M */
    { 0x006E, 0x6E }, /* LATIN SMALL LETTER N */
    { 0x006F, 0x6F }, /* LATIN SMALL LETTER O */
    { 0x0070, 0x70 }, /* LATIN SMALL LETTER P */
    { 0x0071, 0x71 }, /* LATIN SMALL LETTER Q */
    { 0x0072, 0x72 }, /* LATIN SMALL LETTER R */
    { 0x0073, 0x73 }, /* LATIN SMALL LETTER S */
    { 0x0074, 0x74 }, /* LATIN SMALL LETTER T */
    { 0x0075, 0x75 }, /* LATIN SMALL LETTER U */
    { 0x0076, 0x76 }, /* LATIN SMALL LETTER V */
    { 0x0077, 0x77 }, /* LATIN SMALL LETTER W */
    { 0x0078, 0x78 }, /* LATIN SMALL LETTER X */
    { 0x0079, 0x79 }, /* LATIN SMALL LETTER Y */
    { 0x007A, 0x7A }, /* LATIN SMALL LETTER Z */
    { 0x007B, 0x7B }, /* LEFT CURLY BRACKET */
    { 0x007C, 0x7C }, /* VERTICAL LINE */
    { 0x007D, 0x7D }, /* RIGHT CURLY BRACKET */
    { 0x007E, 0x7E }, /* TILDE */
    { 0x00A0, 0xCA }, /* NO-BREAK SPACE */
    { 0x00A1, 0xC1 }, /* INVERTED EXCLAMATION MARK */
    { 0x00A2, 0xA2 }, /* CENT SIGN */
    { 0x00A3, 0xA3 }, /* POUND SIGN */
    { 0x00A5, 0xB4 }, /* YEN SIGN */
    { 0x00A7, 0xA4 }, /* SECTION SIGN */
    { 0x00A8, 0xAC }, /* DIAERESIS */
    { 0x00A9, 0xA9 }, /* COPYRIGHT SIGN */
    { 0x00AA, 0xBB }, /* FEMININE ORDINAL INDICATOR */
    { 0x00AB, 0xC7 }, /* LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
    { 0x00AC, 0xC2 }, /* NOT SIGN */
    { 0x00AE, 0xA8 }, /* REGISTERED SIGN */
    { 0x00AF, 0xF8 }, /* MACRON */
    { 0x00B0, 0xA1 }, /* DEGREE SIGN */
    { 0x00B1, 0xB1 }, /* PLUS-MINUS SIGN */
    { 0x00B4, 0xAB }, /* ACUTE ACCENT */
    { 0x00B5, 0xB5 }, /* MICRO SIGN */
    { 0x00B6, 0xA6 }, /* PILCROW SIGN */
    { 0x00B7, 0xE1 }, /* MIDDLE DOT */
    { 0x00B8, 0xFC }, /* CEDILLA */
    { 0x00BA, 0xBC }, /* MASCULINE ORDINAL INDICATOR */
    { 0x00BB, 0xC8 }, /* RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
    { 0x00BF, 0xC0 }, /* INVERTED QUESTION MARK */
    { 0x00C0, 0xCB }, /* LATIN CAPITAL LETTER A WITH GRAVE */
    { 0x00C1, 0xE7 }, /* LATIN CAPITAL LETTER A WITH ACUTE */
    { 0x00C2, 0xE5 }, /* LATIN CAPITAL LETTER A WITH CIRCUMFLEX */
    { 0x00C3, 0xCC }, /* LATIN CAPITAL LETTER A WITH TILDE */
    { 0x00C4, 0x80 }, /* LATIN CAPITAL LETTER A WITH DIAERESIS */
    { 0x00C5, 0x81 }, /* LATIN CAPITAL LETTER A WITH RING ABOVE */
    { 0x00C6, 0xAE }, /* LATIN CAPITAL LETTER AE */
    { 0x00C7, 0x82 }, /* LATIN CAPITAL LETTER C WITH CEDILLA */
    { 0x00C8, 0xE9 }, /* LATIN CAPITAL LETTER E WITH GRAVE */
    { 0x00C9, 0x83 }, /* LATIN CAPITAL LETTER E WITH ACUTE */
    { 0x00CA, 0xE6 }, /* LATIN CAPITAL LETTER E WITH CIRCUMFLEX */
    { 0x00CB, 0xE8 }, /* LATIN CAPITAL LETTER E WITH DIAERESIS */
    { 0x00CC, 0xED }, /* LATIN CAPITAL LETTER I WITH GRAVE */
    { 0x00CD, 0xEA }, /* LATIN CAPITAL LETTER I WITH ACUTE */
    { 0x00CE, 0xEB }, /* LATIN CAPITAL LETTER I WITH CIRCUMFLEX */
    { 0x00CF, 0xEC }, /* LATIN CAPITAL LETTER I WITH DIAERESIS */
    { 0x00D1, 0x84 }, /* LATIN CAPITAL LETTER N WITH TILDE */
    { 0x00D2, 0xF1 }, /* LATIN CAPITAL LETTER O WITH GRAVE */
    { 0x00D3, 0xEE }, /* LATIN CAPITAL LETTER O WITH ACUTE */
    { 0x00D4, 0xEF }, /* LATIN CAPITAL LETTER O WITH CIRCUMFLEX */
    { 0x00D5, 0xCD }, /* LATIN CAPITAL LETTER O WITH TILDE */
    { 0x00D6, 0x85 }, /* LATIN CAPITAL LETTER O WITH DIAERESIS */
    { 0x00D8, 0xAF }, /* LATIN CAPITAL LETTER O WITH STROKE */
    { 0x00D9, 0xF4 }, /* LATIN CAPITAL LETTER U WITH GRAVE */
    { 0x00DA, 0xF2 }, /* LATIN CAPITAL LETTER U WITH ACUTE */
    { 0x00DB, 0xF3 }, /* LATIN CAPITAL LETTER U WITH CIRCUMFLEX */
    { 0x00DC, 0x86 }, /* LATIN CAPITAL LETTER U WITH DIAERESIS */
    { 0x00DF, 0xA7 }, /* LATIN SMALL LETTER SHARP S */
    { 0x00E0, 0x88 }, /* LATIN SMALL LETTER A WITH GRAVE */
    { 0x00E1, 0x87 }, /* LATIN SMALL LETTER A WITH ACUTE */
    { 0x00E2, 0x89 }, /* LATIN SMALL LETTER A WITH CIRCUMFLEX */
    { 0x00E3, 0x8B }, /* LATIN SMALL LETTER A WITH TILDE */
    { 0x00E4, 0x8A }, /* LATIN SMALL LETTER A WITH DIAERESIS */
    { 0x00E5, 0x8C }, /* LATIN SMALL LETTER A WITH RING ABOVE */
    { 0x00E6, 0xBE }, /* LATIN SMALL LETTER AE */
    { 0x00E7, 0x8D }, /* LATIN SMALL LETTER C WITH CEDILLA */
    { 0x00E8, 0x8F }, /* LATIN SMALL LETTER E WITH GRAVE */
    { 0x00E9, 0x8E }, /* LATIN SMALL LETTER E WITH ACUTE */
    { 0x00EA, 0x90 }, /* LATIN SMALL LETTER E WITH CIRCUMFLEX */
    { 0x00EB, 0x91 }, /* LATIN SMALL LETTER E WITH DIAERESIS */
    { 0x00EC, 0x93 }, /* LATIN SMALL LETTER I WITH GRAVE */
    { 0x00ED, 0x92 }, /* LATIN SMALL LETTER I WITH ACUTE */
    { 0x00EE, 0x94 }, /* LATIN SMALL LETTER I WITH CIRCUMFLEX */
    { 0x00EF, 0x95 }, /* LATIN SMALL LETTER I WITH DIAERESIS */
    { 0x00F1, 0x96 }, /* LATIN SMALL LETTER N WITH TILDE */
    { 0x00F2, 0x98 }, /* LATIN SMALL LETTER O WITH GRAVE */
    { 0x00F3, 0x97 }, /* LATIN SMALL LETTER O WITH ACUTE */
    { 0x00F4, 0x99 }, /* LATIN SMALL LETTER O WITH CIRCUMFLEX */
    { 0x00F5, 0x9B }, /* LATIN SMALL LETTER O WITH TILDE */
    { 0x00F6, 0x9A }, /* LATIN SMALL LETTER O WITH DIAERESIS */
    { 0x00F7, 0xD6 }, /* DIVISION SIGN */
    { 0x00F8, 0xBF }, /* LATIN SMALL LETTER O WITH STROKE */
    { 0x00F9, 0x9D }, /* LATIN SMALL LETTER U WITH GRAVE */
    { 0x00FA, 0x9C }, /* LATIN SMALL LETTER U WITH ACUTE */
    { 0x00FB, 0x9E }, /* LATIN SMALL LETTER U WITH CIRCUMFLEX */
    { 0x00FC, 0x9F }, /* LATIN SMALL LETTER U WITH DIAERESIS */
    { 0x00FF, 0xD8 }, /* LATIN SMALL LETTER Y WITH DIAERESIS */
    { 0x0131, 0xF5 }, /* LATIN SMALL LETTER DOTLESS I */
    { 0x0152, 0xCE }, /* LATIN CAPITAL LIGATURE OE */
    { 0x0153, 0xCF }, /* LATIN SMALL LIGATURE OE */
    { 0x0178, 0xD9 }, /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
    { 0x0192, 0xC4 }, /* LATIN SMALL LETTER F WITH HOOK */
    { 0x02C6, 0xF6 }, /* MODIFIER LETTER CIRCUMFLEX ACCENT */
    { 0x02C7, 0xFF }, /* CARON */
    { 0x02D8, 0xF9 }, /* BREVE */
    { 0x02D9, 0xFA }, /* DOT ABOVE */
    { 0x02DA, 0xFB }, /* RING ABOVE */
    { 0x02DB, 0xFE }, /* OGONEK */
    { 0x02DC, 0xF7 }, /* SMALL TILDE */
    { 0x02DD, 0xFD }, /* DOUBLE ACUTE ACCENT */
    { 0x03A9, 0xBD }, /* GREEK CAPITAL LETTER OMEGA */
    { 0x03C0, 0xB9 }, /* GREEK SMALL LETTER PI */
    { 0x2013, 0xD0 }, /* EN DASH */
    { 0x2014, 0xD1 }, /* EM DASH */
    { 0x2018, 0xD4 }, /* LEFT SINGLE QUOTATION MARK */
    { 0x2019, 0xD5 }, /* RIGHT SINGLE QUOTATION MARK */
    { 0x201A, 0xE2 }, /* SINGLE LOW-9 QUOTATION MARK */
    { 0x201C, 0xD2 }, /* LEFT DOUBLE QUOTATION MARK */
    { 0x201D, 0xD3 }, /* RIGHT DOUBLE QUOTATION MARK */
    { 0x201E, 0xE3 }, /* DOUBLE LOW-9 QUOTATION MARK */
    { 0x2020, 0xA0 }, /* DAGGER */
    { 0x2021, 0xE0 }, /* DOUBLE DAGGER */
    { 0x2022, 0xA5 }, /* BULLET */
    { 0x2026, 0xC9 }, /* HORIZONTAL ELLIPSIS */
    { 0x2030, 0xE4 }, /* PER MILLE SIGN */
    { 0x2039, 0xDC }, /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
    { 0x203A, 0xDD }, /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
    { 0x2044, 0xDA }, /* FRACTION SLASH */
    { 0x20AC, 0xDB }, /* EURO SIGN */
    { 0x2122, 0xAA }, /* TRADE MARK SIGN */
    { 0x2202, 0xB6 }, /* PARTIAL DIFFERENTIAL */
    { 0x2206, 0xC6 }, /* INCREMENT */
    { 0x220F, 0xB8 }, /* N-ARY PRODUCT */
    { 0x2211, 0xB7 }, /* N-ARY SUMMATION */
    { 0x221A, 0xC3 }, /* SQUARE ROOT */
    { 0x221E, 0xB0 }, /* INFINITY */
    { 0x222B, 0xBA }, /* INTEGRAL */
    { 0x2248, 0xC5 }, /* ALMOST EQUAL TO */
    { 0x2260, 0xAD }, /* NOT EQUAL TO */
    { 0x2264, 0xB2 }, /* LESS-THAN OR EQUAL TO */
    { 0x2265, 0xB3 }, /* GREATER-THAN OR EQUAL TO */
    { 0x25CA, 0xD7 }, /* LOZENGE */
    { 0xF8FF, 0xF0 }, /* Apple logo */
    { 0xFB01, 0xDE }, /* LATIN SMALL LIGATURE FI */
    { 0xFB02, 0xDF }, /* LATIN SMALL LIGATURE FL */
};

static const FcCharMap AppleRoman = {
    AppleRomanEnt,
    sizeof (AppleRomanEnt) / sizeof (AppleRomanEnt[0])
};

static const FcCharEnt AdobeSymbolEnt[] = {
    { 0x0020, 0x20 }, /* SPACE	# space */
    { 0x0021, 0x21 }, /* EXCLAMATION MARK	# exclam */
    { 0x0023, 0x23 }, /* NUMBER SIGN	# numbersign */
    { 0x0025, 0x25 }, /* PERCENT SIGN	# percent */
    { 0x0026, 0x26 }, /* AMPERSAND	# ampersand */
    { 0x0028, 0x28 }, /* LEFT PARENTHESIS	# parenleft */
    { 0x0029, 0x29 }, /* RIGHT PARENTHESIS	# parenright */
    { 0x002B, 0x2B }, /* PLUS SIGN	# plus */
    { 0x002C, 0x2C }, /* COMMA	# comma */
    { 0x002E, 0x2E }, /* FULL STOP	# period */
    { 0x002F, 0x2F }, /* SOLIDUS	# slash */
    { 0x0030, 0x30 }, /* DIGIT ZERO	# zero */
    { 0x0031, 0x31 }, /* DIGIT ONE	# one */
    { 0x0032, 0x32 }, /* DIGIT TWO	# two */
    { 0x0033, 0x33 }, /* DIGIT THREE	# three */
    { 0x0034, 0x34 }, /* DIGIT FOUR	# four */
    { 0x0035, 0x35 }, /* DIGIT FIVE	# five */
    { 0x0036, 0x36 }, /* DIGIT SIX	# six */
    { 0x0037, 0x37 }, /* DIGIT SEVEN	# seven */
    { 0x0038, 0x38 }, /* DIGIT EIGHT	# eight */
    { 0x0039, 0x39 }, /* DIGIT NINE	# nine */
    { 0x003A, 0x3A }, /* COLON	# colon */
    { 0x003B, 0x3B }, /* SEMICOLON	# semicolon */
    { 0x003C, 0x3C }, /* LESS-THAN SIGN	# less */
    { 0x003D, 0x3D }, /* EQUALS SIGN	# equal */
    { 0x003E, 0x3E }, /* GREATER-THAN SIGN	# greater */
    { 0x003F, 0x3F }, /* QUESTION MARK	# question */
    { 0x005B, 0x5B }, /* LEFT SQUARE BRACKET	# bracketleft */
    { 0x005D, 0x5D }, /* RIGHT SQUARE BRACKET	# bracketright */
    { 0x005F, 0x5F }, /* LOW LINE	# underscore */
    { 0x007B, 0x7B }, /* LEFT CURLY BRACKET	# braceleft */
    { 0x007C, 0x7C }, /* VERTICAL LINE	# bar */
    { 0x007D, 0x7D }, /* RIGHT CURLY BRACKET	# braceright */
    { 0x00A0, 0x20 }, /* NO-BREAK SPACE	# space */
    { 0x00AC, 0xD8 }, /* NOT SIGN	# logicalnot */
    { 0x00B0, 0xB0 }, /* DEGREE SIGN	# degree */
    { 0x00B1, 0xB1 }, /* PLUS-MINUS SIGN	# plusminus */
    { 0x00B5, 0x6D }, /* MICRO SIGN	# mu */
    { 0x00D7, 0xB4 }, /* MULTIPLICATION SIGN	# multiply */
    { 0x00F7, 0xB8 }, /* DIVISION SIGN	# divide */
    { 0x0192, 0xA6 }, /* LATIN SMALL LETTER F WITH HOOK	# florin */
    { 0x0391, 0x41 }, /* GREEK CAPITAL LETTER ALPHA	# Alpha */
    { 0x0392, 0x42 }, /* GREEK CAPITAL LETTER BETA	# Beta */
    { 0x0393, 0x47 }, /* GREEK CAPITAL LETTER GAMMA	# Gamma */
    { 0x0394, 0x44 }, /* GREEK CAPITAL LETTER DELTA	# Delta */
    { 0x0395, 0x45 }, /* GREEK CAPITAL LETTER EPSILON	# Epsilon */
    { 0x0396, 0x5A }, /* GREEK CAPITAL LETTER ZETA	# Zeta */
    { 0x0397, 0x48 }, /* GREEK CAPITAL LETTER ETA	# Eta */
    { 0x0398, 0x51 }, /* GREEK CAPITAL LETTER THETA	# Theta */
    { 0x0399, 0x49 }, /* GREEK CAPITAL LETTER IOTA	# Iota */
    { 0x039A, 0x4B }, /* GREEK CAPITAL LETTER KAPPA	# Kappa */
    { 0x039B, 0x4C }, /* GREEK CAPITAL LETTER LAMDA	# Lambda */
    { 0x039C, 0x4D }, /* GREEK CAPITAL LETTER MU	# Mu */
    { 0x039D, 0x4E }, /* GREEK CAPITAL LETTER NU	# Nu */
    { 0x039E, 0x58 }, /* GREEK CAPITAL LETTER XI	# Xi */
    { 0x039F, 0x4F }, /* GREEK CAPITAL LETTER OMICRON	# Omicron */
    { 0x03A0, 0x50 }, /* GREEK CAPITAL LETTER PI	# Pi */
    { 0x03A1, 0x52 }, /* GREEK CAPITAL LETTER RHO	# Rho */
    { 0x03A3, 0x53 }, /* GREEK CAPITAL LETTER SIGMA	# Sigma */
    { 0x03A4, 0x54 }, /* GREEK CAPITAL LETTER TAU	# Tau */
    { 0x03A5, 0x55 }, /* GREEK CAPITAL LETTER UPSILON	# Upsilon */
    { 0x03A6, 0x46 }, /* GREEK CAPITAL LETTER PHI	# Phi */
    { 0x03A7, 0x43 }, /* GREEK CAPITAL LETTER CHI	# Chi */
    { 0x03A8, 0x59 }, /* GREEK CAPITAL LETTER PSI	# Psi */
    { 0x03A9, 0x57 }, /* GREEK CAPITAL LETTER OMEGA	# Omega */
    { 0x03B1, 0x61 }, /* GREEK SMALL LETTER ALPHA	# alpha */
    { 0x03B2, 0x62 }, /* GREEK SMALL LETTER BETA	# beta */
    { 0x03B3, 0x67 }, /* GREEK SMALL LETTER GAMMA	# gamma */
    { 0x03B4, 0x64 }, /* GREEK SMALL LETTER DELTA	# delta */
    { 0x03B5, 0x65 }, /* GREEK SMALL LETTER EPSILON	# epsilon */
    { 0x03B6, 0x7A }, /* GREEK SMALL LETTER ZETA	# zeta */
    { 0x03B7, 0x68 }, /* GREEK SMALL LETTER ETA	# eta */
    { 0x03B8, 0x71 }, /* GREEK SMALL LETTER THETA	# theta */
    { 0x03B9, 0x69 }, /* GREEK SMALL LETTER IOTA	# iota */
    { 0x03BA, 0x6B }, /* GREEK SMALL LETTER KAPPA	# kappa */
    { 0x03BB, 0x6C }, /* GREEK SMALL LETTER LAMDA	# lambda */
    { 0x03BC, 0x6D }, /* GREEK SMALL LETTER MU	# mu */
    { 0x03BD, 0x6E }, /* GREEK SMALL LETTER NU	# nu */
    { 0x03BE, 0x78 }, /* GREEK SMALL LETTER XI	# xi */
    { 0x03BF, 0x6F }, /* GREEK SMALL LETTER OMICRON	# omicron */
    { 0x03C0, 0x70 }, /* GREEK SMALL LETTER PI	# pi */
    { 0x03C1, 0x72 }, /* GREEK SMALL LETTER RHO	# rho */
    { 0x03C2, 0x56 }, /* GREEK SMALL LETTER FINAL SIGMA	# sigma1 */
    { 0x03C3, 0x73 }, /* GREEK SMALL LETTER SIGMA	# sigma */
    { 0x03C4, 0x74 }, /* GREEK SMALL LETTER TAU	# tau */
    { 0x03C5, 0x75 }, /* GREEK SMALL LETTER UPSILON	# upsilon */
    { 0x03C6, 0x66 }, /* GREEK SMALL LETTER PHI	# phi */
    { 0x03C7, 0x63 }, /* GREEK SMALL LETTER CHI	# chi */
    { 0x03C8, 0x79 }, /* GREEK SMALL LETTER PSI	# psi */
    { 0x03C9, 0x77 }, /* GREEK SMALL LETTER OMEGA	# omega */
    { 0x03D1, 0x4A }, /* GREEK THETA SYMBOL	# theta1 */
    { 0x03D2, 0xA1 }, /* GREEK UPSILON WITH HOOK SYMBOL	# Upsilon1 */
    { 0x03D5, 0x6A }, /* GREEK PHI SYMBOL	# phi1 */
    { 0x03D6, 0x76 }, /* GREEK PI SYMBOL	# omega1 */
    { 0x2022, 0xB7 }, /* BULLET	# bullet */
    { 0x2026, 0xBC }, /* HORIZONTAL ELLIPSIS	# ellipsis */
    { 0x2032, 0xA2 }, /* PRIME	# minute */
    { 0x2033, 0xB2 }, /* DOUBLE PRIME	# second */
    { 0x2044, 0xA4 }, /* FRACTION SLASH	# fraction */
    { 0x20AC, 0xA0 }, /* EURO SIGN	# Euro */
    { 0x2111, 0xC1 }, /* BLACK-LETTER CAPITAL I	# Ifraktur */
    { 0x2118, 0xC3 }, /* SCRIPT CAPITAL P	# weierstrass */
    { 0x211C, 0xC2 }, /* BLACK-LETTER CAPITAL R	# Rfraktur */
    { 0x2126, 0x57 }, /* OHM SIGN	# Omega */
    { 0x2135, 0xC0 }, /* ALEF SYMBOL	# aleph */
    { 0x2190, 0xAC }, /* LEFTWARDS ARROW	# arrowleft */
    { 0x2191, 0xAD }, /* UPWARDS ARROW	# arrowup */
    { 0x2192, 0xAE }, /* RIGHTWARDS ARROW	# arrowright */
    { 0x2193, 0xAF }, /* DOWNWARDS ARROW	# arrowdown */
    { 0x2194, 0xAB }, /* LEFT RIGHT ARROW	# arrowboth */
    { 0x21B5, 0xBF }, /* DOWNWARDS ARROW WITH CORNER LEFTWARDS	# carriagereturn */
    { 0x21D0, 0xDC }, /* LEFTWARDS DOUBLE ARROW	# arrowdblleft */
    { 0x21D1, 0xDD }, /* UPWARDS DOUBLE ARROW	# arrowdblup */
    { 0x21D2, 0xDE }, /* RIGHTWARDS DOUBLE ARROW	# arrowdblright */
    { 0x21D3, 0xDF }, /* DOWNWARDS DOUBLE ARROW	# arrowdbldown */
    { 0x21D4, 0xDB }, /* LEFT RIGHT DOUBLE ARROW	# arrowdblboth */
    { 0x2200, 0x22 }, /* FOR ALL	# universal */
    { 0x2202, 0xB6 }, /* PARTIAL DIFFERENTIAL	# partialdiff */
    { 0x2203, 0x24 }, /* THERE EXISTS	# existential */
    { 0x2205, 0xC6 }, /* EMPTY SET	# emptyset */
    { 0x2206, 0x44 }, /* INCREMENT	# Delta */
    { 0x2207, 0xD1 }, /* NABLA	# gradient */
    { 0x2208, 0xCE }, /* ELEMENT OF	# element */
    { 0x2209, 0xCF }, /* NOT AN ELEMENT OF	# notelement */
    { 0x220B, 0x27 }, /* CONTAINS AS MEMBER	# suchthat */
    { 0x220F, 0xD5 }, /* N-ARY PRODUCT	# product */
    { 0x2211, 0xE5 }, /* N-ARY SUMMATION	# summation */
    { 0x2212, 0x2D }, /* MINUS SIGN	# minus */
    { 0x2215, 0xA4 }, /* DIVISION SLASH	# fraction */
    { 0x2217, 0x2A }, /* ASTERISK OPERATOR	# asteriskmath */
    { 0x221A, 0xD6 }, /* SQUARE ROOT	# radical */
    { 0x221D, 0xB5 }, /* PROPORTIONAL TO	# proportional */
    { 0x221E, 0xA5 }, /* INFINITY	# infinity */
    { 0x2220, 0xD0 }, /* ANGLE	# angle */
    { 0x2227, 0xD9 }, /* LOGICAL AND	# logicaland */
    { 0x2228, 0xDA }, /* LOGICAL OR	# logicalor */
    { 0x2229, 0xC7 }, /* INTERSECTION	# intersection */
    { 0x222A, 0xC8 }, /* UNION	# union */
    { 0x222B, 0xF2 }, /* INTEGRAL	# integral */
    { 0x2234, 0x5C }, /* THEREFORE	# therefore */
    { 0x223C, 0x7E }, /* TILDE OPERATOR	# similar */
    { 0x2245, 0x40 }, /* APPROXIMATELY EQUAL TO	# congruent */
    { 0x2248, 0xBB }, /* ALMOST EQUAL TO	# approxequal */
    { 0x2260, 0xB9 }, /* NOT EQUAL TO	# notequal */
    { 0x2261, 0xBA }, /* IDENTICAL TO	# equivalence */
    { 0x2264, 0xA3 }, /* LESS-THAN OR EQUAL TO	# lessequal */
    { 0x2265, 0xB3 }, /* GREATER-THAN OR EQUAL TO	# greaterequal */
    { 0x2282, 0xCC }, /* SUBSET OF	# propersubset */
    { 0x2283, 0xC9 }, /* SUPERSET OF	# propersuperset */
    { 0x2284, 0xCB }, /* NOT A SUBSET OF	# notsubset */
    { 0x2286, 0xCD }, /* SUBSET OF OR EQUAL TO	# reflexsubset */
    { 0x2287, 0xCA }, /* SUPERSET OF OR EQUAL TO	# reflexsuperset */
    { 0x2295, 0xC5 }, /* CIRCLED PLUS	# circleplus */
    { 0x2297, 0xC4 }, /* CIRCLED TIMES	# circlemultiply */
    { 0x22A5, 0x5E }, /* UP TACK	# perpendicular */
    { 0x22C5, 0xD7 }, /* DOT OPERATOR	# dotmath */
    { 0x2320, 0xF3 }, /* TOP HALF INTEGRAL	# integraltp */
    { 0x2321, 0xF5 }, /* BOTTOM HALF INTEGRAL	# integralbt */
    { 0x2329, 0xE1 }, /* LEFT-POINTING ANGLE BRACKET	# angleleft */
    { 0x232A, 0xF1 }, /* RIGHT-POINTING ANGLE BRACKET	# angleright */
    { 0x25CA, 0xE0 }, /* LOZENGE	# lozenge */
    { 0x2660, 0xAA }, /* BLACK SPADE SUIT	# spade */
    { 0x2663, 0xA7 }, /* BLACK CLUB SUIT	# club */
    { 0x2665, 0xA9 }, /* BLACK HEART SUIT	# heart */
    { 0x2666, 0xA8 }, /* BLACK DIAMOND SUIT	# diamond */
    { 0xF6D9, 0xD3 }, /* COPYRIGHT SIGN SERIF	# copyrightserif (CUS) */
    { 0xF6DA, 0xD2 }, /* REGISTERED SIGN SERIF	# registerserif (CUS) */
    { 0xF6DB, 0xD4 }, /* TRADE MARK SIGN SERIF	# trademarkserif (CUS) */
    { 0xF8E5, 0x60 }, /* RADICAL EXTENDER	# radicalex (CUS) */
    { 0xF8E6, 0xBD }, /* VERTICAL ARROW EXTENDER	# arrowvertex (CUS) */
    { 0xF8E7, 0xBE }, /* HORIZONTAL ARROW EXTENDER	# arrowhorizex (CUS) */
    { 0xF8E8, 0xE2 }, /* REGISTERED SIGN SANS SERIF	# registersans (CUS) */
    { 0xF8E9, 0xE3 }, /* COPYRIGHT SIGN SANS SERIF	# copyrightsans (CUS) */
    { 0xF8EA, 0xE4 }, /* TRADE MARK SIGN SANS SERIF	# trademarksans (CUS) */
    { 0xF8EB, 0xE6 }, /* LEFT PAREN TOP	# parenlefttp (CUS) */
    { 0xF8EC, 0xE7 }, /* LEFT PAREN EXTENDER	# parenleftex (CUS) */
    { 0xF8ED, 0xE8 }, /* LEFT PAREN BOTTOM	# parenleftbt (CUS) */
    { 0xF8EE, 0xE9 }, /* LEFT SQUARE BRACKET TOP	# bracketlefttp (CUS) */
    { 0xF8EF, 0xEA }, /* LEFT SQUARE BRACKET EXTENDER	# bracketleftex (CUS) */
    { 0xF8F0, 0xEB }, /* LEFT SQUARE BRACKET BOTTOM	# bracketleftbt (CUS) */
    { 0xF8F1, 0xEC }, /* LEFT CURLY BRACKET TOP	# bracelefttp (CUS) */
    { 0xF8F2, 0xED }, /* LEFT CURLY BRACKET MID	# braceleftmid (CUS) */
    { 0xF8F3, 0xEE }, /* LEFT CURLY BRACKET BOTTOM	# braceleftbt (CUS) */
    { 0xF8F4, 0xEF }, /* CURLY BRACKET EXTENDER	# braceex (CUS) */
    { 0xF8F5, 0xF4 }, /* INTEGRAL EXTENDER	# integralex (CUS) */
    { 0xF8F6, 0xF6 }, /* RIGHT PAREN TOP	# parenrighttp (CUS) */
    { 0xF8F7, 0xF7 }, /* RIGHT PAREN EXTENDER	# parenrightex (CUS) */
    { 0xF8F8, 0xF8 }, /* RIGHT PAREN BOTTOM	# parenrightbt (CUS) */
    { 0xF8F9, 0xF9 }, /* RIGHT SQUARE BRACKET TOP	# bracketrighttp (CUS) */
    { 0xF8FA, 0xFA }, /* RIGHT SQUARE BRACKET EXTENDER	# bracketrightex (CUS) */
    { 0xF8FB, 0xFB }, /* RIGHT SQUARE BRACKET BOTTOM	# bracketrightbt (CUS) */
    { 0xF8FC, 0xFC }, /* RIGHT CURLY BRACKET TOP	# bracerighttp (CUS) */
    { 0xF8FD, 0xFD }, /* RIGHT CURLY BRACKET MID	# bracerightmid (CUS) */
    { 0xF8FE, 0xFE }, /* RIGHT CURLY BRACKET BOTTOM	# bracerightbt (CUS) */
};

static const FcCharMap AdobeSymbol = {
    AdobeSymbolEnt,
    sizeof (AdobeSymbolEnt) / sizeof (AdobeSymbolEnt[0]),
};
