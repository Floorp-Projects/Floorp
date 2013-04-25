/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Functions that parse and create HTTP/1.1-like messages(RFC 2068). Basically
 *  code that converts from network(ie text) form to a usable structure
 *  and vice-versa.
 */

#include <errno.h>
#include <limits.h>

#include "plstr.h"
#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "httpish.h"
#include "ccsip_protocol.h"
#include "phone_debug.h"
#include "ccsip_core.h"

//#define HTTPISH_DEBUG if (1)
#define MSG_DELIMIT_SIZE   80
#define TMP_BODY_BUF_SIZE 200
#define CMPC_HEADER_SIZE  256

extern sip_header_t sip_cached_headers[];
httpishMsg_t *
httpish_msg_create (void)
{
    int i;
    httpishMsg_t *msg;

    msg = (httpishMsg_t *) cpr_calloc(1, sizeof(httpishMsg_t));
    if (!msg) {
        return NULL;
    }

    msg->headers = (queuetype *) cpr_calloc(1, sizeof(queuetype));

    if (!msg->headers) {
        cpr_free(msg);
        return NULL;
    }

    msg->retain_flag = FALSE;
    msg->mesg_line = NULL;
    msg->content_length = 0;
    msg->is_complete = FALSE;
    msg->headers_read = FALSE;

    for (i = 0; i < HTTPISH_MAX_BODY_PARTS; i++) {
        msg->mesg_body[i].msgContentType = NULL;
        msg->mesg_body[i].msgBody = NULL;
        msg->mesg_body[i].msgLength = 0;
        msg->mesg_body[i].msgContentId = NULL;
        msg->mesg_body[i].msgContentEnc = SIP_CONTENT_ENCODING_IDENTITY_VALUE;
        msg->mesg_body[i].msgContentDisp = SIP_CONTENT_DISPOSITION_SESSION_VALUE;
        msg->mesg_body[i].msgRequiredHandling = TRUE;
        msg->mesg_body[i].msgContentTypeValue = SIP_CONTENT_TYPE_UNKNOWN_VALUE;
    }

    msg->num_body_parts = 0;
    msg->raw_body = NULL;

    queue_init(msg->headers, 0);

    return msg;
}

int
httpish_strncasecmp(const char *s1, const char* s2, size_t len)
{
  /*This routine is an enhanced version of strncasecmp().
   *It ensures that the two strings being compared for size "len"
   *don't have trailing characters beyond "len" chars.
   *The trailing whitespaces beyond "len" chars is ignored.
   */
    const unsigned char *us1 = (const unsigned char *) s1;
    const unsigned char *us2 = (const unsigned char *) s2;

    /* No match if only one ptr is NULL */
    if ((!s1 && s2) || (s1 && !s2))
        return ((int) (s1 - s2));

    if ((len == 0) || (s1 == s2))
        return 0;

    while (len-- > 0 && toupper(*us1) == toupper(*us2)) {
        if (len == 0 || *us1 == '\0' || *us2 == '\0')
            break;
        us1++;
        us2++;
    }

   if (len == 0 && toupper(*us1) == toupper(*us2)) {
       //all "len" chars are compared, need to look for trailing
       //chars beyond "len" string size. Ignore white spaces.
      while (*(++us1) != '\0') {
          if (*us1 != ' ' && *us1 != '\t') {
               break;
          }
      }

      while (*(++us2) != '\0') {
          if (*us2 != ' ' && *us2 != '\t') {
               break;
          }
      }
   }


   return (toupper(*us1) - toupper(*us2));
}

void
httpish_msg_free (httpishMsg_t *msg)
{
    int i;

    if ((!msg) || (msg->retain_flag == TRUE)) {
        return;
    }

    UTILFREE(msg->mesg_line);

    // Free all body parts
    for (i = 0; i < HTTPISH_MAX_BODY_PARTS; i++) {
        UTILFREE(msg->mesg_body[i].msgContentType);
        UTILFREE(msg->mesg_body[i].msgBody);
        UTILFREE(msg->mesg_body[i].msgContentId);
    }
    UTILFREE(msg->raw_body);

    if (msg->headers) {
        httpish_header *this_header;

        this_header = (httpish_header *) dequeue(msg->headers);
        while (this_header != NULL) {
            UTILFREE(this_header->header);
            UTILFREE(this_header);
            this_header = (httpish_header *) dequeue(msg->headers);
        }
    }

    UTILFREE(msg->headers);
    msg->headers = NULL;

    /* Free the header cache */
    for (i = 0; i < HTTPISH_HEADER_CACHE_SIZE; ++i) {
        if (msg->hdr_cache[i].hdr_start) {
            cpr_free(msg->hdr_cache[i].hdr_start);
        }
    }

    /* Free the httpishMsg_t struct itself */
    cpr_free(msg);
}

boolean
httpish_msg_is_request (httpishMsg_t *msg,
                        const char *schema,
                        int schema_len)
{
    char *loc;

    loc = msg->mesg_line;

    if (!msg->is_complete || !msg->mesg_line) {
        return FALSE;
    }

    /*
     *  There might be a couple of leading spaces. Not allowed,
     *  but still be friendly
     */
    while ((*loc == ' ') && (*loc != '\0')) {
        loc++;
    }

    if (strncmp(loc, schema, schema_len)) {
        return TRUE;
    } else {
        return FALSE;
    }
}


boolean
httpish_msg_is_complete (httpishMsg_t *msg)
{
    return msg->is_complete;
}


hStatus_t
httpish_msg_add_reqline (httpishMsg_t *msg,
                         const char *method,
                         const char *url,
                         const char *version)
{

    uint32_t linesize = 0;

    if (!msg || !method || !url || !version) {
        return HSTATUS_FAILURE;
    }

    if (msg->mesg_line) {
        cpr_free(msg->mesg_line);
    }

    linesize = strlen(method) + 1 + strlen(url) + 1 + strlen(version) + 1;

    msg->mesg_line = (char *) cpr_malloc(linesize * sizeof(char));
    if (!msg->mesg_line) {
        return HSTATUS_FAILURE;
    }

    snprintf(msg->mesg_line, linesize, "%s %s %s", method, url, version);

    return HSTATUS_SUCCESS;
}

hStatus_t
httpish_msg_add_respline (httpishMsg_t *msg,
                          const char *version,
                          uint16_t status_code,
                          const char *reason_phrase)
{
    uint32_t linesize = 0;

    if (!msg || !reason_phrase || !version ||
        (status_code < HTTPISH_MIN_STATUS_CODE)) {
        return HSTATUS_FAILURE;
    }

    if (msg->mesg_line) {
        cpr_free(msg->mesg_line);
    }

    /* Assumes status codes are max 6 characters long */
    linesize = strlen(version) + 1 + 6 + 1 + strlen(reason_phrase) + 1;

    msg->mesg_line = (char *) cpr_malloc(linesize * sizeof(char));
    if (!msg->mesg_line) {
        return HSTATUS_FAILURE;
    }

    snprintf(msg->mesg_line, linesize, "%s %d %s",
             version, status_code, reason_phrase);

    return HSTATUS_SUCCESS;
}


