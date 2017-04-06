/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base64 encoding (binary to ascii).
 */

#include "nssb64.h"
#include "nspr.h"
#include "secitem.h"
#include "secerr.h"

/*
 * XXX See the big comment at the top of nssb64d.c about moving the
 * bulk of this code over into NSPR (the PL part).  It all applies
 * here but I didn't want to duplicate it, to avoid divergence problems.
 */

/*
 **************************************************************
 * XXX Beginning of base64 encoding code to be moved into NSPR.
 */

struct PLBase64EncodeStateStr {
    unsigned chunks;
    unsigned saved;
    unsigned char buf[3];
};

/*
 * This typedef would belong in the NSPR header file (i.e. plbase64.h).
 */
typedef struct PLBase64EncoderStr PLBase64Encoder;

/*
 * The following implementation of base64 encoding was based on code
 * found in libmime (specifically, in mimeenc.c).  It has been adapted to
 * use PR types and naming as well as to provide other necessary semantics
 * (like buffer-in/buffer-out in addition to "streaming" without undue
 * performance hit of extra copying if you made the buffer versions
 * use the output_fn).  It also incorporates some aspects of the current
 * NSPR base64 encoding code.  As such, you may find similarities to
 * both of those implementations.  I tried to use names that reflected
 * the original code when possible.  For this reason you may find some
 * inconsistencies -- libmime used lots of "in" and "out" whereas the
 * NSPR version uses "src" and "dest"; sometimes I changed one to the other
 * and sometimes I left them when I thought the subroutines were at least
 * self-consistent.
 */

PR_BEGIN_EXTERN_C

/*
 * Opaque object used by the encoder to store state.
 */
struct PLBase64EncoderStr {
    /*
     * The one or two bytes pending.  (We need 3 to create a "token",
     * and hold the leftovers here.  in_buffer_count is *only* ever
     * 0, 1, or 2.
     */
    unsigned char in_buffer[2];
    int in_buffer_count;

    /*
     * If the caller wants linebreaks added, line_length specifies
     * where they come out.  It must be a multiple of 4; if the caller
     * provides one that isn't, we round it down to the nearest
     * multiple of 4.
     *
     * The value of current_column counts how many characters have been
     * added since the last linebreaks (or since the beginning, on the
     * first line).  It is also always a multiple of 4; it is unused when
     * line_length is 0.
     */
    PRUint32 line_length;
    PRUint32 current_column;

    /*
     * Where to write the encoded data (used when streaming, not when
     * doing all in-memory (buffer) operations).
     *
     * Note that this definition is chosen to be compatible with PR_Write.
     */
    PRInt32 (*output_fn)(void *output_arg, const char *buf, PRInt32 size);
    void *output_arg;

    /*
     * Where the encoded output goes -- either temporarily (in the streaming
     * case, staged here before it goes to the output function) or what will
     * be the entire buffered result for users of the buffer version.
     */
    char *output_buffer;
    PRUint32 output_buflen; /* the total length of allocated buffer */
    PRUint32 output_length; /* the length that is currently populated */
};

PR_END_EXTERN_C

/*
 * Table to convert a binary value to its corresponding ascii "code".
 */
