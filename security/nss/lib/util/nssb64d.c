/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Base64 decoding (ascii to binary).
 *
 * $Id: nssb64d.c,v 1.7 2008/10/05 20:59:26 nelson%bolyard.com Exp $
 */

#include "nssb64.h"
#include "nspr.h"
#include "secitem.h"
#include "secerr.h"

/*
 * XXX We want this basic support to go into NSPR (the PL part).
 * Until that can happen, the PL interface is going to be kept entirely
 * internal here -- all static functions and opaque data structures.
 * When someone can get it moved over into NSPR, that should be done:
 *    - giving everything names that are accepted by the NSPR module owners
 *	(though I tried to choose ones that would work without modification)
 *    - exporting the functions (remove static declarations and add
 *	to nssutil.def as necessary)
 *    - put prototypes into appropriate header file (probably replacing
 *	the entire current lib/libc/include/plbase64.h in NSPR)
 *	along with a typedef for the context structure (which should be
 *	kept opaque -- definition in the source file only, but typedef
 *	ala "typedef struct PLBase64FooStr PLBase64Foo;" in header file)
 *    - modify anything else as necessary to conform to NSPR required style
 *	(I looked but found no formatting guide to follow)
 *
 * You will want to move over everything from here down to the comment
 * which says "XXX End of base64 decoding code to be moved into NSPR",
 * into a new file in NSPR.
 */

/*
 **************************************************************
 * XXX Beginning of base64 decoding code to be moved into NSPR.
 */

/*
 * This typedef would belong in the NSPR header file (i.e. plbase64.h).
 */
typedef struct PLBase64DecoderStr PLBase64Decoder;

/*
 * The following implementation of base64 decoding was based on code
 * found in libmime (specifically, in mimeenc.c).  It has been adapted to
 * use PR types and naming as well as to provide other necessary semantics
 * (like buffer-in/buffer-out in addition to "streaming" without undue
 * performance hit of extra copying if you made the buffer versions
 * use the output_fn).  It also incorporates some aspects of the current
 * NSPR base64 decoding code.  As such, you may find similarities to
 * both of those implementations.  I tried to use names that reflected
 * the original code when possible.  For this reason you may find some
 * inconsistencies -- libmime used lots of "in" and "out" whereas the
 * NSPR version uses "src" and "dest"; sometimes I changed one to the other
 * and sometimes I left them when I thought the subroutines were at least
 * self-consistent.
 */

PR_BEGIN_EXTERN_C

/*
 * Opaque object used by the decoder to store state.
 */
struct PLBase64DecoderStr {
    /* Current token (or portion, if token_size < 4) being decoded. */
    unsigned char token[4];
    int token_size;

    /*
     * Where to write the decoded data (used when streaming, not when
     * doing all in-memory (buffer) operations).
     *
     * Note that this definition is chosen to be compatible with PR_Write.
     */
    PRInt32 (*output_fn) (void *output_arg, const unsigned char *buf,
			  PRInt32 size);
    void *output_arg;

    /*
     * Where the decoded output goes -- either temporarily (in the streaming
     * case, staged here before it goes to the output function) or what will
     * be the entire buffered result for users of the buffer version.
     */
    unsigned char *output_buffer;
    PRUint32 output_buflen;	/* the total length of allocated buffer */
    PRUint32 output_length;	/* the length that is currently populated */
};

PR_END_EXTERN_C


/*
 * Table to convert an ascii "code" to its corresponding binary value.
 * For ease of use, the binary values in the table are the actual values
 * PLUS ONE.  This is so that the special value of zero can denote an
 * invalid mapping; that was much easier than trying to fill in the other
 * values with some value other than zero, and to check for it.
 * Just remember to SUBTRACT ONE when using the value retrieved.
 */
