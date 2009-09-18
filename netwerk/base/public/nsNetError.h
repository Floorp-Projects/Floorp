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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
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

#ifndef nsNetError_h__
#define nsNetError_h__

#include "nsError.h"


/* NETWORKING ERROR CODES */


/******************************************************************************
 * General async request error codes:
 * 
 * These error codes are commonly passed through callback methods to indicate
 * the status of some requested async request.
 *
 * For example, see nsIRequestObserver::onStopRequest.
 */

/**
 * The async request completed successfully.
 */
#define NS_BINDING_SUCCEEDED \
    NS_OK

/**
 * The async request failed for some unknown reason.
 */
#define NS_BINDING_FAILED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 1)

/**
 * The async request failed because it was aborted by some user action.
 */
#define NS_BINDING_ABORTED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 2)

/**
 * The async request has been "redirected" to a different async request.
 * (e.g., an HTTP redirect occured).
 *
 * This error code is used with load groups to notify the load group observer
 * when a request in the load group is redirected to another request.
 */
#define NS_BINDING_REDIRECTED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 3)

/**
 * The async request has been "retargeted" to a different "handler."
 *
 * This error code is used with load groups to notify the load group observer
 * when a request in the load group is removed from the load group and added
 * to a different load group.
 */
#define NS_BINDING_RETARGETED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 4)


/******************************************************************************
 * Miscellaneous error codes:
 *
 * These errors are not typically passed via onStopRequest.
 */

/**
 * The URI is malformed.
 */
#define NS_ERROR_MALFORMED_URI \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 10)

/**
 * The URI scheme corresponds to an unknown protocol handler.
 */
#define NS_ERROR_UNKNOWN_PROTOCOL \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 18)

/**
 * Returned from nsIChannel::asyncOpen to indicate that OnDataAvailable will
 * not be called because there is no content available.
 *
 * This is used by helper app style protocols (e.g., mailto).
 *
 * XXX perhaps this should be a success code.
 */
#define NS_ERROR_NO_CONTENT \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 17)

/**
 * The requested action could not be completed while the object is busy.
 *
 * Implementations of nsIChannel::asyncOpen will commonly return this error
 * if the channel has already been opened (and has not yet been closed).
 */
#define NS_ERROR_IN_PROGRESS \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 15)

/**
 * Returned from nsIChannel::asyncOpen when trying to open the channel again
 * (reopening is not supported).
 */
#define NS_ERROR_ALREADY_OPENED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 73)

/**
 * The content encoding of the source document was incorrect, for example
 * returning a plain HTML document advertised as Content-Encoding: gzip
 */
#define NS_ERROR_INVALID_CONTENT_ENCODING \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 27)

/******************************************************************************
 * Connectivity error codes:
 */

/**
 * The connection is already established.
 * XXX currently unused - consider removing.
 */
#define NS_ERROR_ALREADY_CONNECTED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 11)

/**
 * The connection does not exist.
 * XXX currently unused - consider removing.
 */
#define NS_ERROR_NOT_CONNECTED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 12)

/**
 * The connection attempt failed, for example, because no server was listening
 * at specified host:port.
 */
#define NS_ERROR_CONNECTION_REFUSED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 13)

/**
 * The connection attempt to a proxy failed.
 */
#define NS_ERROR_PROXY_CONNECTION_REFUSED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 72)

/**
 * The connection was lost due to a timeout error.
 */
#define NS_ERROR_NET_TIMEOUT \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 14)

/**
 * The requested action could not be completed while the networking library
 * is in the offline state.
 */
#define NS_ERROR_OFFLINE \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 16)

/**
 * The requested action was prohibited because it would have caused the
 * networking library to establish a connection to an unsafe or otherwise
 * banned port.
 */
#define NS_ERROR_PORT_ACCESS_NOT_ALLOWED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 19)

/**
 * The connection was established, but no data was ever received.
 */
#define NS_ERROR_NET_RESET \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 20)