httpishReqLine_t *
httpish_msg_get_reqline (httpishMsg_t *msg)
{
    char *this_token;
    char *msgline;
    httpishReqLine_t *hreq = NULL;
    char *strtok_state;

    if (!msg || !msg->mesg_line || !(msgline = cpr_strdup(msg->mesg_line))) {
        return NULL;
    }

    hreq = (httpishReqLine_t *) cpr_malloc(sizeof(httpishReqLine_t));
    if (!hreq) {
        cpr_free(msgline);
        return NULL;
    }

    this_token = PL_strtok_r(msgline, " ", &strtok_state);

    if (!this_token) {
        cpr_free(hreq);
        cpr_free(msgline);
        return NULL;
    }

    hreq->method = cpr_strdup(this_token);

    this_token = PL_strtok_r(NULL, " ", &strtok_state);

    if (!this_token) {
        cpr_free(hreq->method);
        cpr_free(hreq);
        cpr_free(msgline);
        return NULL;
    }

    hreq->url = cpr_strdup(this_token);

    this_token = PL_strtok_r(NULL, " ", &strtok_state);

    if (!this_token) {
        cpr_free(hreq->method);
        cpr_free(hreq->url);
        cpr_free(hreq);
        cpr_free(msgline);
        return NULL;
    }

    hreq->version = cpr_strdup(this_token);
    cpr_free(msgline);
    return hreq;
}


httpishRespLine_t *
httpish_msg_get_respline (httpishMsg_t *msg)
{
    char *this_token;
    char *msgline;
    httpishRespLine_t *hrsp = NULL;
    char *strtok_state;
    unsigned long strtoul_result;
    char *strtoul_end;

    if (!msg || !msg->mesg_line) {
        return NULL;
    }

    msgline = cpr_strdup(msg->mesg_line);
    if (!msgline) {
        return NULL;
    }

    hrsp = (httpishRespLine_t *) cpr_malloc(sizeof(httpishRespLine_t));

    if (!hrsp) {
        cpr_free(msgline);
        return NULL;
    }

    this_token = PL_strtok_r(msgline, " ", &strtok_state);

    if (!this_token) {
        cpr_free(hrsp);
        cpr_free(msgline);
        return NULL;
    }

    hrsp->version = cpr_strdup(this_token);

    this_token = PL_strtok_r(NULL, " ", &strtok_state);

    if (!this_token) {
        cpr_free(hrsp->version);
        cpr_free(hrsp);
        cpr_free(msgline);
        return NULL;
    }

    errno = 0;
    strtoul_result = strtoul(this_token, &strtoul_end, 10);

    if (errno || this_token == strtoul_end || strtoul_result > USHRT_MAX) {
        cpr_free(hrsp->version);
        cpr_free(hrsp);
        cpr_free(msgline);
        return NULL;
    }

    hrsp->status_code = (uint16_t) strtoul_result;

    this_token = PL_strtok_r(NULL, " ", &strtok_state);

    /* reason phrase is optional */
    if (this_token) {
        hrsp->reason_phrase = cpr_strdup(this_token);
    } else {
        hrsp->reason_phrase = NULL;
    }

    cpr_free(msgline);
    return (hrsp);
}



void
httpish_msg_free_reqline (httpishReqLine_t *rqline)
{
    if (!rqline) {
        return;
    }

    UTILFREE(rqline->method);
    UTILFREE(rqline->url);
    UTILFREE(rqline->version);
}


void
httpish_msg_free_respline (httpishRespLine_t *rspline)
{
    if (!rspline) {
        return;
    }

    UTILFREE(rspline->reason_phrase);
    UTILFREE(rspline->version);
}

hStatus_t
httpish_msg_add_text_header (httpishMsg_t *msg,
                             const char *hname,
                             const char *hval)
{
    uint32_t        linesize = 0;
    httpish_header *this_header = NULL;
    char           *header_line = NULL;

    if (!msg || !hname || !hval) {
        return HSTATUS_FAILURE;
    }

    linesize = strlen(hname) + 2 + strlen(hval) + 1;

    header_line = (char *) cpr_malloc(linesize * sizeof(char));
    if (!header_line)
        return HSTATUS_FAILURE;

    this_header = (httpish_header *) cpr_malloc(sizeof(httpish_header));
    if (!this_header) {
        cpr_free(header_line);
        return HSTATUS_FAILURE;
    }

    snprintf(header_line, linesize, "%s: %s", hname, hval);

    this_header->header = header_line;
    this_header->next = NULL;

    enqueue(msg->headers, (void *) this_header);

    return HSTATUS_SUCCESS;
}


hStatus_t
httpish_msg_add_int_header (httpishMsg_t *msg,
                            const char *hname,
                            int32_t hval)
{
    uint32_t        linesize = 0;
    char           *header_line = NULL;
    httpish_header *this_header = NULL;

    if (!msg || !hname) {
        return HSTATUS_FAILURE;
    }

    /* Assumes the int is less than 10 characters */
    linesize = strlen(hname) + 2 + 10 + 1;

    header_line = (char *) cpr_malloc(linesize * sizeof(char));
    if (!header_line) {
        return HSTATUS_FAILURE;
    }

    this_header = (httpish_header *) cpr_malloc(sizeof(httpish_header));
    if (!this_header) {
        cpr_free(header_line);
        return HSTATUS_FAILURE;
    }

    snprintf(header_line, linesize, "%s: %d", hname, hval);

    this_header->header = header_line;
    this_header->next = NULL;

    enqueue(msg->headers, (void *) this_header);

    return HSTATUS_SUCCESS;
}

const char *
httpish_msg_get_cached_header_val (httpishMsg_t *msg,
                                   int cache_index)
{
    return msg->hdr_cache[cache_index].val_start;
}

int
compact_hdr_cmp (char *this_line,
                 const char *c_hname)
{
    char cmpct_hdr[CMPC_HEADER_SIZE];

    if (c_hname) {
        sstrncpy(cmpct_hdr, c_hname, CMPC_HEADER_SIZE);
        return cpr_strcasecmp(this_line, cmpct_hdr);
    }
    return -1;
}

int
httpish_header_name_val (char *sipHeaderName,  char *this_line)
{
    unsigned int x = 0;
    boolean  nameFound = FALSE;

    if (!sipHeaderName || !this_line) {
       return (SIP_ERROR);
    }

    sipHeaderName[0] = '\0';

    /* Remove the leading white spaces  eg: ......From:  or .....From....: */
    while ((*this_line==' ' || *this_line=='\t') ) {
        this_line++;
    }

     /* Copy the allowed characters for header field name */
    while ((*this_line > 32) && (*this_line < 127) && (x < HTTPISH_HEADER_NAME_SIZE)) {
        if (*this_line == ':') {
            nameFound = TRUE;
            sipHeaderName[x] = '\0';
            break;
        }
        sipHeaderName[x] = *this_line;
        this_line++;
        x++;
    }

    /* Remove trailing white spaces */
     if (nameFound == FALSE && x < HTTPISH_HEADER_NAME_SIZE) {
         while ((*this_line == ' ' || *this_line=='\t') ){
             this_line++;
             if (*this_line == ':') {
                 nameFound = TRUE;
                 sipHeaderName[x] = '\0';
                 break;
             }
         }
     }
     sipHeaderName[HTTPISH_HEADER_NAME_SIZE-1] = '\0';

     if (nameFound) {
         return (SIP_OK);
     } else {
         return (SIP_ERROR);
     }
}

