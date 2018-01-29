/*
 * This file contains prototypes for experimental SSL functions.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __sslexp_h_
#define __sslexp_h_

#include "ssl.h"
#include "sslerr.h"

SEC_BEGIN_PROTOS

/* The functions in this header file are not guaranteed to remain available in
 * future NSS versions. Code that uses these functions needs to safeguard
 * against the function not being available. */

#define SSL_EXPERIMENTAL_API(name, arglist, args)                   \
    (SSL_GetExperimentalAPI(name)                                   \
         ? ((SECStatus(*) arglist)SSL_GetExperimentalAPI(name))args \
         : SECFailure)
#define SSL_DEPRECATED_EXPERIMENTAL_API \
    (PR_SetError(SSL_ERROR_UNSUPPORTED_EXPERIMENTAL_API, 0), SECFailure)

/*
 * SSL_GetExtensionSupport() returns whether NSS supports a particular TLS
 * extension.
 *
 * - ssl_ext_none indicates that NSS does not support the extension and
 *   extension hooks can be installed.
 *
 * - ssl_ext_native indicates that NSS supports the extension natively, but
 *   allows an application to override that support and install its own
 *   extension hooks.
 *
 * - ssl_ext_native_only indicates that NSS supports the extension natively
 *   and does not permit custom extension hooks to be installed.  These
 *   extensions are critical to the functioning of NSS.
 */
typedef enum {
    ssl_ext_none,
    ssl_ext_native,
    ssl_ext_native_only
} SSLExtensionSupport;

#define SSL_GetExtensionSupport(extension, support)        \
    SSL_EXPERIMENTAL_API("SSL_GetExtensionSupport",        \
                         (PRUint16 _extension,             \
                          SSLExtensionSupport * _support), \
                         (extension, support))

/*
 * Custom extension hooks.
 *
 * The SSL_InstallExtensionHooks() registers two callback functions for use
 * with the identified extension type.
 *
 * Installing extension hooks disables the checks in TLS 1.3 that ensure that
 * extensions are only added to the correct messages.  The application is
 * responsible for ensuring that extensions are only sent with the right message
 * or messages.
 *
 * Installing an extension handler does not disable checks for whether an
 * extension can be used in a message that is a response to an extension in
 * another message.  Extensions in ServerHello, EncryptedExtensions and the
 * server Certificate messages are rejected unless the client sends an extension
 * in the ClientHello.  Similarly, a client Certificate message cannot contain
 * extensions that don't appear in a CertificateRequest (in TLS 1.3).
 *
 * Setting both |writer| and |handler| to NULL removes any existing hooks for
 * that extension.
 *
 * == SSLExtensionWriter
 *
 * An SSLExtensionWriter function is responsible for constructing the contents
 * of an extension.  This function is called during the construction of all
 * handshake messages where an extension might be included.
 *
 * - The |fd| argument is the socket file descriptor.
 *
 * - The |message| argument is the TLS handshake message type.  The writer will
 *   be called for every handshake message that NSS sends.  Most extensions
 *   should only be sent in a subset of messages.  NSS doesn’t check that
 *   extension writers don’t violate protocol rules regarding which message an
 *   extension can be sent in.
 *
 * - The |data| argument is a pointer to a buffer that should be written to with
 *   any data for the extension.
 *
 * - The |len| argument is an outparam indicating how many bytes were written to
 *   |data|.  The value referenced by |len| is initialized to zero, so an
 *   extension that is empty does not need to write to this value.
 *
 * - The |maxLen| indicates the maximum number of bytes that can be written to
 *   |data|.
 *
 * - The |arg| argument is the value of the writerArg that was passed during
 *   installation.
 *
 * An SSLExtensionWriter function returns PR_TRUE if an extension should be
 * written, and PR_FALSE otherwise.
 *
 * If there is an error, return PR_FALSE; if the error is truly fatal, the
 * application can mark the connection as failed. However, recursively calling
 * functions that alter the file descriptor in the callback - such as PR_Close()
 * - should be avoided.
 *
 * Note: The ClientHello message can be sent twice in TLS 1.3.  An
 * SSLExtensionWriter will be called twice with the same arguments in that case;
 * NSS does not distinguish between a first and second ClientHello.  It is up to
 * the application to track this if it needs to act differently each time.  In
 * most cases the correct behaviour is to provide an identical extension on each
 * invocation.
 *
 * == SSLExtensionHandler
 *
 * An SSLExtensionHandler function consumes a handshake message.  This function
 * is called when an extension is present.
 *
 * - The |fd| argument is the socket file descriptor.
 *
 * - The |message| argument is the TLS handshake message type. This can be used
 *   to validate that the extension was included in the correct handshake
 *   message.
 *
 * - The |data| argument points to the contents of the extension.
 *
 * - The |len| argument contains the length of the extension.
 *
 * - The |alert| argument is an outparam that allows an application to choose
 *   which alert is sent in the case of a fatal error.
 *
 * - The |arg| argument is the value of the handlerArg that was passed during
 *   installation.
 *
 * An SSLExtensionHandler function returns SECSuccess when the extension is
 * process successfully.  It can return SECFailure to cause the handshake to
 * fail.  If the value of alert is written to, NSS will generate a fatal alert
 * using the provided alert code.  The value of |alert| is otherwise not used.
 */
