/*
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
License for the specific language governing rights and limitations
under the License.

The Original Code is expat.

The Initial Developer of the Original Code is James Clark.
Portions created by James Clark are Copyright (C) 1998
James Clark. All Rights Reserved.

Contributor(s):
*/

#include "xmldef.h"
#include "xmltok.h"
#include "nametab.h"

#define VTABLE1 \
  { PREFIX(prologTok), PREFIX(contentTok) }, \
  { PREFIX(attributeValueTok), PREFIX(entityValueTok) }, \
  PREFIX(sameName), \
  PREFIX(nameMatchesAscii), \
  PREFIX(nameLength), \
  PREFIX(skipS), \
  PREFIX(getAtts), \
  PREFIX(charRefNumber), \
  PREFIX(updatePosition), \
  PREFIX(isPublicId)

#define VTABLE2 \
  PREFIX(encode), \
  { PREFIX(toUtf8) }

#define VTABLE VTABLE1, VTABLE2

#define UCS2_GET_NAMING(pages, hi, lo) \
   (namingBitmap[(pages[hi] << 3) + ((lo) >> 5)] & (1 << ((lo) & 0x1F)))

/* A 2 byte UTF-8 representation splits the characters 11 bits
between the bottom 5 and 6 bits of the bytes.
We need 8 bits to index into pages, 3 bits to add to that index and
5 bits to generate the mask. */
#define UTF8_GET_NAMING2(pages, byte) \
    (namingBitmap[((pages)[(((byte)[0]) >> 2) & 7] << 3) \
                      + ((((byte)[0]) & 3) << 1) \
                      + ((((byte)[1]) >> 5) & 1)] \
         & (1 << (((byte)[1]) & 0x1F)))

/* A 3 byte UTF-8 representation splits the characters 16 bits
between the bottom 4, 6 and 6 bits of the bytes.
We need 8 bits to index into pages, 3 bits to add to that index and
5 bits to generate the mask. */
#define UTF8_GET_NAMING3(pages, byte) \
  (namingBitmap[((pages)[((((byte)[0]) & 0xF) << 4) \
                             + ((((byte)[1]) >> 2) & 0xF)] \
		       << 3) \
                      + ((((byte)[1]) & 3) << 1) \
                      + ((((byte)[2]) >> 5) & 1)] \
         & (1 << (((byte)[2]) & 0x1F)))

#define UTF8_GET_NAMING(pages, p, n) \
  ((n) == 2 \
  ? UTF8_GET_NAMING2(pages, (const unsigned char *)(p)) \
  : ((n) == 3 \
     ? UTF8_GET_NAMING3(pages, (const unsigned char *)(p)) \
     : 0))

#define UTF8_INVALID3(p) \
  ((*p) == 0xED \
  ? (((p)[1] & 0x20) != 0) \
  : ((*p) == 0xEF \
     ? ((p)[1] == 0xBF && ((p)[2] == 0xBF || (p)[2] == 0xBE)) \
     : 0))

#define UTF8_INVALID4(p) ((*p) == 0xF4 && ((p)[1] & 0x30) != 0)

struct normal_encoding {
  ENCODING enc;
  unsigned char type[256];
};

static int checkCharRefNumber(int);

#include "xmltok_impl.h"

/* minimum bytes per character */
#define MINBPC 1
#define BYTE_TYPE(enc, p) \
  (((struct normal_encoding *)(enc))->type[(unsigned char)*(p)])
#define BYTE_TO_ASCII(enc, p) (*p)
#define IS_NAME_CHAR(enc, p, n) UTF8_GET_NAMING(namePages, p, n)
#define IS_NMSTRT_CHAR(enc, p, n) UTF8_GET_NAMING(nmstrtPages, p, n)
#define IS_INVALID_CHAR(enc, p, n) \
((n) == 3 \
  ? UTF8_INVALID3((const unsigned char *)(p)) \
  : ((n) == 4 ? UTF8_INVALID4((const unsigned char *)(p)) : 0))

/* c is an ASCII character */
#define CHAR_MATCHES(enc, p, c) (*(p) == c)