boolean
httpish_msg_header_present (httpishMsg_t *msg,
                            const char *hname)
{
    nexthelper *p;
    char       *this_line = NULL;

    /*
     * To allow case-insensitive compact headers, we need to compare 2
     * characters before we can decide, what the header name is.
     * e.g. Call-ID and Content-Type both start with C.
     * For now assume there is no white space between header name and ":"
     */

    if (!msg || !hname || (msg->headers->count == 0)) {
        return FALSE;
    }

    p = (nexthelper *) msg->headers->qhead;
    while (p) {
        this_line = ((httpish_header *)p)->header;
        if (this_line) {
            /* Remove leading spaces */
            while ((*this_line == ' ') && (*this_line != '\0'))
                this_line++;
            if ((strlen(this_line) >= strlen(hname)) &&
                (cpr_strncasecmp(this_line, hname, strlen(hname))) == 0) {
                return TRUE;
            }
        }
        p = p->next;
    }

    return FALSE;
}
const char *
httpish_msg_get_header_val (httpishMsg_t *msg,
                            const char *hname,
                            const char *c_hname)
{
    static const char fname[] = "httpish_msg_get_header_val";
    nexthelper *p;
    char       *this_line = NULL;
    char       headerName[HTTPISH_HEADER_NAME_SIZE];

    headerName[0] = '\0';

    /*
     * To allow case-insensitive compact headers, we need to compare 2
     * characters before we can decide, what the header name is.
     * e.g. Call-ID and Content-Type both start with C.
     * For now assume there is no white space between header name and ":"
     */

    if (!msg || !hname || (msg->headers->count == 0)) {
        return NULL;
    }

    p = (nexthelper *) msg->headers->qhead;
    while (p) {
        this_line = ((httpish_header *)p)->header;

        if (httpish_header_name_val(headerName, this_line)) {
            CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Invalid Header Passed %s", DEB_F_PREFIX_ARGS(HTTPISH, fname), this_line);
            return (NULL);
        }

        if (this_line) {
            if ((cpr_strcasecmp(headerName, hname) == 0 ||
                 compact_hdr_cmp(headerName, c_hname) == 0)) {
                this_line = strchr(this_line, ':');
                if (this_line) {
                    this_line++;
                    /* Remove leading spaces */
                    while ((*this_line == ' ') && (*this_line != '\0'))
                        this_line++;
                    if (*this_line == '\0')
                        return (NULL);
                    else
                        return ((const char *) this_line);
                }
            }
        }
        p = p->next;
    }
    return NULL;
}

int32_t
httpish_msg_get_content_length (httpishMsg_t *msg)
{
    return msg->content_length;
}

