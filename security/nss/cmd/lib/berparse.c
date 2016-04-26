/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "secutil.h"

typedef enum {
    tagDone, lengthDone, leafDone, compositeDone,
    notDone,
    parseError, parseComplete
} ParseState;

typedef unsigned char Byte;
typedef void (*ParseProc)(BERParse *h, unsigned char **buf, int *len);
typedef struct {
    SECArb arb;
    int pos;			/* length from global start to item start */
    SECArb *parent;
} ParseStackElem;

struct BERParseStr {
    PLArenaPool *his;
    PLArenaPool *mine;
    ParseProc proc;
    int stackDepth;
    ParseStackElem *stackPtr;
    ParseStackElem *stack;
    int pending;		/* bytes remaining to complete this part */
    int pos;			/* running length of consumed characters */
    ParseState state;
    PRBool keepLeaves;
    PRBool derOnly;
    BERFilterProc filter;
    void *filterArg;
    BERNotifyProc before;
    void *beforeArg;
    BERNotifyProc after;
    void *afterArg;
};

#define UNKNOWN -1

static unsigned char NextChar(BERParse *h, unsigned char **buf, int *len)
{
    unsigned char c = *(*buf)++;
    (*len)--;
    h->pos++;
    if (h->filter)
	(*h->filter)(h->filterArg, &c, 1);
    return c;
}

static void ParseTag(BERParse *h, unsigned char **buf, int *len)
{
    SECArb* arb = &(h->stackPtr->arb);
    arb->tag = NextChar(h, buf, len);

    PORT_Assert(h->state == notDone);

  /*
   * NOTE: This does not handle the high-tag-number form
   */
    if ((arb->tag & DER_HIGH_TAG_NUMBER) == DER_HIGH_TAG_NUMBER) {
        PORT_SetError(SEC_ERROR_BAD_DER);
	h->state = parseError;
	return;
    }

    h->pending = UNKNOWN;
    arb->length = UNKNOWN;
    if (arb->tag & DER_CONSTRUCTED) {
	arb->body.cons.numSubs = 0;
	arb->body.cons.subs = NULL;
    } else {
	arb->body.item.len = UNKNOWN;
	arb->body.item.data = NULL;
    }

    h->state = tagDone;
}

static void ParseLength(BERParse *h, unsigned char **buf, int *len)
{
    Byte b;
    SECArb *arb = &(h->stackPtr->arb);

    PORT_Assert(h->state == notDone);

    if (h->pending == UNKNOWN) {
	b = NextChar(h, buf, len);
	if ((b & 0x80) == 0) {	/* short form */
	    arb->length = b;
	    /*
	     * if the tag and the length are both zero bytes, then this
	     * should be the marker showing end of list for the
	     * indefinite length composite
	     */
	    if (arb->length == 0 && arb->tag == 0)
		h->state = compositeDone;
	    else
		h->state = lengthDone;
	    return;
	}

	h->pending = b & 0x7f;
	/* 0 implies this is an indefinite length */
	if (h->pending > 4) {
	    PORT_SetError(SEC_ERROR_BAD_DER);
	    h->state = parseError;
	    return;
	}
	arb->length = 0;
    }

    while ((*len > 0) && (h->pending > 0)) {
	b = NextChar(h, buf, len);
	arb->length = (arb->length << 8) + b;
	h->pending--;
    }
    if (h->pending == 0) {
	if (h->derOnly && (arb->length == 0))
	    h->state = parseError;
	else
	    h->state = lengthDone;
    }
    return;
}

static void ParseLeaf(BERParse *h, unsigned char **buf, int *len)
{
    int count;
    SECArb *arb = &(h->stackPtr->arb);

    PORT_Assert(h->state == notDone);
    PORT_Assert(h->pending >= 0);

    if (*len < h->pending)
	count = *len;
    else
	count = h->pending;

    if (h->keepLeaves)
	memcpy(arb->body.item.data + arb->body.item.len, *buf, count);
    if (h->filter)
	(*h->filter)(h->filterArg, *buf, count);
    *buf += count;
    *len -= count;
    arb->body.item.len += count;
    h->pending -= count;
    h->pos += count;
    if (h->pending == 0) {
	h->state = leafDone;
    }
    return;
}

static void CreateArbNode(BERParse *h)
{
    SECArb *arb = PORT_ArenaAlloc(h->his, sizeof(SECArb));

    *arb = h->stackPtr->arb;

    /* 
     * Special case closing the root
     */	
    if (h->stackPtr == h->stack) {
	PORT_Assert(arb->tag & DER_CONSTRUCTED);
	h->state = parseComplete;
    } else {
	SECArb *parent = h->stackPtr->parent;
	parent->body.cons.subs = DS_ArenaGrow(
	    h->his, parent->body.cons.subs,
	    (parent->body.cons.numSubs) * sizeof(SECArb*),
	    (parent->body.cons.numSubs + 1) * sizeof(SECArb*));
	parent->body.cons.subs[parent->body.cons.numSubs] = arb;
	parent->body.cons.numSubs++;
	h->proc = ParseTag;
	h->state = notDone;
	h->pending = UNKNOWN;
    }
    if (h->after)
	(*h->after)(h->afterArg, arb, h->stackPtr - h->stack, PR_FALSE);
}