static unsigned char base64_codetovaluep1[256] = {
/*   0: */	  0,	  0,	  0,	  0,	  0,	  0,	  0,	  0,
/*   8: */	  0,	  0,	  0,	  0,	  0,	  0,	  0,	  0,
/*  16: */	  0,	  0,	  0,	  0,	  0,	  0,	  0,	  0,
/*  24: */	  0,	  0,	  0,	  0,	  0,	  0,	  0,	  0,
/*  32: */	  0,	  0,	  0,	  0,	  0,	  0,	  0,	  0,
/*  40: */	  0,	  0,	  0,	 63,	  0,	  0,	  0,	 64,
/*  48: */	 53,	 54,	 55,	 56,	 57,	 58,	 59,	 60,
/*  56: */	 61,	 62,	  0,	  0,	  0,	  0,	  0,	  0,
/*  64: */	  0,	  1,	  2,	  3,	  4,	  5,	  6,	  7,
/*  72: */	  8,	  9,	 10,	 11,	 12,	 13,	 14,	 15,
/*  80: */	 16,	 17,	 18,	 19,	 20,	 21,	 22,	 23,
/*  88: */	 24,	 25,	 26,	  0,	  0,	  0,	  0,	  0,
/*  96: */	  0,	 27,	 28,	 29,	 30,	 31,	 32,	 33,
/* 104: */	 34,	 35,	 36,	 37,	 38,	 39,	 40,	 41,
/* 112: */	 42,	 43,	 44,	 45,	 46,	 47,	 48,	 49,
/* 120: */	 50,	 51,	 52,	  0,	  0,	  0,	  0,	  0,
/* 128: */	  0,	  0,	  0,	  0,	  0,	  0,	  0,	  0
/* and rest are all zero as well */
};

#define B64_PAD	'='


/*
 * Reads 4; writes 3 (known, or expected, to have no trailing padding).
 * Returns bytes written; -1 on error (unexpected character).
 */
static int
pl_base64_decode_4to3 (const unsigned char *in, unsigned char *out)
{
    int j;
    PRUint32 num = 0;
    unsigned char bits;

    for (j = 0; j < 4; j++) {
	bits = base64_codetovaluep1[in[j]];
	if (bits == 0)
	    return -1;
	num = (num << 6) | (bits - 1);
    }

    out[0] = (unsigned char) (num >> 16);
    out[1] = (unsigned char) ((num >> 8) & 0xFF);
    out[2] = (unsigned char) (num & 0xFF);

    return 3;
}

/*
 * Reads 3; writes 2 (caller already confirmed EOF or trailing padding).
 * Returns bytes written; -1 on error (unexpected character).
 */
static int
pl_base64_decode_3to2 (const unsigned char *in, unsigned char *out)
{
    PRUint32 num = 0;
    unsigned char bits1, bits2, bits3;

    bits1 = base64_codetovaluep1[in[0]];
    bits2 = base64_codetovaluep1[in[1]];
    bits3 = base64_codetovaluep1[in[2]];

    if ((bits1 == 0) || (bits2 == 0) || (bits3 == 0))
	return -1;

    num = ((PRUint32)(bits1 - 1)) << 10;
    num |= ((PRUint32)(bits2 - 1)) << 4;
    num |= ((PRUint32)(bits3 - 1)) >> 2;

    out[0] = (unsigned char) (num >> 8);
    out[1] = (unsigned char) (num & 0xFF);

    return 2;
}

/*
 * Reads 2; writes 1 (caller already confirmed EOF or trailing padding).
 * Returns bytes written; -1 on error (unexpected character).
 */
static int
pl_base64_decode_2to1 (const unsigned char *in, unsigned char *out)
{
    PRUint32 num = 0;
    unsigned char bits1, bits2;

    bits1 = base64_codetovaluep1[in[0]];
    bits2 = base64_codetovaluep1[in[1]];

    if ((bits1 == 0) || (bits2 == 0))
	return -1;

    num = ((PRUint32)(bits1 - 1)) << 2;
    num |= ((PRUint32)(bits2 - 1)) >> 4;

    out[0] = (unsigned char) num;

    return 1;
}

/*
 * Reads 4; writes 0-3.  Returns bytes written or -1 on error.
 * (Writes less than 3 only at (presumed) EOF.)
 */
static int
pl_base64_decode_token (const unsigned char *in, unsigned char *out)
{
    if (in[3] != B64_PAD)
	return pl_base64_decode_4to3 (in, out);

    if (in[2] == B64_PAD)
	return pl_base64_decode_2to1 (in, out);

    return pl_base64_decode_3to2 (in, out);
}

