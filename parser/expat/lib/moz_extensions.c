/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef IS_LITTLE_ENDIAN

#define PREFIX(ident) little2_ ## ident
#define BYTE_TYPE(p) LITTLE2_BYTE_TYPE(XmlGetUtf16InternalEncodingNS(), p)
#define IS_NAME_CHAR_MINBPC(p) LITTLE2_IS_NAME_CHAR_MINBPC(0, p)
#define IS_NMSTRT_CHAR_MINBPC(p) LITTLE2_IS_NMSTRT_CHAR_MINBPC(0, p)

#else

#define PREFIX(ident) big2_ ## ident
#define BYTE_TYPE(p) BIG2_BYTE_TYPE(XmlGetUtf16InternalEncodingNS(), p)
#define IS_NAME_CHAR_MINBPC(p) BIG2_IS_NAME_CHAR_MINBPC(0, p)
#define IS_NMSTRT_CHAR_MINBPC(p) BIG2_IS_NMSTRT_CHAR_MINBPC(0, p)

#endif

#define MOZ_EXPAT_VALID_QNAME       (0)
#define MOZ_EXPAT_EMPTY_QNAME       (1 << 0)
#define MOZ_EXPAT_INVALID_CHARACTER (1 << 1)
#define MOZ_EXPAT_MALFORMED         (1 << 2)

int MOZ_XMLCheckQName(const char* ptr, const char* end, int ns_aware,
                      const char** colon)
{
  int result = MOZ_EXPAT_VALID_QNAME;
  int nmstrt = 1;
  *colon = 0;
  if (ptr == end) {
    return MOZ_EXPAT_EMPTY_QNAME;
  }
  do {
    switch (BYTE_TYPE(ptr)) {
    case BT_COLON:
       /* We're namespace-aware and either first or last character is a colon
          or we've already seen a colon. */
      if (ns_aware && (nmstrt || *colon || ptr + 2 == end)) {
        return MOZ_EXPAT_MALFORMED;
      }
      *colon = ptr;
      nmstrt = ns_aware; /* e.g. "a:0" should be valid if !ns_aware */
      break;
    case BT_NONASCII:
      if (!IS_NAME_CHAR_MINBPC(ptr) ||
          (nmstrt && !*colon && !IS_NMSTRT_CHAR_MINBPC(ptr))) {
        return MOZ_EXPAT_INVALID_CHARACTER;
      }
      if (nmstrt && *colon && !IS_NMSTRT_CHAR_MINBPC(ptr)) {
        /* If a non-starting character like a number is right after the colon,
           this is a namespace error, not invalid character */
        return MOZ_EXPAT_MALFORMED;
      }
      nmstrt = 0;
      break;
    case BT_NMSTRT:
    case BT_HEX:
      nmstrt = 0;
      break;
    case BT_DIGIT:
    case BT_NAME:
    case BT_MINUS:
      if (nmstrt) {
        return MOZ_EXPAT_INVALID_CHARACTER;
      }
      break;
    default:
      return MOZ_EXPAT_INVALID_CHARACTER;
    }
    ptr += 2;
  } while (ptr != end);
  return result;
}

int MOZ_XMLIsLetter(const char* ptr)
{
  switch (BYTE_TYPE(ptr)) {
  case BT_NONASCII:
    if (!IS_NMSTRT_CHAR_MINBPC(ptr)) {
      return 0;
    }
    /* fall through */
  case BT_NMSTRT:
  case BT_HEX:
    return 1;
  default:
    return 0;
  }
}

int MOZ_XMLIsNCNameChar(const char* ptr)
{
  switch (BYTE_TYPE(ptr)) {
  case BT_NONASCII:
    if (!IS_NAME_CHAR_MINBPC(ptr)) {
      return 0;
    }
    /* fall through */
  case BT_NMSTRT:
  case BT_HEX:
  case BT_DIGIT:
  case BT_NAME:
  case BT_MINUS:
    return 1;
  default:
    return 0;
  }
}

int MOZ_XMLTranslateEntity(const char* ptr, const char* end, const char** next,
                           XML_Char* result)
{
  // Can we assert here somehow?
  // MOZ_ASSERT(*ptr == '&');

  const ENCODING* enc = XmlGetUtf16InternalEncodingNS();
  /* scanRef expects to be pointed to the char after the '&'. */
  int tok = PREFIX(scanRef)(enc, ptr + enc->minBytesPerChar, end, next);
  if (tok <= XML_TOK_INVALID) {
    return 0;
  }

  if (tok == XML_TOK_CHAR_REF) {
    /* XmlCharRefNumber expects to be pointed to the '&'. */
    int n = XmlCharRefNumber(enc, ptr);

    /* We could get away with just < 0, but better safe than sorry. */
    if (n <= 0) {
      return 0;
    }

    return XmlUtf16Encode(n, (unsigned short*)result);
  }

  if (tok == XML_TOK_ENTITY_REF) {
    /* XmlPredefinedEntityName expects to be pointed to the char after '&'.

       *next points to after the semicolon, so the entity ends at
       *next - enc->minBytesPerChar. */
    XML_Char ch =
      (XML_Char)XmlPredefinedEntityName(enc, ptr + enc->minBytesPerChar,
                                        *next - enc->minBytesPerChar);
    if (!ch) {
      return 0;
    }

    *result = ch;
    return 1;
  }

  return 0;
}

#undef PREFIX
#undef BYTE_TYPE
#undef IS_NAME_CHAR_MINBPC
#undef IS_NMSTRT_CHAR_MINBPC