typedef PRBool(PR_CALLBACK *SSLExtensionWriter)(
    PRFileDesc *fd, SSLHandshakeType message,
    PRUint8 *data, unsigned int *len, unsigned int maxLen, void *arg);

typedef SECStatus(PR_CALLBACK *SSLExtensionHandler)(
    PRFileDesc *fd, SSLHandshakeType message,
    const PRUint8 *data, unsigned int len,
    SSLAlertDescription *alert, void *arg);

#define SSL_InstallExtensionHooks(fd, extension, writer, writerArg,         \
                                  handler, handlerArg)                      \
    SSL_EXPERIMENTAL_API("SSL_InstallExtensionHooks",                       \
                         (PRFileDesc * _fd, PRUint16 _extension,            \
                          SSLExtensionWriter _writer, void *_writerArg,     \
                          SSLExtensionHandler _handler, void *_handlerArg), \
                         (fd, extension, writer, writerArg,                 \
                          handler, handlerArg))

/*
 * Setup the anti-replay buffer for supporting 0-RTT in TLS 1.3 on servers.
 *
 * To use 0-RTT on a server, you must call this function.  Failing to call this
 * function will result in all 0-RTT being rejected.  Connections will complete,
 * but early data will be rejected.
 *
 * NSS uses a Bloom filter to track the ClientHello messages that it receives
 * (specifically, it uses the PSK binder).  This function initializes a pair of
 * Bloom filters.  The two filters are alternated over time, with new
 * ClientHello messages recorded in the current filter and, if they are not
 * already present, being checked against the previous filter.  If the
 * ClientHello is found, then early data is rejected, but the handshake is
 * allowed to proceed.
 *
 * The false-positive probability of Bloom filters means that some valid
 * handshakes will be marked as potential replays.  Early data will be rejected
 * for a false positive.  To minimize this and to allow a trade-off of space
 * against accuracy, the size of the Bloom filter can be set by this function.
 *
 * The first tuning parameter to consider is |window|, which determines the
 * window over which ClientHello messages will be tracked.  This also causes
 * early data to be rejected if a ClientHello contains a ticket age parameter
 * that is outside of this window (see Section 4.2.10.4 of
 * draft-ietf-tls-tls13-20 for details).  Set |window| to account for any
 * potential sources of clock error.  |window| is the entire width of the
 * window, which is symmetrical.  Therefore to allow 5 seconds of clock error in
 * both directions, set the value to 10 seconds (i.e., 10 * PR_USEC_PER_SEC).
 *
 * After calling this function, early data will be rejected until |window|
 * elapses.  This prevents replay across crashes and restarts.  Only call this
 * function once to avoid inadvertently disabling 0-RTT (use PR_CallOnce() to
 * avoid this problem).
 *
 * The primary tuning parameter is |bits| which determines the amount of memory
 * allocated to each Bloom filter.  NSS will allocate two Bloom filters, each
 * |2^(bits - 3)| octets in size.  The value of |bits| is primarily driven by
 * the number of connections that are expected in any time window.  Note that
 * this needs to account for there being two filters both of which have
 * (presumably) independent false positive rates.  The following formulae can be
 * used to find a value of |bits| and |k| given a chosen false positive
 * probability |p| and the number of requests expected in a given window |n|:
 *
 *   bits = log2(n) + log2(-ln(1 - sqrt(1 - p))) + 1.0575327458897952
 *   k = -log2(p)
 *
 * ... where log2 and ln are base 2 and e logarithms respectively.  For a target
 * false positive rate of 1% and 1000 handshake attempts, this produces bits=14
 * and k=7.  This results in two Bloom filters that are 2kB each in size.  Note
 * that rounding |k| and |bits| up causes the false positive probability for
 * these values to be a much lower 0.123%.
 *
 * IMPORTANT: This anti-replay scheme has several weaknesses.  See the TLS 1.3
 * specification for the details of the generic problems with this technique.
 *
 * In addition to the generic anti-replay weaknesses, the state that the server
 * maintains is in local memory only.  Servers that operate in a cluster, even
 * those that use shared memory for tickets, will not share anti-replay state.
 * Early data can be replayed at least once with every server instance that will
 * accept tickets that are encrypted with the same key.
 */