static PRStatus
pl_base64_decode_buffer (PLBase64Decoder *data, const unsigned char *in,
			 PRUint32 length)
{
    unsigned char *out = data->output_buffer;
    unsigned char *token = data->token;
    int i, n = 0;

    i = data->token_size;
    data->token_size = 0;

    while (length > 0) {
	while (i < 4 && length > 0) {
	    /*
	     * XXX Note that the following simply ignores any unexpected
	     * characters.  This is exactly what the original code in
	     * libmime did, and I am leaving it.  We certainly want to skip
	     * over whitespace (we must); this does much more than that.
	     * I am not confident changing it, and I don't want to slow
	     * the processing down doing more complicated checking, but
	     * someone else might have different ideas in the future.
	     */
	    if (base64_codetovaluep1[*in] > 0 || *in == B64_PAD)
		token[i++] = *in;
	    in++;
	    length--;
	}

	if (i < 4) {
	    /* Didn't get enough for a complete token. */
	    data->token_size = i;
	    break;
	}
	i = 0;

	PR_ASSERT((out - data->output_buffer + 3) <= data->output_buflen);

	/*
	 * Assume we are not at the end; the following function only works
	 * for an internal token (no trailing padding characters) but is
	 * faster that way.  If it hits an invalid character (padding) it
	 * will return an error; we break out of the loop and try again
	 * calling the routine that will handle a final token.
	 * Note that we intentionally do it this way rather than explicitly
	 * add a check for padding here (because that would just slow down
	 * the normal case) nor do we rely on checking whether we have more
	 * input to process (because that would also slow it down but also
	 * because we want to allow trailing garbage, especially white space
	 * and cannot tell that without read-ahead, also a slow proposition).
	 * Whew.  Understand?
	 */
	n = pl_base64_decode_4to3 (token, out);
	if (n < 0)
	    break;

	/* Advance "out" by the number of bytes just written to it. */
	out += n;
	n = 0;
    }

    /*
     * See big comment above, before call to pl_base64_decode_4to3.
     * Here we check if we error'd out of loop, and allow for the case
     * that we are processing the last interesting token.  If the routine
     * which should handle padding characters also fails, then we just
     * have bad input and give up.
     */
    if (n < 0) {
	n = pl_base64_decode_token (token, out);
	if (n < 0)
	    return PR_FAILURE;

	out += n;
    }

    /*
     * As explained above, we can get here with more input remaining, but
     * it should be all characters we do not care about (i.e. would be
     * ignored when transferring from "in" to "token" in loop above,
     * except here we choose to ignore extraneous pad characters, too).
     * Swallow it, performing that check.  If we find more characters that
     * we would expect to decode, something is wrong.
     */
    while (length > 0) {
	if (base64_codetovaluep1[*in] > 0)
	    return PR_FAILURE;
	in++;
	length--;
    }

    /* Record the length of decoded data we have left in output_buffer. */
    data->output_length = (PRUint32) (out - data->output_buffer);
    return PR_SUCCESS;
}

/*
 * Flush any remaining buffered characters.  Given well-formed input,
 * this will have nothing to do.  If the input was missing the padding
 * characters at the end, though, there could be 1-3 characters left
 * behind -- we will tolerate that by adding the padding for them.
 */
static PRStatus
pl_base64_decode_flush (PLBase64Decoder *data)
{
    int count;

    /*
     * If no remaining characters, or all are padding (also not well-formed
     * input, but again, be tolerant), then nothing more to do.  (And, that
     * is considered successful.)
     */
    if (data->token_size == 0 || data->token[0] == B64_PAD)
	return PR_SUCCESS;

    /*
     * Assume we have all the interesting input except for some expected
     * padding characters.  Add them and decode the resulting token.
     */
    while (data->token_size < 4)
	data->token[data->token_size++] = B64_PAD;

    data->token_size = 0;	/* so a subsequent flush call is a no-op */

    count = pl_base64_decode_token (data->token,
				    data->output_buffer + data->output_length);
    if (count < 0)
	return PR_FAILURE;

    /*
     * If there is an output function, call it with this last bit of data.
     * Otherwise we are doing all buffered output, and the decoded bytes
     * are now there, we just need to reflect that in the length.
     */
    if (data->output_fn != NULL) {
	PRInt32 output_result;

	PR_ASSERT(data->output_length == 0);
	output_result = data->output_fn (data->output_arg,
					 data->output_buffer,
					 (PRInt32) count);
	if (output_result < 0)
	    return  PR_FAILURE;
    } else {
	data->output_length += count;
    }

    return PR_SUCCESS;
}


/*
 * The maximum space needed to hold the output of the decoder given
 * input data of length "size".
 */
static PRUint32
PL_Base64MaxDecodedLength (PRUint32 size)
{
    return ((size * 3) / 4);
}