static unsigned char base64_valuetocode[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define B64_PAD '='
#define B64_CR '\r'
#define B64_LF '\n'

static PRStatus
pl_base64_encode_buffer(PLBase64Encoder *data, const unsigned char *in,
                        PRUint32 size)
{
    const unsigned char *end = in + size;
    char *out = data->output_buffer + data->output_length;
    unsigned int i = data->in_buffer_count;
    PRUint32 n = 0;
    int off;
    PRUint32 output_threshold;

    /* If this input buffer is too small, wait until next time. */
    if (size < (3 - i)) {
        data->in_buffer[i++] = in[0];
        if (size > 1)
            data->in_buffer[i++] = in[1];
        PR_ASSERT(i < 3);
        data->in_buffer_count = i;
        return PR_SUCCESS;
    }

    /* If there are bytes that were put back last time, take them now. */
    if (i > 0) {
        n = data->in_buffer[0];
        if (i > 1)
            n = (n << 8) | data->in_buffer[1];
        data->in_buffer_count = 0;
    }

    /* If our total is not a multiple of three, put one or two bytes back. */
    off = (size + i) % 3;
    if (off > 0) {
        size -= off;
        data->in_buffer[0] = in[size];
        if (off > 1)
            data->in_buffer[1] = in[size + 1];
        data->in_buffer_count = off;
        end -= off;
    }

    output_threshold = data->output_buflen - 3;

    /*
     * Populate the output buffer with base64 data, one line (or buffer)
     * at a time.
     */
    while (in < end) {
        int j, k;

        while (i < 3) {
            n = (n << 8) | *in++;
            i++;
        }
        i = 0;

        if (data->line_length > 0) {
            if (data->current_column >= data->line_length) {
                data->current_column = 0;
                *out++ = B64_CR;
                *out++ = B64_LF;
                data->output_length += 2;
            }
            data->current_column += 4; /* the bytes we are about to add */
        }

        for (j = 18; j >= 0; j -= 6) {
            k = (n >> j) & 0x3F;
            *out++ = base64_valuetocode[k];
        }
        n = 0;
        data->output_length += 4;

        if (data->output_length >= output_threshold) {
            PR_ASSERT(data->output_length <= data->output_buflen);
            if (data->output_fn != NULL) {
                PRInt32 output_result;

                output_result = data->output_fn(data->output_arg,
                                                data->output_buffer,
                                                (PRInt32)data->output_length);
                if (output_result < 0)
                    return PR_FAILURE;

                out = data->output_buffer;
                data->output_length = 0;
            } else {
                /*
                 * Check that we are about to exit the loop.  (Since we
                 * are over the threshold, there isn't enough room in the
                 * output buffer for another trip around.)
                 */
                PR_ASSERT(in == end);
                if (in < end) {
                    PR_SetError(PR_BUFFER_OVERFLOW_ERROR, 0);
                    return PR_FAILURE;
                }
            }
        }
    }

    return PR_SUCCESS;
}

static PRStatus
pl_base64_encode_flush(PLBase64Encoder *data)
{
    int i = data->in_buffer_count;

    if (i == 0 && data->output_length == 0)
        return PR_SUCCESS;

    if (i > 0) {
        char *out = data->output_buffer + data->output_length;
        PRUint32 n;
        int j, k;

        n = ((PRUint32)data->in_buffer[0]) << 16;
        if (i > 1)
            n |= ((PRUint32)data->in_buffer[1] << 8);

        data->in_buffer_count = 0;

        if (data->line_length > 0) {
            if (data->current_column >= data->line_length) {
                data->current_column = 0;
                *out++ = B64_CR;
                *out++ = B64_LF;
                data->output_length += 2;
            }
        }

        /*
         * This will fill in more than we really have data for, but the
         * valid parts will end up in the correct position and the extras
         * will be over-written with pad characters below.
         */
        for (j = 18; j >= 0; j -= 6) {
            k = (n >> j) & 0x3F;
            *out++ = base64_valuetocode[k];
        }

        /* Pad with equal-signs. */
        if (i == 1)
            out[-2] = B64_PAD;
        out[-1] = B64_PAD;

        data->output_length += 4;
    }

    if (data->output_fn != NULL) {
        PRInt32 output_result;

        output_result = data->output_fn(data->output_arg, data->output_buffer,
                                        (PRInt32)data->output_length);
        data->output_length = 0;

        if (output_result < 0)
            return PR_FAILURE;
    }

    return PR_SUCCESS;
}

/*
 * The maximum space needed to hold the output of the encoder given input
 * data of length "size", and allowing for CRLF added at least every
 * line_length bytes (we will add it at nearest lower multiple of 4).
 * There is no trailing CRLF.
 */
static PRUint32
PL_Base64MaxEncodedLength(PRUint32 size, PRUint32 line_length)
{
    PRUint32 tokens, tokens_per_line, full_lines, line_break_chars, remainder;

    /* This is the maximum length we support. */
    if (size > 0x3fffffff) {
        return 0;
    }

    tokens = (size + 2) / 3;

    if (line_length == 0) {
        return tokens * 4;
    }

    if (line_length < 4) { /* too small! */
        line_length = 4;
    }

    tokens_per_line = line_length / 4;
    full_lines = tokens / tokens_per_line;
    remainder = (tokens - (full_lines * tokens_per_line)) * 4;
    line_break_chars = full_lines * 2;
    if (remainder == 0) {
        line_break_chars -= 2;
    }

    return (full_lines * tokens_per_line * 4) + line_break_chars + remainder;
}

/*
 * A distinct internal creation function for the buffer version to use.
 * (It does not want to specify an output_fn, and we want the normal
 * Create function to require that.)  All common initialization of the
 * encoding context should be done *here*.
 *
 * Save "line_length", rounded down to nearest multiple of 4 (if not
 * already even multiple).  Allocate output_buffer, if not provided --
 * based on given size if specified, otherwise based on line_length.
 */
static PLBase64Encoder *
pl_base64_create_encoder(PRUint32 line_length, char *output_buffer,
                         PRUint32 output_buflen)
{
    PLBase64Encoder *data;
    PRUint32 line_tokens;

    data = PR_NEWZAP(PLBase64Encoder);
    if (data == NULL)
        return NULL;

    if (line_length > 0 && line_length < 4) /* too small! */
        line_length = 4;

    line_tokens = line_length / 4;
    data->line_length = line_tokens * 4;

    if (output_buffer == NULL) {
        if (output_buflen == 0) {
            if (data->line_length > 0) /* need to include room for CRLF */
                output_buflen = data->line_length + 2;
            else
                output_buflen = 64; /* XXX what is a good size? */
        }

        output_buffer = (char *)PR_Malloc(output_buflen);
        if (output_buffer == NULL) {
            PR_Free(data);
            return NULL;
        }
    }

    data->output_buffer = output_buffer;
    data->output_buflen = output_buflen;
    return data;
}

/*
 * Function to start a base64 encoding context.
 * An "output_fn" is required; the "output_arg" parameter to that is optional.
 * If linebreaks in the encoded output are desired, "line_length" specifies
 * where to place them -- it will be rounded down to the nearest multiple of 4
 * (if it is not already an even multiple of 4).  If it is zero, no linebreaks
 * will be added.  (FYI, a linebreak is CRLF -- two characters.)
 */
static PLBase64Encoder *
PL_CreateBase64Encoder(PRInt32 (*output_fn)(void *, const char *, PRInt32),
                       void *output_arg, PRUint32 line_length)
{
    PLBase64Encoder *data;

    if (output_fn == NULL) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return NULL;
    }

    data = pl_base64_create_encoder(line_length, NULL, 0);
    if (data == NULL)
        return NULL;

    data->output_fn = output_fn;
    data->output_arg = output_arg;

    return data;
}

