/* This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *   * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierTelemetryUtils.h"
#include "mozilla/Assertions.h"

namespace mozilla {
namespace safebrowsing {

uint8_t NetworkErrorToBucket(nsresult rv) {
  switch (rv) {
    // Connection errors
    case NS_ERROR_ALREADY_CONNECTED:
      return 2;
    case NS_ERROR_NOT_CONNECTED:
      return 3;
    case NS_ERROR_CONNECTION_REFUSED:
      return 4;
    case NS_ERROR_NET_TIMEOUT:
      return 5;
    case NS_ERROR_OFFLINE:
      return 6;
    case NS_ERROR_PORT_ACCESS_NOT_ALLOWED:
      return 7;
    case NS_ERROR_NET_RESET:
      return 8;
    case NS_ERROR_NET_INTERRUPT:
      return 9;
    case NS_ERROR_PROXY_CONNECTION_REFUSED:
      return 10;
    case NS_ERROR_NET_PARTIAL_TRANSFER:
      return 11;
    case NS_ERROR_NET_INADEQUATE_SECURITY:
      return 12;
    // DNS errors
    case NS_ERROR_UNKNOWN_HOST:
      return 13;
    case NS_ERROR_DNS_LOOKUP_QUEUE_FULL:
      return 14;
    case NS_ERROR_UNKNOWN_PROXY_HOST:
      return 15;
    // Others
    default:
      return 1;
  }
}

uint32_t HTTPStatusToBucket(uint32_t status) {
  uint32_t statusBucket;
  switch (status) {
    case 100:
    case 101:
      // Unexpected 1xx return code
      statusBucket = 0;
      break;
    case 200:
      // OK - Data is available in the HTTP response body.
      statusBucket = 1;
      break;
    case 201:
    case 202:
    case 203:
    case 205:
    case 206:
      // Unexpected 2xx return code
      statusBucket = 2;
      break;
    case 204:
      // No Content
      statusBucket = 3;
      break;
    case 300:
    case 301:
    case 302:
    case 303:
    case 304:
    case 305:
    case 307:
    case 308:
      // Unexpected 3xx return code
      statusBucket = 4;
      break;
    case 400:
      // Bad Request - The HTTP request was not correctly formed.
      // The client did not provide all required CGI parameters.
      statusBucket = 5;
      break;
    case 401:
    case 402:
    case 405:
    case 406:
    case 407:
    case 409:
    case 410:
    case 411:
    case 412:
    case 414:
    case 415:
    case 416:
    case 417:
    case 421:
    case 426:
    case 428:
    case 429:
    case 431:
    case 451:
      // Unexpected 4xx return code
      statusBucket = 6;
      break;
    case 403:
      // Forbidden - The client id is invalid.
      statusBucket = 7;
      break;
    case 404:
      // Not Found
      statusBucket = 8;
      break;
    case 408:
      // Request Timeout
      statusBucket = 9;
      break;
    case 413:
      // Request Entity Too Large - Bug 1150334
      statusBucket = 10;
      break;
    case 500:
    case 501:
    case 510:
      // Unexpected 5xx return code
      statusBucket = 11;
      break;
    case 502:
    case 504:
    case 511:
      // Local network errors, we'll ignore these.
      statusBucket = 12;
      break;
    case 503:
      // Service Unavailable - The server cannot handle the request.
      // Clients MUST follow the backoff behavior specified in the
      // Request Frequency section.
      statusBucket = 13;
      break;
    case 505:
      // HTTP Version Not Supported - The server CANNOT handle the requested
      // protocol major version.
      statusBucket = 14;
      break;
    default:
      statusBucket = 15;
  };
  return statusBucket;
}

}  // namespace safebrowsing
}  // namespace mozilla