/*
 * A distinct internal creation function for the buffer version to use.
 * (It does not want to specify an output_fn, and we want the normal
 * Create function to require that.)  If more common initialization
 * of the decoding context needs to be done, it should be done *here*.
 */
static PLBase64Decoder *
pl_base64_create_decoder (void)
{
    return PR_NEWZAP(PLBase64Decoder);
}

/*
 * Function to start a base64 decoding context.
 * An "output_fn" is required; the "output_arg" parameter to that is optional.
 */
static PLBase64Decoder *
PL_CreateBase64Decoder (PRInt32 (*output_fn) (void *, const unsigned char *,
					      PRInt32),
			void *output_arg)
{
    PLBase64Decoder *data;

    if (output_fn == NULL) {
	PR_SetError (PR_INVALID_ARGUMENT_ERROR, 0);
	return NULL;
    }

    data = pl_base64_create_decoder ();
    if (data != NULL) {
	data->output_fn = output_fn;
	data->output_arg = output_arg;
    }
    return data;
}


/*
 * Push data through the decoder, causing the output_fn (provided to Create)
 * to be called with the decoded data.
 */
static PRStatus
PL_UpdateBase64Decoder (PLBase64Decoder *data, const char *buffer,
			PRUint32 size)
{
    PRUint32 need_length;
    PRStatus status;

    /* XXX Should we do argument checking only in debug build? */
    if (data == NULL || buffer == NULL || size == 0) {
	PR_SetError (PR_INVALID_ARGUMENT_ERROR, 0);
	return PR_FAILURE;
    }

    /*
     * How much space could this update need for decoding?
     */
    need_length = PL_Base64MaxDecodedLength (size + data->token_size);

    /*
     * Make sure we have at least that much.  If not, (re-)allocate.
     */
    if (need_length > data->output_buflen) {
	unsigned char *output_buffer = data->output_buffer;

	if (output_buffer != NULL)
	    output_buffer = (unsigned char *) PR_Realloc(output_buffer,
							 need_length);
	else
	    output_buffer = (unsigned char *) PR_Malloc(need_length);

	if (output_buffer == NULL)
	    return PR_FAILURE;

	data->output_buffer = output_buffer;
	data->output_buflen = need_length;
    }

    /* There should not have been any leftover output data in the buffer. */
    PR_ASSERT(data->output_length == 0);
    data->output_length = 0;

    status = pl_base64_decode_buffer (data, (const unsigned char *) buffer,
				      size);

    /* Now that we have some decoded data, write it. */
    if (status == PR_SUCCESS && data->output_length > 0) {
	PRInt32 output_result;

	PR_ASSERT(data->output_fn != NULL);
	output_result = data->output_fn (data->output_arg,
					 data->output_buffer,
					 (PRInt32) data->output_length);
	if (output_result < 0)
	    status = PR_FAILURE;
    }

    data->output_length = 0;
    return status;
}


/*
 * When you're done decoding, call this to free the data.  If "abort_p"
 * is false, then calling this may cause the output_fn to be called
 * one last time (as the last buffered data is flushed out).
 */
static PRStatus
PL_DestroyBase64Decoder (PLBase64Decoder *data, PRBool abort_p)
{
    PRStatus status = PR_SUCCESS;

    /* XXX Should we do argument checking only in debug build? */
    if (data == NULL) {
	PR_SetError (PR_INVALID_ARGUMENT_ERROR, 0);
	return PR_FAILURE;
    }

    /* Flush out the last few buffered characters. */
    if (!abort_p)
	status = pl_base64_decode_flush (data);

    if (data->output_buffer != NULL)
	PR_Free(data->output_buffer);
    PR_Free(data);

    return status;
}


/*
 * Perform base64 decoding from an input buffer to an output buffer.
 * The output buffer can be provided (as "dest"); you can also pass in
 * a NULL and this function will allocate a buffer large enough for you,
 * and return it.  If you do provide the output buffer, you must also
 * provide the maximum length of that buffer (as "maxdestlen").
 * The actual decoded length of output will be returned to you in
 * "output_destlen".
 *
 * Return value is NULL on error, the output buffer (allocated or provided)
 * otherwise.
 */