/*
 * Push data through the encoder, causing the output_fn (provided to Create)
 * to be called with the encoded data.
 */
static PRStatus
PL_UpdateBase64Encoder(PLBase64Encoder *data, const unsigned char *buffer,
                       PRUint32 size)
{
    /* XXX Should we do argument checking only in debug build? */
    if (data == NULL || buffer == NULL || size == 0) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    return pl_base64_encode_buffer(data, buffer, size);
}

/*
 * When you're done encoding, call this to free the data.  If "abort_p"
 * is false, then calling this may cause the output_fn to be called
 * one last time (as the last buffered data is flushed out).
 */
static PRStatus
PL_DestroyBase64Encoder(PLBase64Encoder *data, PRBool abort_p)
{
    PRStatus status = PR_SUCCESS;

    /* XXX Should we do argument checking only in debug build? */
    if (data == NULL) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    /* Flush out the last few buffered characters. */
    if (!abort_p)
        status = pl_base64_encode_flush(data);

    if (data->output_buffer != NULL)
        PR_Free(data->output_buffer);
    PR_Free(data);

    return status;
}

/*
 * Perform base64 encoding from an input buffer to an output buffer.
 * The output buffer can be provided (as "dest"); you can also pass in
 * a NULL and this function will allocate a buffer large enough for you,
 * and return it.  If you do provide the output buffer, you must also
 * provide the maximum length of that buffer (as "maxdestlen").
 * The actual encoded length of output will be returned to you in
 * "output_destlen".
 *
 * If linebreaks in the encoded output are desired, "line_length" specifies
 * where to place them -- it will be rounded down to the nearest multiple of 4
 * (if it is not already an even multiple of 4).  If it is zero, no linebreaks
 * will be added.  (FYI, a linebreak is CRLF -- two characters.)
 *
 * Return value is NULL on error, the output buffer (allocated or provided)
 * otherwise.
 */
