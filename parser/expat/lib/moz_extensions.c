/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Peter Van der Beken.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peter@propagandism.org>
 *
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
      if (nmstrt && !IS_NMSTRT_CHAR_MINBPC(ptr)) {
        /* If this is a valid name character and we're namespace-aware, the
           QName is malformed.  Otherwise, this character's invalid at the
           start of a name (or, if we're namespace-aware, at the start of a
           localpart). */
        return (IS_NAME_CHAR_MINBPC(ptr) && ns_aware) ?
               MOZ_EXPAT_MALFORMED :
               MOZ_EXPAT_INVALID_CHARACTER;
      }
      if (!IS_NAME_CHAR_MINBPC(ptr)) {
        return MOZ_EXPAT_INVALID_CHARACTER;
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
  const ENCODING* enc = XmlGetUtf16InternalEncodingNS();
  int tok = PREFIX(scanRef)(enc, ptr, end, next);
  if (tok <= XML_TOK_INVALID) {
    return 0;
  }

  if (tok == XML_TOK_CHAR_REF) {
    int n = XmlCharRefNumber(enc, ptr);

    /* We could get away with just < 0, but better safe than sorry. */
    if (n <= 0) {
      return 0;
    }

    return XmlUtf16Encode(n, (unsigned short*)result);
  }

  if (tok == XML_TOK_ENTITY_REF) {
    /* *next points to after the semicolon, so the entity ends at
       *next - enc->minBytesPerChar. */
    XML_Char ch =
      (XML_Char)XmlPredefinedEntityName(enc, ptr, *next - enc->minBytesPerChar);
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
