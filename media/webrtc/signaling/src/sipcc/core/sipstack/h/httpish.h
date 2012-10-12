/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _HTTPISH_H_
#define _HTTPISH_H_

#include "cpr_types.h"
#include "pmhdefs.h"
#include "httpish_protocol.h"
#include "pmhutils.h"
#include "util_ios_queue.h"

#define HTTPISH_MIN_STATUS_CODE 100
#define HTTPISH_HEADER_CACHE_SIZE  12
#define HTTPISH_HEADER_NAME_SIZE   256

typedef struct h_header
{
    struct h_header *next;
    char *header;
} httpish_header;

typedef struct {
    char *hdr_start;
    char *val_start;
} httpish_cache_t;

#define HTTPISH_MAX_BODY_PARTS 6
typedef struct {
    uint8_t  msgContentDisp;
    boolean  msgRequiredHandling;
    uint8_t  msgContentTypeValue;
    char    *msgContentType;
    char    *msgBody;
    uint32_t msgLength;
    char    *msgContentId;
    uint8_t  msgContentEnc;
} msgBody_t;

typedef struct _httpMsg
{
    struct _httpMsg *next;
    boolean         retain_flag;    /* If TRUE, Do not free the msg */
    char           *mesg_line;
    queuetype      *headers;
    msgBody_t       mesg_body[HTTPISH_MAX_BODY_PARTS];
    char           *raw_body;
    int32_t         content_length;
    uint8_t         num_body_parts;
    boolean         is_complete;
    boolean         headers_read;
    /* Cache the most commonly used headers */
    httpish_cache_t hdr_cache[HTTPISH_HEADER_CACHE_SIZE];
    /* this is the complete message received/sent at the socket */
    char           *complete_message;
} httpishMsg_t;

typedef struct
{
    char *method;
    char *url;
    char *version;
} httpishReqLine_t;


typedef struct
{
    char    *reason_phrase;
    uint16_t status_code;
    char    *version;
} httpishRespLine_t;

typedef enum
{
    STATUS_SUCCESS = 0,
    STATUS_FAILURE,
    STATUS_UNKNOWN,
    HSTATUS_SUCCESS = STATUS_SUCCESS,
    HSTATUS_FAILURE = STATUS_FAILURE,
    HSTATUS_UNKNOWN = STATUS_UNKNOWN
} hStatus_t;

typedef enum
{
    codeClassInvalid = 0,
    codeClass1xx = 1,
    codeClass2xx = 2,
    codeClass3xx = 3,
    codeClass4xx = 4,
    codeClass5xx = 5,
    codeClass6xx = 6
} httpishStatusCodeClass_t;


/* Creates a message and initializes its internal members */
PMH_EXTERN httpishMsg_t *httpish_msg_create(void);

/* Frees internal members of the message, such as headers */
PMH_EXTERN void httpish_msg_free(httpishMsg_t *);

/*
 * Returns TRUE if the message is a  HTTPish Request, FALSE if not.
 * Return value will be wrong if the function is called before the
 * message is complete(see message_is_complete)
 */
PMH_EXTERN boolean httpish_msg_is_request(httpishMsg_t *, const char *, int);

/*
 * A message may be complete or incomplete at a point in time, since
 * one message may be formed of multiple network packets. This function
 * returns TRUE if the message is in fact complete, FALSE if not
 */
PMH_EXTERN boolean httpish_msg_is_complete(httpishMsg_t *);

/*
 * Gets a request line structure out of the message i.e parses the message
 * line. Null on failure, incomplete message etc. User should call
 * httpish_msg_free_reqline when done to free the allocated memory
 * inside the structure.
 */
PMH_EXTERN httpishReqLine_t *httpish_msg_get_reqline(httpishMsg_t *);

/* Frees internal members of the request line structure */
PMH_EXTERN void httpish_msg_free_reqline(httpishReqLine_t *);

/*
 * Gets a response line structure out of the message i.e parses the message
 * line. Null on failure, incomplete message etc. User should call
 * httpish_msg_free_respline when done to free the allocated memory
 * inside the structure.
 */
PMH_EXTERN httpishRespLine_t *httpish_msg_get_respline(httpishMsg_t *);

/* Frees internal members of the response line structure */
PMH_EXTERN void httpish_msg_free_respline(httpishRespLine_t *);

/*
 * Adds a message line to the message given a request line elements.
 * This makes it a Request message
 * This routine allocates memory internally, which is freed on
 * calling httpish_msg_free()
 */
PMH_EXTERN hStatus_t httpish_msg_add_reqline(httpishMsg_t *,
                                             const char *method,
                                             const char *url,
                                             const char *version);

/*
 * Adds a message line to the message given response line elements.
 * This makes it a Response message
 * This routine allocates memory internally, which is freed on
 * calling httpish_msg_free(). Status codes are assumed to be a
 * maximum of 6 characters long ie <= 999999
 */
PMH_EXTERN hStatus_t httpish_msg_add_respline(httpishMsg_t *,
                                              const char *version,
                                              uint16_t status_code,
                                              const char *reason_phrase);