#define SSL_SetupAntiReplay(window, k, bits)                                    \
    SSL_EXPERIMENTAL_API("SSL_SetupAntiReplay",                                 \
                         (PRTime _window, unsigned int _k, unsigned int _bits), \
                         (window, k, bits))

/*
 * This function allows a server application to generate a session ticket that
 * will embed the provided token.
 *
 * This function will cause a NewSessionTicket message to be sent by a server.
 * This happens even if SSL_ENABLE_SESSION_TICKETS is disabled.  This allows a
 * server to suppress the usually automatic generation of a session ticket at
 * the completion of the handshake - which do not include any token - and to
 * control when session tickets are transmitted.
 *
 * This function will fail unless the socket has an active TLS 1.3 session.
 * Earlier versions of TLS do not support the spontaneous sending of the
 * NewSessionTicket message.
 */
#define SSL_SendSessionTicket(fd, appToken, appTokenLen)              \
    SSL_EXPERIMENTAL_API("SSL_SendSessionTicket",                     \
                         (PRFileDesc * _fd, const PRUint8 *_appToken, \
                          unsigned int _appTokenLen),                 \
                         (fd, appToken, appTokenLen))

/*
 * A stateless retry handler gives an application some control over NSS handling
 * of ClientHello messages.
 *
 * SSL_HelloRetryRequestCallback() installs a callback that allows an
 * application to control how NSS sends HelloRetryRequest messages.  This
 * handler is only used on servers and will only be called if the server selects
 * TLS 1.3.  Support for older TLS versions could be added in other releases.
 *
 * The SSLHelloRetryRequestCallback is invoked during the processing of a
 * TLS 1.3 ClientHello message.  It takes the following arguments:
 *
 * - |firstHello| indicates if the NSS believes that this is an initial
 *   ClientHello.  An initial ClientHello will never include a cookie extension,
 *   though it may contain a session ticket.
 *
 * - |clientToken| includes a token previously provided by the application.  If
 *   |clientTokenLen| is 0, then |clientToken| may be NULL.
 *
 *   - If |firstHello| is PR_FALSE, the value that was provided in the
 *     |retryToken| outparam of previous invocations of this callback will be
 *     present here.
 *
 *   - If |firstHello| is PR_TRUE, and the handshake is resuming a session, then
 *     this will contain any value that was passed in the |token| parameter of
 *     SSL_SendNewSessionTicket() method (see below).  If this is not resuming a
 *     session, then the token will be empty (and this value could be NULL).
 *
 * - |clientTokenLen| is the length of |clientToken|.
 *
 * - |retryToken| is an item that callback can write to.  This provides NSS with
 *   a token.  This token is encrypted and integrity protected and embedded in
 *   the cookie extension of a HelloRetryRequest.  The value of this field is
 *   only used if the handler returns ssl_stateless_retry_check.  NSS allocates
 *   space for this value.
 *
 * - |retryTokenLen| is an outparam for the length of the token. If this value
 *   is not set, or set to 0, an empty token will be sent.
 *
 * - |retryTokenMax| is the size of the space allocated for retryToken. An
 *   application cannot write more than this many bytes to retryToken.
 *
 * - |arg| is the same value that was passed to
 *   SSL_InstallStatelessRetryHandler().
 *
 * The handler can validate any the value of |clientToken|, query the socket
 * status (using SSL_GetPreliminaryChannelInfo() for example) and decide how to
 * proceed:
 *
 * - Returning ssl_hello_retry_fail causes the handshake to fail.  This might be
 *   used if the token is invalid or the application wishes to abort the
 *   handshake.
 *
 * - Returning ssl_hello_retry_accept causes the handshake to proceed.
 *
 * - Returning ssl_hello_retry_request causes NSS to send a HelloRetryRequest
 *   message and request a second ClientHello.  NSS generates a cookie extension
 *   and embeds the value of |retryToken|.  The value of |retryToken| value may
 *   be left empty if the application does not require any additional context to
 *   validate a second ClientHello attempt.  This return code cannot be used to
 *   reject a second ClientHello (i.e., when firstHello is PR_FALSE); NSS will
 *   abort the handshake if this value is returned from a second call.
 *
 * An application that chooses to perform a stateless retry can discard the
 * server socket.  All necessary state to continue the TLS handshake will be
 * included in the cookie extension.  This makes it possible to use a new socket
 * to handle the remainder of the handshake.  The existing socket can be safely
 * discarded.
 *
 * If the same socket is retained, the information in the cookie will be checked
 * for consistency against the existing state of the socket.  Any discrepancy
 * will result in the connection being closed.
 *
 * Tokens should be kept as small as possible.  NSS sets a limit on the size of
 * tokens, which it passes in |retryTokenMax|.  Depending on circumstances,
 * observing a smaller limit might be desirable or even necessary.  For
 * instance, having HelloRetryRequest and ClientHello fit in a single packet has
 * significant performance benefits.
 */
