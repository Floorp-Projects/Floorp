/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#ifndef __secnew_h_
#define __secnew_h_

#include <stdio.h>

typedef struct BERTemplateStr BERTemplate;
typedef struct BERParseStr BERParse;
typedef struct SECArbStr SECArb;

/*
 * An array of these structures define an encoding for an object using
 * DER. The array is terminated with an entry where kind == 0.
 */
struct BERTemplateStr {
    /* Kind of item to decode/encode */
    unsigned long kind;

    /*
     * Offset from base of structure to SECItem that will hold
     * decoded/encoded value.
     */
    unsigned short offset;

    /*
     * Used with DER_SET or DER_SEQUENCE. If not zero then points to a
     * sub-template. The sub-template is filled in and completed before
     * continuing on.
     */
    BERTemplate *sub;

    /*
     * Argument value, dependent on kind.  Size of structure to allocate
     * when kind==DER_POINTER For Context-Specific Implicit types its the
     * underlying type to use.
     */
    unsigned long arg;
};

/*
 * an arbitrary object
 */
struct SECArbStr {
    unsigned long tag;		/* NOTE: does not support high tag form */
    unsigned long length;	/* as reported in stream */
    union {
	SECItem item;
	struct {
	   int numSubs;
	   SECArb **subs;
	} cons;
    } body;
};

/*
 * Decode a piece of der encoded data.
 *      "dest" points to a structure that will be filled in with the
 *         decoding results.
 *      "t" is a template structure which defines the shape of the
 *         expected data.
 *      "src" is the ber encoded data.
 */

extern SECStatus BER_Decode(PRArenaPool * arena, void *dest, BERTemplate *t,
                           SECArb *arb);


/*
 * Encode a data structure into DER.
 * 	"dest" will be filled in (and memory allocated) to hold the der
 * 	   encoded structure in "src"
 * 	"t" is a template structure which defines the shape of the
 * 	   stored data
 * 	"src" is a pointer to the structure that will be encoded
 */

extern SECStatus BER_Encode(PRArenaPool *arena, SECItem *dest, BERTemplate *t,
			   void *src);

/*
 * Client provided function that will get called with all the bytes
 * passing through the parser
 */
typedef void (*BERFilterProc)(void *instance, unsigned char *buf, int length);

/*
 * Client provided function that can will be called after the tag and
 * length information has been collected. It can be set up to be called
 * either before or after the data has been colleced.
 */
typedef void (*BERNotifyProc)(
    void *instance, SECArb *arb, int depth, PRBool before);

extern BERParse *BER_ParseInit(PRArenaPool *arena, PRBool forceDER);
extern SECArb *BER_ParseFini(BERParse *h);
extern SECStatus BER_ParseSome(BERParse *h, unsigned char *buf, int len);

extern void BER_SetFilter(BERParse *h, BERFilterProc proc, void *instance);
extern void BER_SetLeafStorage(BERParse *h, PRBool keep);
extern void BER_SetNotifyProc(BERParse *h, BERNotifyProc proc, void *instance,
			      PRBool beforeData);

/*
 * A BERUnparseProc is used as a callback to put the encoded SECArb tree
 * tree to some stream. It returns PR_TRUE if the unparsing is to be
 * aborted.
 */
typedef SECStatus (*BERUnparseProc)(
    void *instance, unsigned char *data, int length, SECArb* arb);

/*
 * BER_Unparse walks the SECArb tree calling the BERUnparseProc with
 * various pieces. It returns SECFailure if there was an error during that
 * tree walk.
 */
extern SECStatus BER_Unparse(SECArb *arb, BERUnparseProc proc, void *instance);

/*
 * BER_ResolveLengths does a recursive walk through the tree generating
 * non-zero entries for the length field of each node. It will fail if it
 * discoveres a non-constructed node with a unknown length data field.
 * Leaves are supposed to be of known length.
 */
extern SECStatus BER_ResolveLengths(SECArb *arb);

/*
 * BER_PRettyPrintArb will write an ASCII version of the tree to the FILE
 * out.
 */
extern SECStatus BER_PrettyPrintArb(FILE *out, SECArb* a);

#endif /* __secnew_h_ */