static boolean
httpish_msg_to_wstream (pmhWstream_t *ws,
                        httpishMsg_t *msg)
{
    nexthelper *p;
    char        tmp_body_buf[TMP_BODY_BUF_SIZE];
    int         buf_len, total_length = 0, i, boundary_size;

    if (!pmhutils_wstream_write_line(ws, msg->mesg_line)) {
        return (FALSE);
    }

    p = (nexthelper *) msg->headers->qhead;
    while (p) {
        if (!pmhutils_wstream_write_line(ws,
                (char *) (((httpish_header *)p)->header))) {
            return (FALSE);
        }
        p = p->next;
    }
    if (msg->num_body_parts > 0) {
        if (msg->num_body_parts > 1) {
            // Write out the special Content-Type header and the
            // Mime-Version header and the aggregate Content-Length header
            // followed by the unique boundary
            buf_len = snprintf(tmp_body_buf, TMP_BODY_BUF_SIZE,
                               "%s: multipart/mixed; boundary=%s\r\n",
                               HTTPISH_HEADER_CONTENT_TYPE, uniqueBoundary);

            if (!pmhutils_wstream_write_bytes(ws, tmp_body_buf, buf_len)) {
                return (FALSE);
            }

            buf_len = snprintf(tmp_body_buf, TMP_BODY_BUF_SIZE, "%s: 1.0\r\n",
                               HTTPISH_HEADER_MIME_VERSION);
            if (!pmhutils_wstream_write_bytes(ws, tmp_body_buf, buf_len)) {
                return (FALSE);
            }

            // Traverse the list and calculate the total size of the
            // body
            boundary_size = strlen("\r\n--\r\n") + strlen(uniqueBoundary);
            for (i = 0; i < msg->num_body_parts; i++) {
                total_length += boundary_size;
                total_length += msg->mesg_body[i].msgLength;
                total_length += sizeof(HTTPISH_HEADER_CONTENT_TYPE) + 1;
                switch (msg->mesg_body[i].msgContentTypeValue) {
                default:
                case SIP_CONTENT_TYPE_UNKNOWN_VALUE:
                    total_length += strlen(msg->mesg_body[i].msgContentType) + 2;
                    break;
                case SIP_CONTENT_TYPE_SDP_VALUE:
                    total_length += sizeof(SIP_CONTENT_TYPE_SDP) + 1;
                    break;
                case SIP_CONTENT_TYPE_SIPFRAG_VALUE:
                    total_length += sizeof(SIP_CONTENT_TYPE_SIPFRAG) + 1;
                    break;
                case SIP_CONTENT_TYPE_DIALOG_VALUE:
                    total_length += sizeof(SIP_CONTENT_TYPE_DIALOG) + 1;
                    break;
                case SIP_CONTENT_TYPE_KPML_REQUEST_VALUE:
                    total_length += sizeof(SIP_CONTENT_TYPE_KPML_REQUEST) + 1;
                    break;
                case SIP_CONTENT_TYPE_KPML_RESPONSE_VALUE:
                    total_length += sizeof(SIP_CONTENT_TYPE_KPML_RESPONSE) + 1;
                    break;
                case SIP_CONTENT_TYPE_REMOTECC_REQUEST_VALUE:
                    total_length += sizeof(SIP_CONTENT_TYPE_REMOTECC_REQUEST) + 1;
                    break;
                case SIP_CONTENT_TYPE_REMOTECC_RESPONSE_VALUE:
                    total_length += sizeof(SIP_CONTENT_TYPE_REMOTECC_RESPONSE) + 1;
                    break;
                case SIP_CONTENT_TYPE_CTI_VALUE:
                    total_length += sizeof(SIP_CONTENT_TYPE_CTI) + 1;
                    break;
                case SIP_CONTENT_TYPE_CMXML_VALUE:
                    total_length += sizeof(SIP_CONTENT_TYPE_CMXML) + 1;
                    break;
                case SIP_CONTENT_TYPE_TEXT_PLAIN_VALUE:
                    total_length += sizeof(SIP_CONTENT_TYPE_TEXT_PLAIN) + 1;
                    break;
                case SIP_CONTENT_TYPE_PRESENCE_VALUE:
                    total_length += sizeof(SIP_CONTENT_TYPE_PRESENCE) + 1;
                    break;
                }
                // Now estimate size of Content-Disposition header
                total_length += sizeof(SIP_HEADER_CONTENT_DISP) + 1;
                switch (msg->mesg_body[i].msgContentDisp) {
                case SIP_CONTENT_DISPOSITION_RENDER_VALUE:
                    total_length += sizeof(SIP_CONTENT_DISPOSITION_RENDER) - 1;
                    break;
                case SIP_CONTENT_DISPOSITION_SESSION_VALUE:
                default:
                    total_length += sizeof(SIP_CONTENT_DISPOSITION_SESSION) - 1;
                    break;
                case SIP_CONTENT_DISPOSITION_ICON_VALUE:
                    total_length += sizeof(SIP_CONTENT_DISPOSITION_ICON) - 1;
                    break;
                case SIP_CONTENT_DISPOSITION_ALERT_VALUE:
                    total_length += sizeof(SIP_CONTENT_DISPOSITION_ALERT) - 1;
                    break;
                case SIP_CONTENT_DISPOSITION_PRECONDITION_VALUE:
                    total_length += sizeof(SIP_CONTENT_DISPOSITION_PRECONDITION) - 1;
                    break;
                }
                // Now the handling attribute
                total_length += sizeof(";handling=") - 1;
                if (msg->mesg_body[i].msgRequiredHandling) {
                    total_length += sizeof("required") + 1;
                } else {
                    total_length += sizeof("optional") + 1;
                }
                // Now the content id
                if (msg->mesg_body[i].msgContentId) {
                    total_length += sizeof(HTTPISH_HEADER_CONTENT_ID) + 1 +
                        strlen(msg->mesg_body[i].msgContentId) + 2;

                }
            }
            // Now account for the closing boundary which is 2+boundary_size
            total_length += 2 + boundary_size + 2;

            // Now write it out
            buf_len = snprintf(tmp_body_buf, TMP_BODY_BUF_SIZE, "%s: %d\r\n",
                               HTTPISH_HEADER_CONTENT_LENGTH, total_length);

            if (!pmhutils_wstream_write_bytes(ws, tmp_body_buf, buf_len)) {
                return (FALSE);
            }

            buf_len = snprintf(tmp_body_buf, TMP_BODY_BUF_SIZE, "\r\n--%s\r\n", uniqueBoundary);

            if (!pmhutils_wstream_write_bytes(ws, tmp_body_buf, buf_len)) {
                return (FALSE);
            }

        } else {
            // Write out Content-Length for the first body
            total_length = msg->mesg_body[0].msgLength;
            buf_len = snprintf(tmp_body_buf, TMP_BODY_BUF_SIZE, "%s: %d\r\n",
                               HTTPISH_HEADER_CONTENT_LENGTH, total_length);

            if (!pmhutils_wstream_write_bytes(ws, tmp_body_buf, buf_len)) {
                return (FALSE);
            }
        }
        for (i = 0; i < msg->num_body_parts; i++) {
            if (i > 0) {
                // If there is another body to come, write the unique boundary
                buf_len = snprintf(tmp_body_buf, TMP_BODY_BUF_SIZE, "\r\n--%s\r\n", uniqueBoundary);
                if (!pmhutils_wstream_write_bytes(ws, tmp_body_buf, buf_len)) {
                    return (FALSE);
                }
            }
            snprintf(tmp_body_buf, TMP_BODY_BUF_SIZE, "Content-Type: %s\r\n",
                    msg->mesg_body[i].msgContentType);
            sstrncat(tmp_body_buf, "Content-Disposition: ",
                    sizeof(tmp_body_buf) - strlen(tmp_body_buf));

            switch (msg->mesg_body[i].msgContentDisp) {
            case SIP_CONTENT_DISPOSITION_RENDER_VALUE:
                sstrncat(tmp_body_buf, SIP_CONTENT_DISPOSITION_RENDER,
                        sizeof(tmp_body_buf) - strlen(tmp_body_buf));
                break;
            case SIP_CONTENT_DISPOSITION_SESSION_VALUE:
            default:
                sstrncat(tmp_body_buf, SIP_CONTENT_DISPOSITION_SESSION,
                        sizeof(tmp_body_buf) - strlen(tmp_body_buf));
                break;
            case SIP_CONTENT_DISPOSITION_ICON_VALUE:
                sstrncat(tmp_body_buf, SIP_CONTENT_DISPOSITION_ICON,
                        sizeof(tmp_body_buf) - strlen(tmp_body_buf));
                break;
            case SIP_CONTENT_DISPOSITION_ALERT_VALUE:
                sstrncat(tmp_body_buf, SIP_CONTENT_DISPOSITION_ALERT,
                        sizeof(tmp_body_buf) - strlen(tmp_body_buf));
                break;
            case SIP_CONTENT_DISPOSITION_PRECONDITION_VALUE:
                sstrncat(tmp_body_buf, SIP_CONTENT_DISPOSITION_PRECONDITION,
                        sizeof(tmp_body_buf) - strlen(tmp_body_buf));
                break;
            }
            if (msg->mesg_body[i].msgRequiredHandling) {
                sstrncat(tmp_body_buf, ";handling=required\r\n",
                        sizeof(tmp_body_buf) - strlen(tmp_body_buf));
            } else {
                sstrncat(tmp_body_buf, ";handling=optional\r\n",
                        sizeof(tmp_body_buf) - strlen(tmp_body_buf));
            }
            if (msg->mesg_body[i].msgContentId) {
                sstrncat(tmp_body_buf, "Content-Id: ",
                        sizeof(tmp_body_buf) - strlen(tmp_body_buf));
                sstrncat(tmp_body_buf, msg->mesg_body[i].msgContentId,
                        sizeof(tmp_body_buf) - strlen(tmp_body_buf));
                sstrncat(tmp_body_buf, "\r\n",
                        sizeof(tmp_body_buf) - strlen(tmp_body_buf));
            }
            sstrncat(tmp_body_buf, "\r\n",
                     sizeof(tmp_body_buf) - strlen(tmp_body_buf));
            buf_len = strlen(tmp_body_buf);
            if (!pmhutils_wstream_write_bytes(ws, tmp_body_buf, buf_len)) {
                return (FALSE);
            }

            // Now write the body
            if (!pmhutils_wstream_write_bytes(ws, msg->mesg_body[i].msgBody,
                                              msg->mesg_body[i].msgLength)) {
                return (FALSE);
            }
        }
        // After writing out the last body part, write out the last unique
        // boundary line
        if (msg->num_body_parts > 1) {
            snprintf(tmp_body_buf, TMP_BODY_BUF_SIZE, "\r\n--%s--\r\n", uniqueBoundary);
            buf_len = strlen(tmp_body_buf);
            if (!pmhutils_wstream_write_bytes(ws, tmp_body_buf, buf_len)) {
                return (FALSE);
            }
        }
    } else {
        if (!pmhutils_wstream_write_byte(ws, '\r')) {
            return (FALSE);
        }
        if (!pmhutils_wstream_write_byte(ws, '\n')) {
            return (FALSE);
        }
    }

    return (TRUE);
}