typedef enum {
    ssl_hello_retry_fail,
    ssl_hello_retry_accept,
    ssl_hello_retry_request
} SSLHelloRetryRequestAction;

typedef SSLHelloRetryRequestAction(PR_CALLBACK *SSLHelloRetryRequestCallback)(
    PRBool firstHello, const PRUint8 *clientToken, unsigned int clientTokenLen,
    PRUint8 *retryToken, unsigned int *retryTokenLen, unsigned int retryTokMax,
    void *arg);

#define SSL_HelloRetryRequestCallback(fd, cb, arg)                       \
    SSL_EXPERIMENTAL_API("SSL_HelloRetryRequestCallback",                \
                         (PRFileDesc * _fd,                              \
                          SSLHelloRetryRequestCallback _cb, void *_arg), \
                         (fd, cb, arg))

/* Update traffic keys (TLS 1.3 only).
 *
 * The |requestUpdate| flag determines whether to request an update from the
 * remote peer.
 */
#define SSL_KeyUpdate(fd, requestUpdate)                            \
    SSL_EXPERIMENTAL_API("SSL_KeyUpdate",                           \
                         (PRFileDesc * _fd, PRBool _requestUpdate), \
                         (fd, requestUpdate))

/*
 * Session cache API.
 */

/*
 * Information that can be retrieved about a resumption token.
 * See SSL_GetResumptionTokenInfo for details about how to use this API.
 * Note that peerCert points to a certificate in the NSS database and must be
 * copied by the application if it should be used after NSS shutdown or after
 * calling SSL_DestroyResumptionTokenInfo.
 */
typedef struct SSLResumptionTokenInfoStr {
    PRUint16 length;
    CERTCertificate *peerCert;
    PRUint8 *alpnSelection;
    PRUint32 alpnSelectionLen;
    PRUint32 maxEarlyDataSize;
} SSLResumptionTokenInfo;