#define PREFIX(ident) normal_ ## ident
#include "xmltok_impl.c"

#undef MINBPC
#undef BYTE_TYPE
#undef BYTE_TO_ASCII
#undef CHAR_MATCHES
#undef IS_NAME_CHAR
#undef IS_NMSTRT_CHAR
#undef IS_INVALID_CHAR

enum {
  /* cvalN is value of masked first byte of N byte sequence */
  cval1 = 0x00,
  cval2 = 0xc0,
  cval3 = 0xe0,
  cval4 = 0xf0,
  /* minN is minimum legal resulting value for N byte sequence */
  min2 = 0x80,
  min3 = 0x800,
  min4 = 0x10000
};

static
int utf8_encode(const ENCODING *enc, int c, char *buf)
{
  if (c < 0)
    return 0;
  if (c < min2) {
    buf[0] = (c | cval1);
    return 1;
  }
  if (c < min3) {
    buf[0] = ((c >> 6) | cval2);
    buf[1] = ((c & 0x3f) | 0x80);
    return 2;
  }
  if (c < min4) {
    buf[0] = ((c >> 12) | cval3);
    buf[1] = (((c >> 6) & 0x3f) | 0x80);
    buf[2] = ((c & 0x3f) | 0x80);
    return 3;
  }
  if (c < 0x110000) {
    buf[0] = ((c >> 18) | cval4);
    buf[1] = (((c >> 12) & 0x3f) | 0x80);
    buf[2] = (((c >> 6) & 0x3f) | 0x80);
    buf[3] = ((c & 0x3f) | 0x80);
    return 4;
  }
  return 0;
}

static
void utf8_toUtf8(const ENCODING *enc,
		 const char **fromP, const char *fromLim,
		 char **toP, const char *toLim)
{
  char *to;
  const char *from;
  if (fromLim - *fromP > toLim - *toP) {
    /* Avoid copying partial characters. */
    for (fromLim = *fromP + (toLim - *toP); fromLim > *fromP; fromLim--)
      if (((unsigned char)fromLim[-1] & 0xc0) != 0x80)
	break;
  }
  for (to = *toP, from = *fromP; from != fromLim; from++, to++)
    *to = *from;
  *fromP = from;
  *toP = to;
}

static const struct normal_encoding utf8_encoding = {
  { VTABLE1, utf8_encode, { utf8_toUtf8 }, 1 },
  {
#include "asciitab.h"
#include "utf8tab.h"
  }
};

static const struct normal_encoding internal_utf8_encoding = {
  { VTABLE1, utf8_encode, { utf8_toUtf8 }, 1 },
  {
#include "iasciitab.h"
#include "utf8tab.h"
  }
};

static
int latin1_encode(const ENCODING *enc, int c, char *buf)
{
  if (c < 0)
    return 0;
  if (c <= 0xFF) {
    buf[0] = (char)c;
    return 1;
  }
  return 0;
}

static
void latin1_toUtf8(const ENCODING *enc,
		   const char **fromP, const char *fromLim,
		   char **toP, const char *toLim)
{
  for (;;) {
    unsigned char c;
    if (*fromP == fromLim)
      break;
    c = (unsigned char)**fromP;
    if (c & 0x80) {
      if (toLim - *toP < 2)
	break;
      *(*toP)++ = ((c >> 6) | cval2);
      *(*toP)++ = ((c & 0x3f) | 0x80);
      (*fromP)++;
    }
    else {
      if (*toP == toLim)
	break;
      *(*toP)++ = *(*fromP)++;
    }
  }
}

static const struct normal_encoding latin1_encoding = {
  { VTABLE1, latin1_encode, { latin1_toUtf8 }, 1 },
  {
#include "asciitab.h"
#include "latin1tab.h"
  }
};

#define latin1tab (latin1_encoding.type)

#undef PREFIX

static int unicode_byte_type(char hi, char lo)
{
  switch ((unsigned char)hi) {
  case 0xD8: case 0xD9: case 0xDA: case 0xDB:
    return BT_LEAD4;
  case 0xDC: case 0xDD: case 0xDE: case 0xDF:
    return BT_TRAIL;
  case 0xFF:
    switch ((unsigned char)lo) {
    case 0xFF:
    case 0xFE:
      return BT_NONXML;
    }
    break;
  }
  return BT_NONASCII;
}