hStatus_t
httpish_msg_write (httpishMsg_t *msg,
                   char *buf,
                   uint32_t *nbytes)
{
    pmhWstream_t *ws = NULL;

    ws = pmhutils_wstream_create_with_buf(buf, *nbytes);
    if (!ws) {
        return (HSTATUS_FAILURE);
    }

    if (!httpish_msg_to_wstream(ws, msg)) {
        pmhutils_wstream_delete(ws, FALSE);
        cpr_free(ws);
        return (HSTATUS_FAILURE);
    }

    *nbytes = pmhutils_wstream_get_length(ws);
    pmhutils_wstream_delete(ws, FALSE);
    cpr_free(ws);
    return HSTATUS_SUCCESS;
}

int
httpish_cache_header_val (httpishMsg_t *hmsg,
                          char *this_line)
{
    static const char fname[] = "httpish_cache_header_val";
    char *hdr_start;
    httpish_cache_t *hdr_cache;
    int i;
    char headerName[HTTPISH_HEADER_NAME_SIZE];

    headerName[0] = '\0';

    hdr_cache = hmsg->hdr_cache;
    hdr_start = this_line;

    if (httpish_header_name_val(headerName, this_line)) {
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Invalid Header %s", DEB_F_PREFIX_ARGS(HTTPISH, fname), this_line);
        return (SIP_ERROR);
    }

    for (i = 0; i < HTTPISH_HEADER_CACHE_SIZE; ++i) {
        sip_header_t *tmp = sip_cached_headers + i;

        if (cpr_strcasecmp(headerName, tmp->hname) == 0 ||
            compact_hdr_cmp(headerName, tmp->c_hname) == 0) {
            this_line = strchr(this_line, ':');
            if (this_line) {
                this_line++; /* Skip the ':' */

                /* Remove leading spaces */
                while (*this_line == ' ' || *this_line == '\t') {
                    this_line++;
                }
                if (*this_line) {
                    if (hdr_cache[i].hdr_start) {
                        int org_len, offset;
                        int size;
                        char *newbuf;

                        /* Multiple instances of a header, concatenate the
                         * header values with a ','
                         */
                        org_len = strlen(hdr_cache[i].hdr_start);
                        offset = hdr_cache[i].val_start - hdr_cache[i].hdr_start;
                        size = org_len + 2 + strlen(this_line);
                        newbuf = (char *) cpr_realloc(hdr_cache[i].hdr_start,
                                                      size);
                        if (newbuf == NULL) {
                            cpr_free(hdr_cache[i].hdr_start);
                            hdr_cache[i].hdr_start = NULL;
                            break;
                        }
                        hdr_cache[i].hdr_start = newbuf;
                        hdr_cache[i].val_start = hdr_cache[i].hdr_start + offset;
                        hdr_cache[i].hdr_start[org_len] = ',';
                        sstrncpy(hdr_cache[i].hdr_start + org_len + 1, this_line,
                                size - org_len - 1);
                        cpr_free(hdr_start);
                    } else {
                        hdr_cache[i].hdr_start = hdr_start;
                        hdr_cache[i].val_start = this_line;
                    }
                } else { // this line is blank
                    cpr_free(hdr_start);
                }
            } else { // this line does not have a ':'
                cpr_free(hdr_start);
            }
            return 0;
        }
    }
    return -1;
}

/*
 * Return -1 for invalid/empty content-length value
 * else return the content length
 */
int32_t
get_content_length (httpishMsg_t *hmsg)
{
    int i;
    const char *hdr_val;
    long strtol_result;
    char *strtol_end;

    hdr_val = httpish_msg_get_cached_header_val(hmsg, CONTENT_LENGTH);
    if (hdr_val == NULL) {
        return -1;
    }

    for (i = 0; hdr_val[i]; ++i) {
        if (!isdigit((int) hdr_val[i])) {
            return -1;
        }
    }

    /* If the string was empty then the content length is still invalid */
    if (!i) {
        return -1;
    }

    errno = 0;
    strtol_result = strtol(hdr_val, &strtol_end, 10);

    if (errno || hdr_val == strtol_end || strtol_result > INT_MAX) {
        return -1;
    } else {
        return (int) strtol_result;
    }
}

uint8_t
get_content_type_value (const char *content_type)
{
    if (!content_type) {
        return SIP_CONTENT_TYPE_UNKNOWN_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_SDP,
                                sizeof(SIP_CONTENT_TYPE_SDP) - 1)) {
        return SIP_CONTENT_TYPE_SDP_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_SIPFRAG,
                                sizeof(SIP_CONTENT_TYPE_SIPFRAG) - 1)) {
        return SIP_CONTENT_TYPE_SIPFRAG_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_TEXT_PLAIN,
                                sizeof(SIP_CONTENT_TYPE_TEXT_PLAIN) - 1)) {
        return SIP_CONTENT_TYPE_TEXT_PLAIN_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_SIP,
                                sizeof(SIP_CONTENT_TYPE_SIP) - 1)) {
        return SIP_CONTENT_TYPE_SIP_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_MWI,
                                sizeof(SIP_CONTENT_TYPE_MWI) - 1)) {
        return SIP_CONTENT_TYPE_MWI_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_MULTIPART_MIXED,
                                sizeof(SIP_CONTENT_TYPE_MULTIPART_MIXED) - 1)) {
        return SIP_CONTENT_TYPE_MULTIPART_MIXED_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_MULTIPART_ALTERNATIVE,
                                sizeof(SIP_CONTENT_TYPE_MULTIPART_ALTERNATIVE) - 1)) {
        return SIP_CONTENT_TYPE_MULTIPART_ALTERNATIVE_VALUE;
    }
    else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_DIALOG,
                              sizeof(SIP_CONTENT_TYPE_DIALOG) - 1)) {
        return SIP_CONTENT_TYPE_DIALOG_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_KPML_REQUEST,
                                sizeof(SIP_CONTENT_TYPE_KPML_REQUEST) - 1)) {
        return SIP_CONTENT_TYPE_KPML_REQUEST_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_KPML_RESPONSE,
                                sizeof(SIP_CONTENT_TYPE_KPML_RESPONSE) - 1)) {
        return SIP_CONTENT_TYPE_KPML_RESPONSE_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_REMOTECC_REQUEST,
                                sizeof(SIP_CONTENT_TYPE_REMOTECC_REQUEST) - 1)) {
        return SIP_CONTENT_TYPE_REMOTECC_REQUEST_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_CONFIGAPP,
                                sizeof(SIP_CONTENT_TYPE_CONFIGAPP) - 1)) {
        return SIP_CONTENT_TYPE_CONFIGAPP_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_REMOTECC_RESPONSE,
                                sizeof(SIP_CONTENT_TYPE_REMOTECC_RESPONSE) - 1)) {
        return SIP_CONTENT_TYPE_REMOTECC_RESPONSE_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_PRESENCE,
                                sizeof(SIP_CONTENT_TYPE_PRESENCE) - 1)) {
        return SIP_CONTENT_TYPE_PRESENCE_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_CMXML,
                                sizeof(SIP_CONTENT_TYPE_CMXML) - 1)) {
        return SIP_CONTENT_TYPE_CMXML_VALUE;
    } else if (!httpish_strncasecmp(content_type, SIP_CONTENT_TYPE_CTI,
                                sizeof(SIP_CONTENT_TYPE_CTI) - 1)) {
        return SIP_CONTENT_TYPE_CTI_VALUE;
    }
    return SIP_CONTENT_TYPE_UNKNOWN_VALUE;
}

