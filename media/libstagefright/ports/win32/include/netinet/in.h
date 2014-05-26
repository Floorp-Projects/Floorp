/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

#ifndef IN_H_
#define IN_H_

#include <stdint.h>

#if defined(_M_IX86) || defined(_M_AMD64)

static uint32_t
ntohl(uint32_t x)
{
  return x << 24 | (x << 8 & 0xff0000) | (x >> 8 & 0xff00) | x >> 24;
}

static uint16_t
ntohs(uint16_t x)
{
  return x << 8 | x >> 8;
}

static uint32_t
htonl(uint32_t x)
{
  return x << 24 | (x << 8 & 0xff0000) | (x >> 8 & 0xff00) | x >> 24;
}

static uint16_t
htons(uint16_t x)
{
  return x << 8 | x >> 8;
}

#else
#error Unsupported architecture
#endif

#endif