#define DEFINE_UTF16_ENCODE \
static \
int PREFIX(encode)(const ENCODING *enc, int charNum, char *buf) \
{ \
  if (charNum < 0) \
    return 0; \
  if (charNum < 0x10000) { \
    SET2(buf, charNum); \
    return 2; \
  } \
  if (charNum < 0x110000) { \
    charNum -= 0x10000; \
    SET2(buf, (charNum >> 10) + 0xD800); \
    SET2(buf + 2, (charNum & 0x3FF) + 0xDC00); \
    return 4; \
  } \
  return 0; \
}

#define DEFINE_UTF16_TO_UTF8 \
static \
void PREFIX(toUtf8)(const ENCODING *enc, \
		    const char **fromP, const char *fromLim, \
		    char **toP, const char *toLim) \
{ \
  const char *from; \
  for (from = *fromP; from != fromLim; from += 2) { \
    int plane; \
    unsigned char lo2; \
    unsigned char lo = GET_LO(from); \
    unsigned char hi = GET_HI(from); \
    switch (hi) { \
    case 0: \
      if (lo < 0x80) { \
        if (*toP == toLim) { \
          *fromP = from; \
	  return; \
        } \
        *(*toP)++ = lo; \
        break; \
      } \
      /* fall through */ \
    case 0x1: case 0x2: case 0x3: \
    case 0x4: case 0x5: case 0x6: case 0x7: \
      if (toLim -  *toP < 2) { \
        *fromP = from; \
	return; \
      } \
      *(*toP)++ = ((lo >> 6) | (hi << 2) |  cval2); \
      *(*toP)++ = ((lo & 0x3f) | 0x80); \
      break; \
    default: \
      if (toLim -  *toP < 3)  { \
        *fromP = from; \
	return; \
      } \
      /* 16 bits divided 4, 6, 6 amongst 3 bytes */ \
      *(*toP)++ = ((hi >> 4) | cval3); \
      *(*toP)++ = (((hi & 0xf) << 2) | (lo >> 6) | 0x80); \
      *(*toP)++ = ((lo & 0x3f) | 0x80); \
      break; \
    case 0xD8: case 0xD9: case 0xDA: case 0xDB: \
      if (toLim -  *toP < 4) { \
	*fromP = from; \
	return; \
      } \
      plane = (((hi & 0x3) << 2) | ((lo >> 6) & 0x3)) + 1; \
      *(*toP)++ = ((plane >> 2) | cval4); \
      *(*toP)++ = (((lo >> 2) & 0xF) | ((plane & 0x3) << 4) | 0x80); \
      from += 2; \
      lo2 = GET_LO(from); \
      *(*toP)++ = (((lo & 0x3) << 4) \
	           | ((GET_HI(from) & 0x3) << 2) \
		   | (lo2 >> 6) \
		   | 0x80); \
      *(*toP)++ = ((lo2 & 0x3f) | 0x80); \
      break; \
    } \
  } \
  *fromP = from; \
}

#define PREFIX(ident) little2_ ## ident
#define MINBPC 2
#define BYTE_TYPE(enc, p) \
 ((p)[1] == 0 ? latin1tab[(unsigned char)*(p)] : unicode_byte_type((p)[1], (p)[0]))
#define BYTE_TO_ASCII(enc, p) ((p)[1] == 0 ? (p)[0] : -1)
#define CHAR_MATCHES(enc, p, c) ((p)[1] == 0 && (p)[0] == c)
#define IS_NAME_CHAR(enc, p, n) \
  UCS2_GET_NAMING(namePages, (unsigned char)p[1], (unsigned char)p[0])
#define IS_NMSTRT_CHAR(enc, p, n) \
  UCS2_GET_NAMING(nmstrtPages, (unsigned char)p[1], (unsigned char)p[0])

#include "xmltok_impl.c"

#define SET2(ptr, ch) \
  (((ptr)[0] = ((ch) & 0xff)), ((ptr)[1] = ((ch) >> 8)))