/******************************************************************
 * msg_process_one_body
 * This function will process one of the body parts of the whole body
 * Headers not understood will be ignored
 * - msg_start should be pointing at the first header of the body part
 * - msg_end at its last character
 ******************************************************************/
int
msg_process_one_body (httpishMsg_t *hmsg,
                      char *msg_start,
                      char *msg_end,
                      int current_body_part)
{
    static const char fname[] = "msg_process_one_body";
    pmhRstream_t *rstream = NULL;
    char         *content_type = NULL, *content_disp = NULL;
    char         *line = NULL, *body = NULL;
    char         *content_enc = NULL;
    char         *content_id = NULL;  int nbytes = 0;
    boolean       body_read = FALSE;

    // Adjust msg_start to point to the first valid character
    while (*msg_start == '\r' || *msg_start == '\n') {
        msg_start++;
    }

    // Convert this chunk of data into a more parse-able format
    rstream = pmhutils_rstream_create(msg_start, (uint32_t)(msg_end - msg_start));

    while (!body_read) {
        if (rstream) {
            line = pmhutils_rstream_read_line(rstream);
        }

        if (line) {
            // Look for the kind of line it is
            if (!cpr_strncasecmp(line, HTTPISH_HEADER_CONTENT_TYPE,
                                 sizeof(HTTPISH_HEADER_CONTENT_TYPE) - 1)) {
                // Its Content-Type, read in the value
                // XXX what if the line looks like "Content-Type-Garbage: some-value"?
                content_type = line + sizeof(HTTPISH_HEADER_CONTENT_TYPE);
                // XXX what if the line looks like "Content-Type :  some-value"?
                while (*content_type == ' ') {
                    content_type++;
                }
                hmsg->mesg_body[current_body_part].msgContentTypeValue = get_content_type_value(content_type);
                nbytes = strlen(content_type) + 1;
                hmsg->mesg_body[current_body_part].msgContentType =
                             (char *) cpr_malloc((nbytes)*sizeof(char));
                if (hmsg->mesg_body[current_body_part].msgContentType == NULL) {
                   CCSIP_DEBUG_ERROR(SIP_F_PREFIX"malloc failed", fname);
                } else {
                    memcpy(hmsg->mesg_body[current_body_part].msgContentType,
                           content_type, nbytes);
                }
            } else if (!cpr_strncasecmp(line, HTTPISH_HEADER_CONTENT_ID,
                                        sizeof(HTTPISH_HEADER_CONTENT_ID) - 1)) {
                //Its Content-Id, read the value
                content_id = line + sizeof(HTTPISH_HEADER_CONTENT_ID);
                while(*content_id == ' ') {
                    content_id++;
                }
                nbytes = strlen(content_id) + 1;
                hmsg->mesg_body[current_body_part].msgContentId =
                             (char *) cpr_malloc((nbytes)*sizeof(char));
                if (hmsg->mesg_body[current_body_part].msgContentId == NULL) {
                   CCSIP_DEBUG_ERROR(SIP_F_PREFIX"malloc failed", fname);
                }
                memcpy(hmsg->mesg_body[current_body_part].msgContentId,
                          content_id, nbytes);
            } else if (!cpr_strncasecmp(line, SIP_HEADER_CONTENT_DISP,
                                        sizeof(SIP_HEADER_CONTENT_DISP) - 1)) {
                // Its Content-Disposition, read in the value
                content_disp = line + sizeof(SIP_HEADER_CONTENT_DISP);
                while (*content_disp == ' ') {
                    content_disp++;
                }
                if (!cpr_strncasecmp(content_disp, SIP_CONTENT_DISPOSITION_SESSION,
                                     sizeof(SIP_CONTENT_DISPOSITION_SESSION) - 1)) {
                    hmsg->mesg_body[current_body_part].msgContentDisp =
                        SIP_CONTENT_DISPOSITION_SESSION_VALUE;
                    content_disp += sizeof(SIP_CONTENT_DISPOSITION_SESSION) - 1;
                } else {
                    hmsg->mesg_body[current_body_part].msgContentDisp =
                        SIP_CONTENT_DISPOSITION_UNKNOWN_VALUE;
                    content_disp = strchr(line, ';');
                }
                if (content_disp && *content_disp == ';') {
                    content_disp++;
                    if (!cpr_strncasecmp(content_disp, "handling", 8)) {
                        content_disp += 9;
                        if (!cpr_strncasecmp(content_disp, "required", 8)) {
                            hmsg->mesg_body[current_body_part].msgRequiredHandling = TRUE;
                        } else if (!cpr_strncasecmp(content_disp, "optional", 8)) {
                            hmsg->mesg_body[current_body_part].msgRequiredHandling = FALSE;
                        }
                    }
                }
            } else if (!cpr_strncasecmp(line, HTTPISH_HEADER_CONTENT_ENCODING,
                                        sizeof(HTTPISH_HEADER_CONTENT_ENCODING) - 1)) {
                content_enc = line + sizeof(HTTPISH_HEADER_CONTENT_ENCODING);
                while (*content_enc == ' ') {
                    content_enc++;
                }
                if (!cpr_strcasecmp(content_enc, SIP_CONTENT_ENCODING_IDENTITY)) {
                    hmsg->mesg_body[current_body_part].msgContentEnc =
                        SIP_CONTENT_ENCODING_IDENTITY_VALUE;
                } else {
                    hmsg->mesg_body[current_body_part].msgContentEnc =
                        SIP_CONTENT_ENCODING_UNKNOWN_VALUE;
                }
            } else if (!(*line)) {
                body_read = TRUE;
                // The rest of the bytes are the body
                hmsg->mesg_body[current_body_part].msgLength =
                    rstream->nbytes - rstream->bytes_read;
                body = pmhutils_rstream_read_bytes(rstream, rstream->nbytes - rstream->bytes_read);
                if (body) {
                    hmsg->mesg_body[current_body_part].msgBody = body;
                }
            } else {
                // Unhandled header type
                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Unrecognized header in body", DEB_F_PREFIX_ARGS(HTTPISH, fname));
            }
            // Free the read line
            cpr_free(line);
            line = NULL;
        } else {
            body_read = TRUE;
        }
    }
    // Free the created stream structure
    pmhutils_rstream_delete(rstream, FALSE);
    cpr_free(rstream);
    return 0;
}