/*
 * Allows applications to retrieve information about a resumption token.
 * This does not require a TLS session.
 *
 * - The |tokenData| argument is a pointer to the resumption token as byte array
 *   of length |tokenLen|.
 * - The |token| argument is a pointer to a SSLResumptionTokenInfo struct of
 *   of |len|. The struct gets filled by this function.
 * See SSL_DestroyResumptionTokenInfo for information about how to manage the
 * |token| memory.
 */
#define SSL_GetResumptionTokenInfo(tokenData, tokenLen, token, len)          \
    SSL_EXPERIMENTAL_API("SSL_GetResumptionTokenInfo",                       \
                         (const PRUint8 *_tokenData, unsigned int _tokenLen, \
                          SSLResumptionTokenInfo *_token, PRUintn _len),     \
                         (tokenData, tokenLen, token, len))

/*
 * SSL_GetResumptionTokenInfo allocates memory in order to populate |tokenInfo|.
 * Any SSLResumptionTokenInfo struct filled with SSL_GetResumptionTokenInfo
 * has to be freed with SSL_DestroyResumptionTokenInfo.
 */
#define SSL_DestroyResumptionTokenInfo(tokenInfo) \
    SSL_EXPERIMENTAL_API(                         \
        "SSL_DestroyResumptionTokenInfo",         \
        (SSLResumptionTokenInfo * _tokenInfo),    \
        (tokenInfo))

/*
 * This is the function signature for function pointers used as resumption
 * token callback. The caller has to copy the memory at |resumptionToken| with
 * length |len| before returning.
 *
 * - The |fd| argument is the socket file descriptor.
 * - The |resumptionToken| is a pointer to the resumption token as byte array
 *   of length |len|.
 * - The |ctx| is a void pointer to the context set by the application in
 *   SSL_SetResumptionTokenCallback.
 */
typedef SECStatus(PR_CALLBACK *SSLResumptionTokenCallback)(
    PRFileDesc *fd, const PRUint8 *resumptionToken, unsigned int len,
    void *ctx);

/*
 * This allows setting a callback for external session caches to store
 * resumption tokens.
 *
 * - The |fd| argument is the socket file descriptor.
 * - The |cb| is a function pointer to an implementation of
 *   SSLResumptionTokenCallback.
 * - The |ctx| is a pointer to some application specific context, which is
 *   returned when |cb| is called.
 */
#define SSL_SetResumptionTokenCallback(fd, cb, ctx)                     \
    SSL_EXPERIMENTAL_API(                                               \
        "SSL_SetResumptionTokenCallback",                               \
        (PRFileDesc * _fd, SSLResumptionTokenCallback _cb, void *_ctx), \
        (fd, cb, ctx))

/*
 * This allows setting a resumption token for a session.
 * The function returns SECSuccess iff the resumption token can be used,
 * SECFailure in any other case. The caller should remove the |token| from its
 * cache when the function returns SECFailure.
 *
 * - The |fd| argument is the socket file descriptor.
 * - The |token| is a pointer to the resumption token as byte array
 *   of length |len|.
 */
#define SSL_SetResumptionToken(fd, token, len)                              \
    SSL_EXPERIMENTAL_API(                                                   \
        "SSL_SetResumptionToken",                                           \
        (PRFileDesc * _fd, const PRUint8 *_token, const unsigned int _len), \
        (fd, token, len))

/* TLS 1.3 allows a server to set a limit on the number of bytes of early data
 * that can be received. This allows that limit to be set. This function has no
 * effect on a client. */
#define SSL_SetMaxEarlyDataSize(fd, size)                    \
    SSL_EXPERIMENTAL_API("SSL_SetMaxEarlyDataSize",          \
                         (PRFileDesc * _fd, PRUint32 _size), \
                         (fd, size))

/* Deprecated experimental APIs */

#define SSL_UseAltServerHelloType(fd, enable) SSL_DEPRECATED_EXPERIMENTAL_API

SEC_END_PROTOS

#endif /* __sslexp_h_ */
