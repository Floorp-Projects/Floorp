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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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