/******************************************************************
 * msg_process_multiple_bodies
 * This function will process multiple body parts of the whole body
 * boundary should point to the delimiter string, raw body points to
 * the beginning of the whole body structure in the message
 *
 * The algorithm here is to find the beginning and the end of each
 * body part and send it off for further header and body extraction
 ******************************************************************/
int
msg_process_multiple_bodies (httpishMsg_t *hmsg,
                             char *boundary,
                             char *raw_body)
{
    char    msg_delimit[MSG_DELIMIT_SIZE];
    int     i, body_part = 0;
    boolean end_of_proc = FALSE;
    char   *msg_start, *msg_end;

    // First copy the boundary in an array
    msg_delimit[0] = '-';
    msg_delimit[1] = '-';
    for (i = 0; boundary[i] != '\r' && boundary[i] != '\n' &&
         boundary[i] != ';' && boundary[i] != '\0'; i++) {
         if (i + 2 >= MSG_DELIMIT_SIZE) {
             return body_part;
         }
        msg_delimit[i + 2] = boundary[i];
    }
    msg_delimit[i + 2] = '\0';

    // Loop through the body segments
    while (!end_of_proc && body_part < HTTPISH_MAX_BODY_PARTS) {
        msg_start = strstr(raw_body, msg_delimit);
        if (msg_start) {
            msg_start += strlen(msg_delimit) + 1;
            msg_end = strstr(msg_start, msg_delimit);
            if (msg_end) {
                (void) msg_process_one_body(hmsg, msg_start, msg_end - 1,
                                            body_part);
            } else {
                // No boundary found for message end, return error
                end_of_proc = TRUE;
                continue;
            }
        } else {
            // No boundary found for message start, return error
            end_of_proc = TRUE;
            continue;
        }
        raw_body = msg_end;
        // Check if this is the last boundary
        if (*(raw_body + strlen(msg_delimit)) == '-') {
            end_of_proc = TRUE;
        }
        body_part++;
    }
    return body_part;
}

hStatus_t
httpish_msg_process_network_msg (httpishMsg_t *hmsg,
                                 char *nmsg,
                                 uint32_t *nbytes)
{
    static const char fname[] = "httpish_msg_process_network_msg";
    pmhRstream_t *rs = NULL;
    int32_t       bytes_remaining, delta;
    char         *mline;
    hStatus_t     retval;
    char         *raw_body = NULL;
    const char   *content_type = NULL;
    const char   *content_id = NULL;
    int32_t      contentid_len;
    int32_t      contenttype_len;

    if (!hmsg || !nmsg || (*nbytes <= 0)) {
        return HSTATUS_FAILURE;
    }

    if (hmsg->is_complete == TRUE) {
        *nbytes = 0;
        return HSTATUS_SUCCESS;
    }

    if ((rs = pmhutils_rstream_create(nmsg, *nbytes)) == NULL)
        return HSTATUS_FAILURE;

    /* Try to read message line */
    while (!hmsg->mesg_line) {
        mline = pmhutils_rstream_read_line(rs);
        if (!mline) {
            *nbytes = rs->bytes_read;
            if (rs->eof == TRUE) {
                retval = HSTATUS_SUCCESS;
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Msg line read failure due to RS->EOF", fname);
            } else {
                retval = HSTATUS_FAILURE;
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Msg line read failure", fname);
            }
            pmhutils_rstream_delete(rs, FALSE);
            cpr_free(rs);
            return retval;
        }
        if (!(*mline)) {
            cpr_free(mline);
        } else {
            hmsg->mesg_line = mline;
        }
    }

    /* There is a message line. We could have a header or a message body */
    while (!hmsg->headers_read) {
        char *this_header;

        this_header = pmhutils_rstream_read_line(rs);
        if (!this_header) {
            *nbytes = rs->bytes_read;
            if (rs->eof == TRUE) {
                retval = HSTATUS_SUCCESS;
                CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Header line read failure due to RS->EOF", DEB_F_PREFIX_ARGS(HTTPISH, fname));
            } else {
                retval = HSTATUS_FAILURE;
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Header line read failure", fname);
            }
            pmhutils_rstream_delete(rs, FALSE);
            cpr_free(rs);
            return retval;
        }
        if (!(*this_header)) {
            cpr_free(this_header);
            hmsg->headers_read = TRUE;
        } else {
            httpish_header *h;

            if (httpish_cache_header_val(hmsg, this_header) == -1) {
                /*
                 * For a non-cacheable header or error in caching routine,
                 * use the header linked list.
                 */
                h = (httpish_header *) cpr_malloc(sizeof(httpish_header));
                if (!h) {
                    *nbytes = rs->bytes_read;
                    pmhutils_rstream_delete(rs, FALSE);
                    cpr_free(rs);
                    cpr_free(this_header);
                    return HSTATUS_FAILURE;
                }

                h->next = NULL;
                h->header = this_header;
                enqueue(hmsg->headers, (void *)h);
            }

        }
    }

    /* Calculate the bytes remaining in the read stream */
    bytes_remaining = rs->nbytes - rs->bytes_read;

    /* Now get the content length header value */
    hmsg->content_length = get_content_length(hmsg);
    if (hmsg->content_length == -1) {
        /* Bad or missing content-length header
         * For UDP, assume remaining msg is message body.
         * For TCP, message is not complete without content-length header
         * but we will ignore this possibility for now (assume content-length will always be there)
         * since we don't know if we have a complete message or a fragmented one
         */
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Content-Length header not received", fname);
        hmsg->content_length = bytes_remaining;
    }

    delta = bytes_remaining - hmsg->content_length;
    if (delta) {
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX "Content Length %d, Bytes Remaining %d.", DEB_F_PREFIX_ARGS(HTTPISH, fname),
                            hmsg->content_length, bytes_remaining);
    }
    if (delta < 0) {
        /* We have fewer bytes than specified by Content-Length header */
        hmsg->content_length = bytes_remaining;
        hmsg->is_complete = FALSE;
        CCSIP_DEBUG_MESSAGE(DEB_F_PREFIX"Partial body received", DEB_F_PREFIX_ARGS(HTTPISH, fname));
    } else {
        hmsg->is_complete = TRUE;
    }

    if (hmsg->content_length > 0) {
        raw_body = pmhutils_rstream_read_bytes(rs, hmsg->content_length);
        if (!raw_body) {
            pmhutils_rstream_delete(rs, FALSE);
            cpr_free(rs);
            return HSTATUS_FAILURE;
        }
    }

    // If message is not complete we should not parse the message any more
    if (!hmsg->is_complete) {
        *nbytes = rs->bytes_read;
        pmhutils_rstream_delete(rs, FALSE);
        cpr_free(rs);
        UTILFREE(raw_body);
        return HSTATUS_SUCCESS;
    }

    // Figure out whether more parsing of the received body is necessary
    content_type = httpish_msg_get_cached_header_val(hmsg, CONTENT_TYPE);
    if (content_type && (hmsg->content_length > 0)) {
        if (!cpr_strncasecmp(content_type, "multipart/mixed", 15)) {

            char *boundary = NULL;
            int num_bodies = 0;

            // find the boundary tag
            boundary = strchr(content_type, '=');
            if (boundary) {
                boundary++;
                if (raw_body) {
                    num_bodies = msg_process_multiple_bodies(hmsg, boundary, raw_body);
                }

                if (num_bodies == 0) {
                    CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in decoding multipart messages", fname);
                } else {
                    hmsg->num_body_parts = (uint8_t) num_bodies;
                }
            } else {
                // No boundary specified!
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Error in decoding multipart messages: No body delimiter", fname);
            }
            hmsg->raw_body = raw_body;

        } else {
            // All of the body is of a single type
            // hmsg->mesg_body[0].msgBody = raw_body;
            hmsg->mesg_body[0].msgBody = (char *)
                cpr_malloc(hmsg->content_length + 1);
            if (hmsg->mesg_body[0].msgBody == NULL) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to get memory", fname);
                pmhutils_rstream_delete(rs, FALSE);
                cpr_free(rs);
                cpr_free(raw_body);
                return HSTATUS_FAILURE;
            }

            if (raw_body) {
                memcpy(hmsg->mesg_body[0].msgBody, raw_body,
                       hmsg->content_length + 1);
            }

            content_id = httpish_msg_get_header_val(hmsg, HTTPISH_HEADER_CONTENT_ID, NULL);
            if (content_id) {
                contentid_len = strlen(content_id) + 1;
                hmsg->mesg_body[0].msgContentId = (char *)
                       cpr_malloc(contentid_len * sizeof(char));
                if (hmsg->mesg_body[0].msgContentId == NULL) {
                   CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to get memory", fname);
                    pmhutils_rstream_delete(rs, FALSE);
                    cpr_free(rs);
                    cpr_free(raw_body);
                    return HSTATUS_FAILURE;
                }
	        memcpy(hmsg->mesg_body[0].msgContentId,
			content_id, contentid_len);
            }

            hmsg->mesg_body[0].msgContentTypeValue = get_content_type_value(content_type);
            contenttype_len = strlen(content_type) + 1;
            hmsg->mesg_body[0].msgContentType = (char *)
                   cpr_malloc(contenttype_len * sizeof(char));
            if (hmsg->mesg_body[0].msgContentType == NULL) {
                CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to get memory", fname);
                pmhutils_rstream_delete(rs, FALSE);
                cpr_free(rs);
                cpr_free(raw_body);
                return HSTATUS_FAILURE;
            }
            memcpy(hmsg->mesg_body[0].msgContentType,
                   content_type, contenttype_len);

            hmsg->mesg_body[0].msgContentDisp =
                SIP_CONTENT_DISPOSITION_SESSION_VALUE;
            hmsg->mesg_body[0].msgRequiredHandling = TRUE;
            hmsg->mesg_body[0].msgLength = hmsg->content_length;
            hmsg->num_body_parts = 1;
            hmsg->raw_body = raw_body;
        }
    } else if (hmsg->content_length > 0) {
        // No content-type specified but there is a body present. Treat this
        // as an error
        hmsg->is_complete = FALSE;
        hmsg->content_length = -1;
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Body found without content-type", fname);
        UTILFREE(raw_body);
        pmhutils_rstream_delete(rs, FALSE);
        cpr_free(rs);
        return HSTATUS_SUCCESS;
    }

    *nbytes = rs->bytes_read;
    pmhutils_rstream_delete(rs, FALSE);
    cpr_free(rs);
    return HSTATUS_SUCCESS;
}


