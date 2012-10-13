/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "pmhutils.h"
#include "phone.h"


#define WSTREAM_START_SIZE         1024

pmhRstream_t *
pmhutils_rstream_create (char *buf, uint32_t nbytes)
{
    pmhRstream_t *pmhRstream;

    if (!buf || (nbytes == 0)) {
        return NULL;
    }

    pmhRstream = (pmhRstream_t *) cpr_malloc(sizeof(pmhRstream_t));
    if (!pmhRstream) {
        return NULL;
    }

    pmhRstream->eof = pmhRstream->error = FALSE;
    pmhRstream->buff = pmhRstream->loc = buf;
    pmhRstream->nbytes = nbytes;
    pmhRstream->bytes_read = 0;

    return pmhRstream;
}

void
pmhutils_rstream_delete (pmhRstream_t *pmhRstream, boolean freebuf)
{
    if (!pmhRstream) {
        return;
    }

    pmhRstream->error = TRUE;

    if (freebuf && pmhRstream->buff) {
        cpr_free(pmhRstream->buff);
    }
}

char *
pmhutils_rstream_read_bytes (pmhRstream_t *pmhRstream, int32_t nbytes)
{
    char *ret;

    if (!pmhRstream || !pmhRstream->loc || (pmhRstream->eof == TRUE)) {
        return NULL;
    }

    if (pmhRstream->bytes_read >= pmhRstream->nbytes) {
        pmhRstream->eof = TRUE;
        return NULL;
    }

    if ((pmhRstream->nbytes - pmhRstream->bytes_read) < (int32_t) nbytes) {
        return NULL;
    }
    ret = (char *) cpr_malloc((nbytes + 1) * sizeof(char));
    if (ret == NULL) {
        // Addition of 1 byte (NULL) will save us from strlen errr
        return NULL;
    }
    memcpy(ret, pmhRstream->loc, nbytes);
    ret[nbytes] = 0;              /* ensure null terminating character */
    pmhRstream->bytes_read += nbytes;
    pmhRstream->loc += nbytes;

    return (ret);
}


#define SANITY_LINE_SIZE PKTBUF_SIZ

unsigned short linesize = 80;
unsigned short incr_size = 32;

char *
pmhutils_rstream_read_line (pmhRstream_t *pmhRstream)
{
    char *ret_line;
    char *new_loc = NULL;
    int offset, bytes_allocated;
    boolean line_break;

    if (!pmhRstream || !pmhRstream->loc || (pmhRstream->eof == TRUE)) {
        return NULL;
    }

    if (pmhRstream->bytes_read >= pmhRstream->nbytes) {
        pmhRstream->eof = TRUE;
        return NULL;
    }

    new_loc = pmhRstream->loc;

    ret_line = (char *) cpr_malloc(linesize);
    if (ret_line == NULL) {
        return NULL;
    }
    offset = 0;
    bytes_allocated = linesize;
    /* Search for the first occurrence of a line break */
    while (1) {
        line_break = FALSE;
        if (*new_loc == '\r') {
            line_break = TRUE;
            new_loc++;
        }
        if (*new_loc == '\n') {
            line_break = TRUE;
            new_loc++;
        }
        if (line_break) {
            if (*new_loc != ' ' && *new_loc != '\t') {
                break;
            }
        }

        if (offset == (bytes_allocated - 1)) {
            char *newbuf;

            bytes_allocated += incr_size;
            newbuf = (char *) cpr_realloc(ret_line, bytes_allocated);
            if (newbuf == NULL) {
                if (bytes_allocated != 0) {
                    cpr_free(ret_line);
                }
                return NULL;
            }
            ret_line = newbuf;
        }
        ret_line[offset++] = *new_loc++;
        if (*new_loc == '\0') {
            /* We hit the end of buffer before hitting a line break */
            pmhRstream->eof = TRUE;
            pmhRstream->bytes_read += (new_loc - pmhRstream->loc);
            pmhRstream->loc = new_loc;
            cpr_free(ret_line);
            return NULL;
        }
    }

    pmhRstream->bytes_read += (new_loc - pmhRstream->loc);
    /* Update the internal location pointer and bytes read */

    if (pmhRstream->bytes_read >= pmhRstream->nbytes) {
        pmhRstream->loc = pmhRstream->buff + pmhRstream->nbytes;
        pmhRstream->eof = TRUE;
    } else {
        pmhRstream->loc = pmhRstream->buff + pmhRstream->bytes_read;
    }

    ret_line[offset] = 0;
    return ret_line;
}