#define GET_LO(ptr) ((unsigned char)(ptr)[0])
#define GET_HI(ptr) ((unsigned char)(ptr)[1])

DEFINE_UTF16_ENCODE
DEFINE_UTF16_TO_UTF8

#undef SET2
#undef GET_LO
#undef GET_HI
#undef MINBPC
#undef BYTE_TYPE
#undef BYTE_TO_ASCII
#undef CHAR_MATCHES
#undef IS_NAME_CHAR
#undef IS_NMSTRT_CHAR
#undef IS_INVALID_CHAR

static const struct encoding little2_encoding = { VTABLE, 2 };

#undef PREFIX

#define PREFIX(ident) big2_ ## ident
#define MINBPC 2
/* CHAR_MATCHES is guaranteed to have MINBPC bytes available. */
#define BYTE_TYPE(enc, p) \
 ((p)[0] == 0 ? latin1tab[(unsigned char)(p)[1]] : unicode_byte_type((p)[0], (p)[1]))
#define BYTE_TO_ASCII(enc, p) ((p)[0] == 0 ? (p)[1] : -1)
#define CHAR_MATCHES(enc, p, c) ((p)[0] == 0 && (p)[1] == c)
#define IS_NAME_CHAR(enc, p, n) \
  UCS2_GET_NAMING(namePages, (unsigned char)p[0], (unsigned char)p[1])
#define IS_NMSTRT_CHAR(enc, p, n) \
  UCS2_GET_NAMING(nmstrtPages, (unsigned char)p[0], (unsigned char)p[1])

#include "xmltok_impl.c"

#define SET2(ptr, ch) \
  (((ptr)[0] = ((ch) >> 8)), ((ptr)[1] = ((ch) & 0xFF)))
#define GET_LO(ptr) ((unsigned char)(ptr)[1])
#define GET_HI(ptr) ((unsigned char)(ptr)[0])

DEFINE_UTF16_ENCODE
DEFINE_UTF16_TO_UTF8

#undef SET2
#undef GET_LO
#undef GET_HI
#undef MINBPC
#undef BYTE_TYPE
#undef BYTE_TO_ASCII
#undef CHAR_MATCHES
#undef IS_NAME_CHAR
#undef IS_NMSTRT_CHAR
#undef IS_INVALID_CHAR

static const struct encoding big2_encoding = { VTABLE, 2 };

#undef PREFIX

static
int streqci(const char *s1, const char *s2)
{
  for (;;) {
    char c1 = *s1++;
    char c2 = *s2++;
    if ('a' <= c1 && c1 <= 'z')
      c1 += 'A' - 'a';
    if ('a' <= c2 && c2 <= 'z')
      c2 += 'A' - 'a';
    if (c1 != c2)
      return 0;
    if (!c1)
      break;
  }
  return 1;
}

static
int initScan(const ENCODING *enc, int state, const char *ptr, const char *end,
	     const char **nextTokPtr)
{
  const ENCODING **encPtr;

  if (ptr == end)
    return XML_TOK_NONE;
  encPtr = ((const INIT_ENCODING *)enc)->encPtr;
  if (ptr + 1 == end) {
    switch ((unsigned char)*ptr) {
    case 0xFE:
    case 0xFF:
    case 0x00:
    case 0x3C:
      return XML_TOK_PARTIAL;
    }
  }
  else {
    switch (((unsigned char)ptr[0] << 8) | (unsigned char)ptr[1]) {
    case 0x003C:
      *encPtr = &big2_encoding;
      return XmlTok(*encPtr, state, ptr, end, nextTokPtr);
    case 0xFEFF:
      *nextTokPtr = ptr + 2;
      *encPtr = &big2_encoding;
      return XML_TOK_BOM;
    case 0x3C00:
      *encPtr = &little2_encoding;
      return XmlTok(*encPtr, state, ptr, end, nextTokPtr);
    case 0xFFFE:
      *nextTokPtr = ptr + 2;
      *encPtr = &little2_encoding;
      return XML_TOK_BOM;
    }
  }
  *encPtr = &utf8_encoding.enc;
  return XmlTok(*encPtr, state, ptr, end, nextTokPtr);
}