hStatus_t
httpish_msg_add_body (httpishMsg_t *msg,
                      char *body,
                      uint32_t nbytes,
                      const char *content_type,
                      uint8_t msg_disposition,
                      boolean required,
                      char *content_id)
{
    static const char fname[] = "httpish_msg_add_body";
    uint8_t current_body_part;
    uint32_t contenttype_len;

    if (!msg || !body || (nbytes == 0)) {
        return HSTATUS_FAILURE;
    }

    if (msg->num_body_parts == HTTPISH_MAX_BODY_PARTS) {
        return HSTATUS_FAILURE;
    }

    // Add body at the next available index
    current_body_part = msg->num_body_parts;

    contenttype_len = strlen(content_type) + 1;
    msg->mesg_body[current_body_part].msgContentType = (char *)
            cpr_malloc(contenttype_len * sizeof(char));
    if (msg->mesg_body[current_body_part].msgContentType == NULL) {
        CCSIP_DEBUG_ERROR(SIP_F_PREFIX"Unable to get memory", fname);
        return HSTATUS_FAILURE;
    }

    msg->mesg_body[current_body_part].msgBody = body;
    memcpy(msg->mesg_body[current_body_part].msgContentType,
           content_type, contenttype_len);
    msg->mesg_body[current_body_part].msgContentTypeValue = get_content_type_value(content_type);
    msg->mesg_body[current_body_part].msgContentDisp = msg_disposition;
    msg->mesg_body[current_body_part].msgRequiredHandling = required;
    msg->mesg_body[current_body_part].msgLength = nbytes;
    msg->mesg_body[current_body_part].msgContentId = content_id;

    msg->num_body_parts++;

    return HSTATUS_SUCCESS;
}


httpishStatusCodeClass_t
httpish_msg_get_code_class (uint16_t statusCode)
{
    httpishStatusCodeClass_t retval;
    int fc = statusCode / 100;

    switch (fc) {
    case 1:
        retval = codeClass1xx;
        break;
    case 2:
        retval = codeClass2xx;
        break;
    case 3:
        retval = codeClass3xx;
        break;
    case 4:
        retval = codeClass4xx;
        break;
    case 5:
        retval = codeClass5xx;
        break;
    case 6:
        retval = codeClass6xx;
        break;
    default:
        retval = codeClassInvalid;
        break;
    }

    return retval;
}


uint16_t
httpish_msg_get_num_particular_headers (httpishMsg_t *msg,
                                        const char *hname,
                                        const char *c_hname,
                                        char *header_val[],
                                        uint16_t max_headers)
{
    nexthelper *p;
    char       *this_line = NULL;
    uint16_t    found = 0;

    if (!msg || !hname) {
        return 0;
    }

    p = (nexthelper *) msg->headers->qhead;

    while (p && found < max_headers) {
        this_line = ((httpish_header *)p)->header;
        if (this_line) {
            /* Remove leading spaces */
            while ((*this_line == ' ') && (*this_line != '\0'))
                this_line++;
            if ((strlen(this_line) > strlen(hname) + 1) &&
                (cpr_strncasecmp(this_line, hname, strlen(hname)) == 0 ||
                 compact_hdr_cmp(this_line, c_hname) == 0)) {
                this_line = strchr(this_line, ':');
                if (this_line) {
                    this_line++;
                    /* Remove leading spaces */
                    while ((*this_line == ' ') && (*this_line != '\0'))
                        this_line++;
                    if (*this_line == '\0') {
                        p = p->next;
                        continue;
                    } else {
                        header_val[found] = this_line;
                        found++;
                    }
                }
            }
        }
        p = p->next;
    }
    return found;
}