static char *
PL_Base64EncodeBuffer(const unsigned char *src, PRUint32 srclen,
                      PRUint32 line_length, char *dest, PRUint32 maxdestlen,
                      PRUint32 *output_destlen)
{
    PRUint32 need_length;
    PLBase64Encoder *data = NULL;
    PRStatus status;

    PR_ASSERT(srclen > 0);
    if (srclen == 0) {
        return dest;
    }

    /*
     * How much space could we possibly need for encoding this input?
     */
    need_length = PL_Base64MaxEncodedLength(srclen, line_length);
    if (need_length == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    /*
     * Make sure we have at least that much, if output buffer provided.
     */
    if (dest != NULL) {
        PR_ASSERT(maxdestlen >= need_length);
        if (maxdestlen < need_length) {
            PR_SetError(PR_BUFFER_OVERFLOW_ERROR, 0);
            return NULL;
        }
    } else {
        maxdestlen = need_length;
    }

    data = pl_base64_create_encoder(line_length, dest, maxdestlen);
    if (data == NULL)
        return NULL;

    status = pl_base64_encode_buffer(data, src, srclen);

    /*
     * We do not wait for Destroy to flush, because Destroy will also
     * get rid of our encoder context, which we need to look at first!
     */
    if (status == PR_SUCCESS)
        status = pl_base64_encode_flush(data);

    if (status != PR_SUCCESS) {
        (void)PL_DestroyBase64Encoder(data, PR_TRUE);
        return NULL;
    }

    dest = data->output_buffer;

    /* Must clear this or Destroy will free it. */
    data->output_buffer = NULL;

    *output_destlen = data->output_length;
    status = PL_DestroyBase64Encoder(data, PR_FALSE);
    if (status == PR_FAILURE) {
        PR_Free(dest);
        return NULL;
    }

    return dest;
}

/*
 * XXX End of base64 encoding code to be moved into NSPR.
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
struct NSSBase64EncoderStr {
    PLBase64Encoder *pl_data;
};

PR_END_EXTERN_C

/*
 * Function to start a base64 encoding context.
 */
NSSBase64Encoder *
NSSBase64Encoder_Create(PRInt32 (*output_fn)(void *, const char *, PRInt32),
                        void *output_arg)
{
    PLBase64Encoder *pl_data;
    NSSBase64Encoder *nss_data;

    nss_data = PORT_ZNew(NSSBase64Encoder);
    if (nss_data == NULL)
        return NULL;

    pl_data = PL_CreateBase64Encoder(output_fn, output_arg, 64);
    if (pl_data == NULL) {
        PORT_Free(nss_data);
        return NULL;
    }

    nss_data->pl_data = pl_data;
    return nss_data;
}

/*
 * Push data through the encoder, causing the output_fn (provided to Create)
 * to be called with the encoded data.
 */
SECStatus
NSSBase64Encoder_Update(NSSBase64Encoder *data, const unsigned char *buffer,
                        PRUint32 size)
{
    PRStatus pr_status;

    /* XXX Should we do argument checking only in debug build? */
    if (data == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    pr_status = PL_UpdateBase64Encoder(data->pl_data, buffer, size);
    if (pr_status == PR_FAILURE)
        return SECFailure;

    return SECSuccess;
}

/*
 * When you're done encoding, call this to free the data.  If "abort_p"
 * is false, then calling this may cause the output_fn to be called
 * one last time (as the last buffered data is flushed out).
 */
SECStatus
NSSBase64Encoder_Destroy(NSSBase64Encoder *data, PRBool abort_p)
{
    PRStatus pr_status;

    /* XXX Should we do argument checking only in debug build? */
    if (data == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    pr_status = PL_DestroyBase64Encoder(data->pl_data, abort_p);

    PORT_Free(data);

    if (pr_status == PR_FAILURE)
        return SECFailure;

    return SECSuccess;
}

/*
 * Perform base64 encoding of binary data "inItem" to an ascii string.
 * The output buffer may be provided (as "outStrOpt"); you can also pass
 * in a NULL and the buffer will be allocated for you.  The result will
 * be null-terminated, and if the buffer is provided, "maxOutLen" must
 * specify the maximum length of the buffer and will be checked to
 * supply sufficient space space for the encoded result.  (If "outStrOpt"
 * is NULL, "maxOutLen" is ignored.)
 *
 * If "outStrOpt" is NULL, allocation will happen out of the passed-in
 * "arenaOpt", if *it* is non-NULL, otherwise standard allocation (heap)
 * will be used.
 *
 * Return value is NULL on error, the output buffer (allocated or provided)
 * otherwise.
 */
char *
NSSBase64_EncodeItem(PLArenaPool *arenaOpt, char *outStrOpt,
                     unsigned int maxOutLen, SECItem *inItem)
{
    char *out_string = outStrOpt;
    PRUint32 max_out_len;
    PRUint32 out_len = 0;
    void *mark = NULL;
    char *dummy;

    PORT_Assert(inItem != NULL && inItem->data != NULL && inItem->len != 0);
    if (inItem == NULL || inItem->data == NULL || inItem->len == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    max_out_len = PL_Base64MaxEncodedLength(inItem->len, 64);
    if (max_out_len == 0) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    if (arenaOpt != NULL)
        mark = PORT_ArenaMark(arenaOpt);

    if (out_string == NULL) {
        if (arenaOpt != NULL)
            out_string = PORT_ArenaAlloc(arenaOpt, max_out_len + 1);
        else
            out_string = PORT_Alloc(max_out_len + 1);

        if (out_string == NULL) {
            if (arenaOpt != NULL)
                PORT_ArenaRelease(arenaOpt, mark);
            return NULL;
        }
    } else {
        if ((max_out_len + 1) > maxOutLen) {
            PORT_SetError(SEC_ERROR_OUTPUT_LEN);
            return NULL;
        }
        max_out_len = maxOutLen;
    }

    dummy = PL_Base64EncodeBuffer(inItem->data, inItem->len, 64,
                                  out_string, max_out_len, &out_len);
    if (dummy == NULL) {
        if (arenaOpt != NULL) {
            PORT_ArenaRelease(arenaOpt, mark);
        } else {
            PORT_Free(out_string);
        }
        return NULL;
    }

    if (arenaOpt != NULL)
        PORT_ArenaUnmark(arenaOpt, mark);

    out_string[out_len] = '\0';
    return out_string;
}

/*
 * XXX Everything below is deprecated.  If you add new stuff, put it
 * *above*, not below.
 */

/*
 * XXX The following "BTOA" functions are provided for backward compatibility
 * with current code.  They should be considered strongly deprecated.
 * When we can convert all our code over to using the new NSSBase64Encoder_
 * functions defined above, we should get rid of these altogether.  (Remove
 * protoypes from base64.h as well -- actually, remove that file completely).
 * If someone thinks either of these functions provides such a very useful
 * interface (though, as shown, the same functionality can already be
 * obtained by calling NSSBase64_EncodeItem directly), fine -- but then
 * that API should be provided with a nice new NSSFoo name and using
 * appropriate types, etc.
 */

#include "base64.h"

/*
** Return an PORT_Alloc'd ascii string which is the base64 encoded
** version of the input string.
*/
char *
BTOA_DataToAscii(const unsigned char *data, unsigned int len)
{
    SECItem binary_item;

    binary_item.data = (unsigned char *)data;
    binary_item.len = len;

    return NSSBase64_EncodeItem(NULL, NULL, 0, &binary_item);
}

/*
** Convert from binary encoding of an item to ascii.
*/
char *
BTOA_ConvertItemToAscii(SECItem *binary_item)
{
    return NSSBase64_EncodeItem(NULL, NULL, 0, binary_item);
}