/*
 *  Adds a header with a text value to message.
 *    hname = name of the header for eg. "Content-Type"
 *    hval = value of the header for eg. "application/sdp"
 *    This routine allocates memory internally, which is freed on
 *  calling httpish_msg_free()
 */
PMH_EXTERN hStatus_t httpish_msg_add_text_header(httpishMsg_t *msg,
                                                 const char *hname,
                                                 const char *hval);

/*
 *  Adds a header with a integer value to message.
 *    hname = name of the header for eg. "Content-Length"
 *    hval = value of the header for eg. 234
 *    This routine allocates memory internally, which is freed on
 *    calling httpish_msg_free()
 */
/* Assumes the int is less than 10 characters*/
PMH_EXTERN hStatus_t httpish_msg_add_int_header(httpishMsg_t *msg,
                                                const char *hname,
                                                int32_t hvalue);

/*
 * Removes a header from the message.
 * hname = name of the header. eg. hname = "From"
 * Memory for that header string is freed.
 */
PMH_EXTERN hStatus_t httpish_msg_remove_header(httpishMsg_t *msg,
                                               const char *hname);


/*
 *  Returns the value of the header if found, NULL if not.
 *  hname = name of the header, eg. "Content-Length"
 *  The pointer should not be freed by the user. (It will be
 *  freed on freeing the message)
 */
PMH_EXTERN const char *httpish_msg_get_header_val(httpishMsg_t *,
                                                  const char *hname,
                                                  const char *c_hname);

PMH_EXTERN hStatus_t httpish_msg_get_header_vals(httpishMsg_t *,
                                                 const char *hname,
                                                 const char *c_hname,
                                                 uint16_t *nheaders,
                                                 char **header_vals);

PMH_EXTERN const char *httpish_msg_get_cached_header_val(httpishMsg_t *, int);


/*
 *  Gets an array of pointers to all the headers. The number of
 *    headers is returned in num_headers.
 *    If invoked such :
 *    all_the_headers = httpish_msg_get_all_headers(...),
 *    memory is allocated only for "all_the_headers" array, not for
 *    its individual elements. Hence only the pointer to the all_the_headers
 *    array should be freed.
 */
PMH_EXTERN char **httpish_msg_get_all_headers(httpishMsg_t *msg, uint32_t *num_headers);

PMH_EXTERN uint32_t httpish_msg_get_num_headers(httpishMsg_t *msg);


PMH_EXTERN uint16_t httpish_msg_get_num_particular_headers(httpishMsg_t *msg,
                                                           const char *hname,
                                                           const char *c_hname,
                                                           char *header_val[],
                                                           uint16_t max_headers);

/*
 * Utility function to get value of the Content-Length header. Returns -1
 * if the header is not present.
 */
PMH_EXTERN int32_t httpish_msg_get_content_length(httpishMsg_t *);


/*
 * Adds the content body to the message. Results in the addition of the
 * Content-Length header as well.
 */
PMH_EXTERN hStatus_t httpish_msg_add_body(httpishMsg_t *msg,
                                          char *body,
                                          uint32_t nbytes,
                                          const char *content_type,
                                          uint8_t msg_disposition,
                                          boolean required,
                                          char *content_id);

PMH_EXTERN boolean httpish_msg_header_present(httpishMsg_t *,
                                              const char *hname);



/*
 *  Allocates a buffer and writes the message into it in the network
 *        format i.e ready to send. On return, nbytes is filled in with
 *        the number of bytes contained in the message buffer. Returns
 *        NULL on failure. User needs to free memory when done.
 */
PMH_EXTERN char *httpish_msg_write_to_buf(httpishMsg_t * msg, uint32_t *nbytes);

/* Writes it out as a null terminated string. Expected use is debug */
PMH_EXTERN char *httpish_msg_write_to_string(httpishMsg_t * msg);

/*
 * Writes out a message in the network format(ie ready to send),
 * in a user provided buffer. nbytes is filled in with the number
 * of bytes in buf on entry and filled in with the
 * actual number of bytes written if the return value is SUCCESS.
 * There is no attempt to grow or realloc the buffer ie FAILURE
 * is returned if the buffer is not large enough.
 */
PMH_EXTERN hStatus_t httpish_msg_write(httpishMsg_t *msg,
                                       char *buf,
                                       uint32_t *nbytes);

/*
 *  This function is used to read bytes from a network packet into the
 *        message structure.
 *  msg = previously created httpishMsg using httpish_msg_create()
 *  nmsg = network message
 *  bytes_read = Number of bytes read from nmsg is filled in when the
 *               function returns.
 *  It is expected that after this returns HSTATUS_SUCCESS,
 *  httpish_is_message_complete() is called to see whether a complete
 *  message was received(or a previously started message was completed,
 *  at which point it can be processed further.
 */
PMH_EXTERN hStatus_t httpish_msg_process_network_msg(httpishMsg_t *msg,
                                                     char *nmsg,
                                                     uint32_t *bytes_read);

/*
 * Utility to get the class of status codes in http like responses.
 * For eg., the class for a code 480 is 4, as currently defined.
 */
PMH_EXTERN httpishStatusCodeClass_t httpish_msg_get_code_class(uint16_t statusCode);


#endif /* _HTTPISH_H_ */