static
int initScanProlog(const ENCODING *enc, const char *ptr, const char *end,
		   const char **nextTokPtr)
{
  return initScan(enc, XML_PROLOG_STATE, ptr, end, nextTokPtr);
}

static
int initScanContent(const ENCODING *enc, const char *ptr, const char *end,
		    const char **nextTokPtr)
{
  return initScan(enc, XML_CONTENT_STATE, ptr, end, nextTokPtr);
}

static
void initUpdatePosition(const ENCODING *enc, const char *ptr,
			const char *end, POSITION *pos)
{
  normal_updatePosition(&utf8_encoding.enc, ptr, end, pos);
}

const ENCODING *XmlGetInternalEncoding(int e)
{
  switch (e) {
  case XML_UTF8_ENCODING:
    return &internal_utf8_encoding.enc;
  }
  return 0;
}

int XmlInitEncoding(INIT_ENCODING *p, const ENCODING **encPtr, const char *name)
{
  if (name) {
    if (streqci(name, "ISO-8859-1")) {
      *encPtr = &latin1_encoding.enc;
      return 1;
    }
    if (streqci(name, "UTF-8")) {
      *encPtr = &utf8_encoding.enc;
      return 1;
    }
    if (!streqci(name, "UTF-16"))
      return 0;
  }
  p->initEnc.scanners[XML_PROLOG_STATE] = initScanProlog;
  p->initEnc.scanners[XML_CONTENT_STATE] = initScanContent;
  p->initEnc.updatePosition = initUpdatePosition;
  p->initEnc.minBytesPerChar = 1;
  p->encPtr = encPtr;
  *encPtr = &(p->initEnc);
  return 1;
}

static
int toAscii(const ENCODING *enc, const char *ptr, const char *end)
{
  char buf[1];
  char *p = buf;
  XmlConvert(enc, XML_UTF8_ENCODING, &ptr, end, &p, p + 1);
  if (p == buf)
    return -1;
  else
    return buf[0];
}

static
int isSpace(int c)
{
  switch (c) {
  case ' ':
  case '\r':
  case '\n':
  case '\t':
    return 1;
  }
  return 0;
}

/* Return 1 if there's just optional white space
or there's an S followed by name=val. */
static
int parsePseudoAttribute(const ENCODING *enc,
			 const char *ptr,
			 const char *end,
			 const char **namePtr,
			 const char **valPtr,
			 const char **nextTokPtr)
{
  int c;
  char open;
  if (ptr == end) {
    *namePtr = 0;
    return 1;
  }
  if (!isSpace(toAscii(enc, ptr, end))) {
    *nextTokPtr = ptr;
    return 0;
  }
  do {
    ptr += enc->minBytesPerChar;
  } while (isSpace(toAscii(enc, ptr, end)));
  if (ptr == end) {
    *namePtr = 0;
    return 1;
  }
  *namePtr = ptr;
  for (;;) {
    c = toAscii(enc, ptr, end);
    if (c == -1) {
      *nextTokPtr = ptr;
      return 0;
    }
    if (c == '=')
      break;
    if (isSpace(c)) {
      do {
	ptr += enc->minBytesPerChar;
      } while (isSpace(c = toAscii(enc, ptr, end)));
      if (c != '=') {
	*nextTokPtr = ptr;
	return 0;
      }
      break;
    }
    ptr += enc->minBytesPerChar;
  }
  if (ptr == *namePtr) {
    *nextTokPtr = ptr;
    return 0;
  }
  ptr += enc->minBytesPerChar;
  c = toAscii(enc, ptr, end);
  while (isSpace(c)) {
    ptr += enc->minBytesPerChar;
    c = toAscii(enc, ptr, end);
  }
  if (c != '"' && c != '\'') {
    *nextTokPtr = ptr;
    return 0;
  }
  open = c;
  ptr += enc->minBytesPerChar;
  *valPtr = ptr;
  for (;; ptr += enc->minBytesPerChar) {
    c = toAscii(enc, ptr, end);
    if (c == open)
      break;
    if (!('a' <= c && c <= 'z')
	&& !('A' <= c && c <= 'Z')
	&& !('0' <= c && c <= '9')
	&& c != '.'
	&& c != '-'
	&& c != '_') {
      *nextTokPtr = ptr;
      return 0;
    }
  }
  *nextTokPtr = ptr + enc->minBytesPerChar;
  return 1;
}