SECStatus BER_ParseSome(BERParse *h, unsigned char *buf, int len)
{
    if (h->state == parseError) return PR_TRUE;

    while (len) {
        (*h->proc)(h, &buf, &len);
	if (h->state == parseComplete) {
	    PORT_SetError(SEC_ERROR_BAD_DER);
	    h->state = parseError;
	    return PR_TRUE;
	}
	if (h->state == parseError) return PR_TRUE;
	PORT_Assert(h->state != parseComplete);

        if (h->state <= compositeDone) {
	    if (h->proc == ParseTag) {
		PORT_Assert(h->state == tagDone);
		h->proc = ParseLength;
		h->state = notDone;
	    } else if (h->proc == ParseLength) {
		SECArb *arb = &(h->stackPtr->arb);
		PORT_Assert(h->state == lengthDone || h->state == compositeDone);

		if (h->before)
		    (*h->before)(h->beforeArg, arb,
				 h->stackPtr - h->stack, PR_TRUE);

		/*
		 * Check to see if this is the end of an indefinite
		 * length composite
		 */
		if (h->state == compositeDone) {
		    SECArb *parent = h->stackPtr->parent;
		    PORT_Assert(parent);
		    PORT_Assert(parent->tag & DER_CONSTRUCTED);
		    if (parent->length != 0) {
			PORT_SetError(SEC_ERROR_BAD_DER);
			h->state = parseError;
			return PR_TRUE;
		    }
		    /*
		     * NOTE: This does not check for an indefinite length
		     * composite being contained inside a definite length
		     * composite. It is not clear that is legal.
		     */
		    h->stackPtr--;
		    CreateArbNode(h);
		} else {
		    h->stackPtr->pos = h->pos;


		    if (arb->tag & DER_CONSTRUCTED) {
			SECArb *parent;
			/*
			 * Make sure there is room on the stack before we
			 * stick anything else there.
			 */
			PORT_Assert(h->stackPtr - h->stack < h->stackDepth);
			if (h->stackPtr - h->stack == h->stackDepth - 1) {
			    int newDepth = h->stackDepth * 2;
			    h->stack = DS_ArenaGrow(h->mine, h->stack,
				sizeof(ParseStackElem) * h->stackDepth,
				sizeof(ParseStackElem) * newDepth);
			    h->stackPtr = h->stack + h->stackDepth + 1;
			    h->stackDepth = newDepth;
			}
			parent = &(h->stackPtr->arb);
			h->stackPtr++;
			h->stackPtr->parent = parent;
			h->proc = ParseTag;
			h->state = notDone;
			h->pending = UNKNOWN;
		    } else {
			if (arb->length < 0) {
			    PORT_SetError(SEC_ERROR_BAD_DER);
			    h->state = parseError;
			    return PR_TRUE;
			}
			arb->body.item.len = 0;
			if (arb->length > 0 && h->keepLeaves) {
			    arb->body.item.data =
				PORT_ArenaAlloc(h->his, arb->length);
			} else {
			    arb->body.item.data = NULL;
			}
			h->proc = ParseLeaf;
			h->state = notDone;
			h->pending = arb->length;
		    }
		}
	    } else {
		ParseStackElem *parent;
		PORT_Assert(h->state = leafDone);
		PORT_Assert(h->proc == ParseLeaf);

		for (;;) {
		    CreateArbNode(h);
		    if (h->stackPtr == h->stack)
			break;
		    parent = (h->stackPtr - 1);
		    PORT_Assert(parent->arb.tag & DER_CONSTRUCTED);
		    if (parent->arb.length == 0) /* need explicit end */
			break;
		    if (parent->pos + parent->arb.length > h->pos)
			break;
		    if (parent->pos + parent->arb.length < h->pos) {
			PORT_SetError(SEC_ERROR_BAD_DER);
			h->state = parseError;
			return PR_TRUE;
		    }
		    h->stackPtr = parent;
		}
	    }

	}
    }
    return PR_FALSE;
}
BERParse *BER_ParseInit(PLArenaPool *arena, PRBool derOnly)
{
    BERParse *h;
    PLArenaPool *temp = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (temp == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }
    h = PORT_ArenaAlloc(temp, sizeof(BERParse));
    if (h == NULL) {
	PORT_FreeArena(temp, PR_FALSE);
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return NULL;
    }
    h->his = arena;
    h->mine = temp;
    h->proc = ParseTag;
    h->stackDepth = 20;
    h->stack = PORT_ArenaZAlloc(h->mine,
			      sizeof(ParseStackElem) * h->stackDepth);
    h->stackPtr = h->stack;
    h->state = notDone;
    h->pos = 0;
    h->keepLeaves = PR_TRUE;
    h->before = NULL;
    h->after = NULL;
    h->filter = NULL;
    h->derOnly = derOnly;
    return h;
}

SECArb *BER_ParseFini(BERParse *h)
{
    PLArenaPool *myArena = h->mine;
    SECArb *arb;

    if (h->state != parseComplete) {
	arb = NULL;
    } else {
	arb = PORT_ArenaAlloc(h->his, sizeof(SECArb));
	*arb = h->stackPtr->arb;
    }

    PORT_FreeArena(myArena, PR_FALSE);

    return arb;
}


void BER_SetFilter(BERParse *h, BERFilterProc proc, void *instance)
{
    h->filter = proc;
    h->filterArg = instance;
}

void BER_SetLeafStorage(BERParse *h, PRBool keep)
{
    h->keepLeaves = keep;
}

void BER_SetNotifyProc(BERParse *h, BERNotifyProc proc, void *instance,
			      PRBool beforeData)
{
    if (beforeData) {
	h->before = proc;
	h->beforeArg = instance;
    } else {
	h->after = proc;
	h->afterArg = instance;
    }
}