pmhWstream_t *
pmhutils_wstream_create_with_buf (char *buf, uint32_t nbytes)
{
    pmhWstream_t *pmhWstream = NULL;

    if (buf && nbytes && (nbytes > 0)) {
        pmhWstream = (pmhWstream_t *) cpr_malloc(sizeof(pmhWstream_t));
        if (!pmhWstream) {
            return (NULL);
        }
        pmhWstream->buff = buf;
        pmhWstream->nbytes = 0;
        pmhWstream->total_bytes = nbytes;
        pmhWstream->growable = FALSE;
    }
    return (pmhWstream);
}


uint32_t
pmhutils_wstream_get_length (pmhWstream_t *ws)
{
    if (ws) {
        return (ws->nbytes);
    } else {
        return (0);
    }
}


void
pmhutils_wstream_delete (pmhWstream_t *pmhWstream, boolean freebuf)
{
    if (pmhWstream && freebuf && pmhWstream->buff) {
        cpr_free(pmhWstream->buff);
    }
}


boolean
pmhutils_wstream_write_line (pmhWstream_t *pmhWstream, char *this_line)
{

    /* Sanity check */
    if (this_line == NULL || strlen(this_line) > SANITY_LINE_SIZE) {
        return FALSE;
    }

    if (!(pmhWstream && this_line)) {
        return FALSE;
    }

    while ((pmhWstream->nbytes + (int32_t)strlen(this_line)) >
            pmhWstream->total_bytes) {
        if (!pmhWstream->growable ||
            (FALSE == pmhutils_wstream_grow(pmhWstream))) {
            return FALSE;
        }
    }

    memcpy(&pmhWstream->buff[pmhWstream->nbytes], this_line,
           strlen(this_line));

    pmhWstream->nbytes += strlen(this_line);
    if (!pmhutils_wstream_write_byte(pmhWstream, '\r')) {
        return FALSE;
    }

    if (!pmhutils_wstream_write_byte(pmhWstream, '\n')) {
        return FALSE;
    }

    return TRUE;
}

boolean
pmhutils_wstream_write_byte (pmhWstream_t *pmhWstream, char c)
{
    if (!pmhWstream) {
        return FALSE;
    }

    if ((pmhWstream->nbytes + 1) > pmhWstream->total_bytes) {
        if (!pmhWstream->growable ||
            (FALSE == pmhutils_wstream_grow(pmhWstream))) {
            return FALSE;
        }
    }

    pmhWstream->buff[pmhWstream->nbytes] = c;
    pmhWstream->nbytes++;

    return TRUE;
}

boolean
pmhutils_wstream_write_bytes (pmhWstream_t *pmhWstream,
                              char *inbuf, uint32_t nbytes)
{

    if (!pmhWstream) {
        return FALSE;
    }

    while ((pmhWstream->nbytes + (int32_t) nbytes) > pmhWstream->total_bytes) {
        if (!pmhWstream->growable ||
            (FALSE == pmhutils_wstream_grow(pmhWstream))) {
            return FALSE;
        }
    }

    memcpy(&pmhWstream->buff[pmhWstream->nbytes], inbuf, nbytes);

    pmhWstream->nbytes += nbytes;

    return TRUE;
}

boolean
pmhutils_wstream_grow (pmhWstream_t *pmhWstream)
{
    char *newbuf;

    if (!pmhWstream || !pmhWstream->buff || !pmhWstream->growable) {
        return FALSE;
    }

    newbuf = (char *) cpr_realloc((void *) pmhWstream->buff,
                                  pmhWstream->total_bytes + WSTREAM_START_SIZE);
    if (newbuf == NULL) {
        if ((pmhWstream->total_bytes + WSTREAM_START_SIZE) != 0) {
            cpr_free(pmhWstream->buff);
        }
        pmhWstream->buff = NULL;
        return FALSE;
    }

    pmhWstream->buff = newbuf;

    pmhWstream->total_bytes += WSTREAM_START_SIZE;

    return TRUE;
}