static
const ENCODING *findEncoding(const ENCODING *enc, const char *ptr, const char *end)
{
#define ENCODING_MAX 128
  char buf[ENCODING_MAX];
  char *p = buf;
  int i;
  XmlConvert(enc, XML_UTF8_ENCODING, &ptr, end, &p, p + ENCODING_MAX - 1);
  if (ptr != end)
    return 0;
  *p = 0;
  for (i = 0; buf[i]; i++) {
    if ('a' <= buf[i] && buf[i] <= 'z')
      buf[i] +=  'A' - 'a';
  }
  if (streqci(buf, "UTF-8"))
    return &utf8_encoding.enc;
  if (streqci(buf, "ISO-8859-1"))
    return &latin1_encoding.enc;
  if (streqci(buf, "UTF-16")) {
    static const unsigned short n = 1;
    if (enc->minBytesPerChar == 2)
      return enc;
    return &big2_encoding;
  }
  return 0;  
}

int XmlParseXmlDecl(int isGeneralTextEntity,
		    const ENCODING *enc,
		    const char *ptr,
		    const char *end,
		    const char **badPtr,
		    const char **versionPtr,
		    const char **encodingName,
		    const ENCODING **encoding,
		    int *standalone)
{
  const char *val = 0;
  const char *name = 0;
  ptr += 5 * enc->minBytesPerChar;
  end -= 2 * enc->minBytesPerChar;
  if (!parsePseudoAttribute(enc, ptr, end, &name, &val, &ptr) || !name) {
    *badPtr = ptr;
    return 0;
  }
  if (!XmlNameMatchesAscii(enc, name, "version")) {
    if (!isGeneralTextEntity) {
      *badPtr = name;
      return 0;
    }
  }
  else {
    if (versionPtr)
      *versionPtr = val;
    if (!parsePseudoAttribute(enc, ptr, end, &name, &val, &ptr)) {
      *badPtr = ptr;
      return 0;
    }
    if (!name)
      return 1;
  }
  if (XmlNameMatchesAscii(enc, name, "encoding")) {
    int c = toAscii(enc, val, end);
    if (!('a' <= c && c <= 'z') && !('A' <= c && c <= 'Z')) {
      *badPtr = val;
      return 0;
    }
    if (encodingName)
      *encodingName = val;
    if (encoding)
      *encoding = findEncoding(enc, val, ptr - enc->minBytesPerChar);
    if (!parsePseudoAttribute(enc, ptr, end, &name, &val, &ptr)) {
      *badPtr = ptr;
      return 0;
    }
    if (!name)
      return 1;
  }
  if (!XmlNameMatchesAscii(enc, name, "standalone") || isGeneralTextEntity) {
    *badPtr = name;
    return 0;
  }
  if (XmlNameMatchesAscii(enc, val, "yes")) {
    if (standalone)
      *standalone = 1;
  }
  else if (XmlNameMatchesAscii(enc, val, "no")) {
    if (standalone)
      *standalone = 0;
  }
  else {
    *badPtr = val;
    return 0;
  }
  while (isSpace(toAscii(enc, ptr, end)))
    ptr += enc->minBytesPerChar;
  if (ptr != end) {
    *badPtr = ptr;
    return 0;
  }
  return 1;
}

static
int checkCharRefNumber(int result)
{
  switch (result >> 8) {
  case 0xD8: case 0xD9: case 0xDA: case 0xDB:
  case 0xDC: case 0xDD: case 0xDE: case 0xDF:
    return -1;
  case 0:
    if (latin1_encoding.type[result] == BT_NONXML)
      return -1;
    break;
  case 0xFF:
    if (result == 0xFFFE || result == 0xFFFF)
      return -1;
    break;
  }
  return result;
}

