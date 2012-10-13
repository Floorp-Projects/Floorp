/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _HTTPISH_PROTOCOL_H_
#define _HTTPISH_PROTOCOL_H_

/*
 * Some common headers only. There does not seem a whole lot of
 * sense in defining error codes since they tend to have different
 * semantics
 */

#define HTTPISH_HEADER_CONTENT_LENGTH "Content-Length"
#define HTTPISH_C_HEADER_CONTENT_LENGTH "l"
#define HTTPISH_HEADER_CONTENT_TYPE "Content-Type"
#define HTTPISH_HEADER_CONTENT_ENCODING "Content-Encoding"
#define HTTPISH_HEADER_CONTENT_ID   "Content-Id"

#define HTTPISH_HEADER_USER_AGENT "User-Agent"
#define HTTPISH_HEADER_SERVER "Server"
#define HTTPISH_HEADER_DATE "Date"

#define HTTPISH_HEADER_VIA "Via"
#define HTTPISH_HEADER_MAX_FORWARDS "Max-Forwards"
#define HTTPISH_HEADER_EXPIRES "Expires"
#define HTTPISH_HEADER_LOCATION "Location"

#define HTTPISH_HEADER_MIME_VERSION "Mime-Version"
#define uniqueBoundary "uniqueBoundary"

#endif /* _HTTPISH_PROTOCOL_H_ */
