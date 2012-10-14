/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_WIN_IN_H_
#define _CPR_WIN_IN_H_

/*
 * macros to convert to and from network byte ordering
 * dummies if target is big endian
 */
#define __LITTLE_ENDIAN__ 1
#ifdef __LITTLE_ENDIAN__
# define htonl(addr)    ((((uint32_t)(addr) & 0x000000FF)<<24) | \
                         (((uint32_t)(addr) & 0x0000FF00)<< 8) | \
                         (((uint32_t)(addr) & 0x00FF0000)>> 8) | \
                         (((uint32_t)(addr) & 0xFF000000)>>24))
# define ntohl(addr)    htonl(addr)
# define htons(addr)     ((((uint16_t)(addr) & 0x000000FF)<< 8) |  \
                         (((uint16_t)(addr) & 0x0000FF00)>> 8))
# define ntohs(addr)    htons(addr)
#else
# define htonl(a)       ((uint32_t)(a))
# define ntohl(a)       ((uint32_t)(a))
# define htons(a)       ((uint16_t)(a))
# define ntohs(a)       ((uint16_t)(a))
#endif

#endif // #ifndef _CPR_WIN_IN_H_