static unsigned char *
PL_Base64DecodeBuffer (const char *src, PRUint32 srclen, unsigned char *dest,
		       PRUint32 maxdestlen, PRUint32 *output_destlen)
{
    PRUint32 need_length;
    unsigned char *output_buffer = NULL;
    PLBase64Decoder *data = NULL;
    PRStatus status;

    if (srclen == 0) {
	*output_destlen = 0;
	if (dest == NULL) {
	    /* PR_Malloc(0) is undefined */
	    return (unsigned char *) PR_Malloc(1);
	}
	return dest;
    }

    /*
     * How much space could we possibly need for decoding this input?
     */
    need_length = PL_Base64MaxDecodedLength (srclen);

    /*
     * Make sure we have at least that much, if output buffer provided.
     * If no output buffer provided, then we allocate that much.
     */
    if (dest != NULL) {
	PR_ASSERT(maxdestlen >= need_length);
	if (maxdestlen < need_length) {
	    PR_SetError(PR_BUFFER_OVERFLOW_ERROR, 0);
	    goto loser;
	}
	output_buffer = dest;
    } else {
	output_buffer = (unsigned char *) PR_Malloc(need_length);
	if (output_buffer == NULL)
	    goto loser;
	maxdestlen = need_length;
    }

    data = pl_base64_create_decoder();
    if (data == NULL)
	goto loser;

    data->output_buflen = maxdestlen;
    data->output_buffer = output_buffer;

    status = pl_base64_decode_buffer (data, (const unsigned char *) src,
				      srclen);

    /*
     * We do not wait for Destroy to flush, because Destroy will also
     * get rid of our decoder context, which we need to look at first!
     */
    if (status == PR_SUCCESS)
	status = pl_base64_decode_flush (data);

    /* Must clear this or Destroy will free it. */
    data->output_buffer = NULL;

    if (status == PR_SUCCESS) {
	*output_destlen = data->output_length;
	status = PL_DestroyBase64Decoder (data, PR_FALSE);
	data = NULL;
	if (status == PR_FAILURE)
	    goto loser;
	return output_buffer;
    }

loser:
    if (dest == NULL && output_buffer != NULL)
	PR_Free(output_buffer);
    if (data != NULL)
	(void) PL_DestroyBase64Decoder (data, PR_TRUE);
    return NULL;
}


/*
 * XXX End of base64 decoding code to be moved into NSPR.
 ********************************************************
 */

/*
 * This is the beginning of the NSS cover functions.  These will
 * provide the interface we want to expose as NSS-ish.  For example,
 * they will operate on our Items, do any special handling or checking
 * we want to do, etc.
 */


PR_BEGIN_EXTERN_C

/*
 * A boring cover structure for now.  Perhaps someday it will include
 * some more interesting fields.
 */
struct NSSBase64DecoderStr {
    PLBase64Decoder *pl_data;
};

PR_END_EXTERN_C


/*
 * Function to start a base64 decoding context.
 */
NSSBase64Decoder *
NSSBase64Decoder_Create (PRInt32 (*output_fn) (void *, const unsigned char *,
					       PRInt32),
			 void *output_arg)
{
    PLBase64Decoder *pl_data;
    NSSBase64Decoder *nss_data;

    nss_data = PORT_ZNew(NSSBase64Decoder);
    if (nss_data == NULL)
	return NULL;

    pl_data = PL_CreateBase64Decoder (output_fn, output_arg);
    if (pl_data == NULL) {
	PORT_Free(nss_data);
	return NULL;
    }

    nss_data->pl_data = pl_data;
    return nss_data;
}


/*
 * Push data through the decoder, causing the output_fn (provided to Create)
 * to be called with the decoded data.
 */