/**
 * The connection was established, but the data transfer was interrupted.
 */
#define NS_ERROR_NET_INTERRUPT \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 71)

// XXX really need to better rationalize these error codes.  are consumers of
//     necko really expected to know how to discern the meaning of these??


/**
 * This request is not resumable, but it was tried to resume it, or to
 * request resume-specific data.
 */
#define NS_ERROR_NOT_RESUMABLE \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 25)

/**
 * It was attempted to resume the request, but the entity has changed in the
 * meantime.
 */
#define NS_ERROR_ENTITY_CHANGED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 32)

/**
 * The request failed as a result of a detected redirection loop.
 */
#define NS_ERROR_REDIRECT_LOOP \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 31)

/**
 * The request failed because the content type returned by the server was
 * not a type expected by the channel (for nested channels such as the JAR
 * channel).
 */
#define NS_ERROR_UNSAFE_CONTENT_TYPE \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 74)

/******************************************************************************
 * FTP specific error codes:
 *
 * XXX document me
 */

#define NS_ERROR_FTP_LOGIN \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 21)

#define NS_ERROR_FTP_CWD \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 22)

#define NS_ERROR_FTP_PASV \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 23)

#define NS_ERROR_FTP_PWD \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 24)

#define NS_ERROR_FTP_LIST \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 28)

/******************************************************************************
 * DNS specific error codes:
 */

/**
 * The lookup of a hostname failed.  This generally refers to the hostname
 * from the URL being loaded.
 */
#define NS_ERROR_UNKNOWN_HOST \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 30)

/**
 * A low or medium priority DNS lookup failed because the pending
 * queue was already full. High priorty (the default) always
 * makes room
 */
#define NS_ERROR_DNS_LOOKUP_QUEUE_FULL \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 33)

/**
 * The lookup of a proxy hostname failed.
 *
 * If a channel is configured to speak to a proxy server, then it will
 * generate this error if the proxy hostname cannot be resolved.
 */
#define NS_ERROR_UNKNOWN_PROXY_HOST \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 42)


/******************************************************************************
 * Socket specific error codes:
 */

/**
 * The specified socket type does not exist.
 */
#define NS_ERROR_UNKNOWN_SOCKET_TYPE \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 51)

/**
 * The specified socket type could not be created.
 */
#define NS_ERROR_SOCKET_CREATE_FAILED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 52)


/******************************************************************************
 * Cache specific error codes:
 *
 * XXX document me
 */

#define NS_ERROR_CACHE_KEY_NOT_FOUND \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 61)

#define NS_ERROR_CACHE_DATA_IS_STREAM \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 62)

#define NS_ERROR_CACHE_DATA_IS_NOT_STREAM \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 63)

#define NS_ERROR_CACHE_WAIT_FOR_VALIDATION \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 64)

#define NS_ERROR_CACHE_ENTRY_DOOMED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 65)

#define NS_ERROR_CACHE_READ_ACCESS_DENIED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 66)

#define NS_ERROR_CACHE_WRITE_ACCESS_DENIED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 67)

#define NS_ERROR_CACHE_IN_USE \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 68)

/**
 * Error passed through onStopRequest if the document could not be fetched
 * from the cache.
 */
#define NS_ERROR_DOCUMENT_NOT_CACHED \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 70)


/******************************************************************************
 * Effective TLD Service specific error codes:
 */

/**
 * The requested number of domain levels exceeds those present in the host string.
 */
#define NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 80)

/**
 * The host string is an IP address.
 */
#define NS_ERROR_HOST_IS_IP_ADDRESS \
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 81)


/******************************************************************************
 * StreamLoader specific result codes:
 */

/**
 * Result code returned by nsIStreamLoaderObserver to indicate that
 * the observer is taking over responsibility for the data buffer,
 * and the loader should NOT free it.
 */
#define NS_SUCCESS_ADOPTED_DATA \
    NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_NETWORK, 90)


#endif // !nsNetError_h__