SECStatus
NSSBase64Decoder_Update (NSSBase64Decoder *data, const char *buffer,
			 PRUint32 size)
{
    PRStatus pr_status;

    /* XXX Should we do argument checking only in debug build? */
    if (data == NULL) {
	PORT_SetError (SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    pr_status = PL_UpdateBase64Decoder (data->pl_data, buffer, size);
    if (pr_status == PR_FAILURE)
	return SECFailure;

    return SECSuccess;
}


/*
 * When you're done decoding, call this to free the data.  If "abort_p"
 * is false, then calling this may cause the output_fn to be called
 * one last time (as the last buffered data is flushed out).
 */
SECStatus
NSSBase64Decoder_Destroy (NSSBase64Decoder *data, PRBool abort_p)
{
    PRStatus pr_status;

    /* XXX Should we do argument checking only in debug build? */
    if (data == NULL) {
	PORT_SetError (SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    pr_status = PL_DestroyBase64Decoder (data->pl_data, abort_p);

    PORT_Free(data);

    if (pr_status == PR_FAILURE)
	return SECFailure;

    return SECSuccess;
}


/*
 * Perform base64 decoding from an ascii string "inStr" to an Item.
 * The length of the input must be provided as "inLen".  The Item
 * may be provided (as "outItemOpt"); you can also pass in a NULL
 * and the Item will be allocated for you.
 *
 * In any case, the data within the Item will be allocated for you.
 * All allocation will happen out of the passed-in "arenaOpt", if non-NULL.
 * If "arenaOpt" is NULL, standard allocation (heap) will be used and
 * you will want to free the result via SECITEM_FreeItem.
 *
 * Return value is NULL on error, the Item (allocated or provided) otherwise.
 */
SECItem *
NSSBase64_DecodeBuffer (PRArenaPool *arenaOpt, SECItem *outItemOpt,
			const char *inStr, unsigned int inLen)
{
    SECItem *out_item = outItemOpt;
    PRUint32 max_out_len = PL_Base64MaxDecodedLength (inLen);
    PRUint32 out_len;
    void *mark = NULL;
    unsigned char *dummy;

    PORT_Assert(outItemOpt == NULL || outItemOpt->data == NULL);

    if (arenaOpt != NULL)
	mark = PORT_ArenaMark (arenaOpt);

    out_item = SECITEM_AllocItem (arenaOpt, outItemOpt, max_out_len);
    if (out_item == NULL) {
	if (arenaOpt != NULL)
	    PORT_ArenaRelease (arenaOpt, mark);
	return NULL;
    }

    dummy = PL_Base64DecodeBuffer (inStr, inLen, out_item->data,
				   max_out_len, &out_len);
    if (dummy == NULL) {
	if (arenaOpt != NULL) {
	    PORT_ArenaRelease (arenaOpt, mark);
	    if (outItemOpt != NULL) {
		outItemOpt->data = NULL;
		outItemOpt->len = 0;
	    }
	} else {
	    SECITEM_FreeItem (out_item,
			      (outItemOpt == NULL) ? PR_TRUE : PR_FALSE);
	}
	return NULL;
    }

    if (arenaOpt != NULL)
	PORT_ArenaUnmark (arenaOpt, mark);
    out_item->len = out_len;
    return out_item;
}


/*
 * XXX Everything below is deprecated.  If you add new stuff, put it
 * *above*, not below.
 */

/*
 * XXX The following "ATOB" functions are provided for backward compatibility
 * with current code.  They should be considered strongly deprecated.
 * When we can convert all our code over to using the new NSSBase64Decoder_
 * functions defined above, we should get rid of these altogether.  (Remove
 * protoypes from base64.h as well -- actually, remove that file completely).
 * If someone thinks either of these functions provides such a very useful
 * interface (though, as shown, the same functionality can already be
 * obtained by calling NSSBase64_DecodeBuffer directly), fine -- but then
 * that API should be provided with a nice new NSSFoo name and using
 * appropriate types, etc.
 */

#include "base64.h"

/*
** Return an PORT_Alloc'd string which is the base64 decoded version
** of the input string; set *lenp to the length of the returned data.
*/
unsigned char *
ATOB_AsciiToData(const char *string, unsigned int *lenp)
{
    SECItem binary_item, *dummy;

    binary_item.data = NULL;
    binary_item.len = 0;

    dummy = NSSBase64_DecodeBuffer (NULL, &binary_item, string,
				    (PRUint32) PORT_Strlen(string));
    if (dummy == NULL)
	return NULL;

    PORT_Assert(dummy == &binary_item);

    *lenp = dummy->len;
    return dummy->data;
}
 
/*
** Convert from ascii to binary encoding of an item.
*/
SECStatus
ATOB_ConvertAsciiToItem(SECItem *binary_item, char *ascii)
{
    SECItem *dummy;

    if (binary_item == NULL) {
	PORT_SetError (SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /*
     * XXX Would prefer to assert here if data is non-null (actually,
     * don't need to, just let NSSBase64_DecodeBuffer do it), so as to
     * to catch unintended memory leaks, but callers are not clean in
     * this respect so we need to explicitly clear here to avoid the
     * assert in NSSBase64_DecodeBuffer.
     */
    binary_item->data = NULL;
    binary_item->len = 0;

    dummy = NSSBase64_DecodeBuffer (NULL, binary_item, ascii,
				    (PRUint32) PORT_Strlen(ascii));

    if (dummy == NULL)
	return SECFailure;

    return SECSuccess;
}
