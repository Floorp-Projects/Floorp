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

#ifndef IS_INVALID_CHAR
#define IS_INVALID_CHAR(enc, ptr, n) (0)
#endif

#define INVALID_LEAD_CASE(n, ptr, nextTokPtr) \
    case BT_LEAD ## n: \
      if (end - ptr < n) \
	return XML_TOK_PARTIAL_CHAR; \
      if (IS_INVALID_CHAR(enc, ptr, n)) { \
        *(nextTokPtr) = (ptr); \
        return XML_TOK_INVALID; \
      } \
      ptr += n; \
      break;

#define INVALID_CASES(ptr, nextTokPtr) \
  INVALID_LEAD_CASE(2, ptr, nextTokPtr) \
  INVALID_LEAD_CASE(3, ptr, nextTokPtr) \
  INVALID_LEAD_CASE(4, ptr, nextTokPtr) \
  case BT_NONXML: \
  case BT_MALFORM: \
  case BT_TRAIL: \
    *(nextTokPtr) = (ptr); \
    return XML_TOK_INVALID;

#define CHECK_NAME_CASE(n, enc, ptr, end, nextTokPtr) \
   case BT_LEAD ## n: \
     if (end - ptr < n) \
       return XML_TOK_PARTIAL_CHAR; \
     if (!IS_NAME_CHAR(enc, ptr, n)) { \
       *nextTokPtr = ptr; \
       return XML_TOK_INVALID; \
     } \
     ptr += n; \
     break;

#define CHECK_NAME_CASES(enc, ptr, end, nextTokPtr) \
  case BT_NONASCII: \
    if (!IS_NAME_CHAR_MINBPC(enc, ptr)) { \
      *nextTokPtr = ptr; \
      return XML_TOK_INVALID; \
    } \
  case BT_NMSTRT: \
  case BT_HEX: \
  case BT_DIGIT: \
  case BT_NAME: \
  case BT_MINUS: \
    ptr += MINBPC; \
    break; \
  CHECK_NAME_CASE(2, enc, ptr, end, nextTokPtr) \
  CHECK_NAME_CASE(3, enc, ptr, end, nextTokPtr) \
  CHECK_NAME_CASE(4, enc, ptr, end, nextTokPtr)

#define CHECK_NMSTRT_CASE(n, enc, ptr, end, nextTokPtr) \
   case BT_LEAD ## n: \
     if (end - ptr < n) \
       return XML_TOK_PARTIAL_CHAR; \
     if (!IS_NMSTRT_CHAR(enc, ptr, n)) { \
       *nextTokPtr = ptr; \
       return XML_TOK_INVALID; \
     } \
     ptr += n; \
     break;

#define CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr) \
  case BT_NONASCII: \
    if (!IS_NMSTRT_CHAR_MINBPC(enc, ptr)) { \
      *nextTokPtr = ptr; \
      return XML_TOK_INVALID; \
    } \
  case BT_NMSTRT: \
  case BT_HEX: \
    ptr += MINBPC; \
    break; \
  CHECK_NMSTRT_CASE(2, enc, ptr, end, nextTokPtr) \
  CHECK_NMSTRT_CASE(3, enc, ptr, end, nextTokPtr) \
  CHECK_NMSTRT_CASE(4, enc, ptr, end, nextTokPtr)

#ifndef PREFIX
#define PREFIX(ident) ident
#endif

/* ptr points to character following "<!-" */

static
int PREFIX(scanComment)(const ENCODING *enc, const char *ptr, const char *end,
			const char **nextTokPtr)
{
  if (ptr != end) {
    if (!CHAR_MATCHES(enc, ptr, '-')) {
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
    ptr += MINBPC;
    while (ptr != end) {
      switch (BYTE_TYPE(enc, ptr)) {
      INVALID_CASES(ptr, nextTokPtr)
      case BT_MINUS:
	if ((ptr += MINBPC) == end)
	  return XML_TOK_PARTIAL;
	if (CHAR_MATCHES(enc, ptr, '-')) {
	  if ((ptr += MINBPC) == end)
	    return XML_TOK_PARTIAL;
	  if (!CHAR_MATCHES(enc, ptr, '>')) {
	    *nextTokPtr = ptr;
	    return XML_TOK_INVALID;
	  }
	  *nextTokPtr = ptr + MINBPC;
	  return XML_TOK_COMMENT;
	}
	/* fall through */
      default:
	ptr += MINBPC;
	break;
      }
    }
  }
  return XML_TOK_PARTIAL;
}

/* ptr points to character following "<!" */

static
int PREFIX(scanDecl)(const ENCODING *enc, const char *ptr, const char *end,
		     const char **nextTokPtr)
{
  if (ptr == end)
    return XML_TOK_PARTIAL;
  switch (BYTE_TYPE(enc, ptr)) {
  case BT_MINUS:
    return PREFIX(scanComment)(enc, ptr + MINBPC, end, nextTokPtr);
  case BT_LSQB:
    *nextTokPtr = ptr + MINBPC;
    return XML_TOK_COND_SECT_OPEN;
  case BT_NMSTRT:
  case BT_HEX:
    ptr += MINBPC;
    break;
  default:
    *nextTokPtr = ptr;
    return XML_TOK_INVALID;
  }
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
    case BT_PERCNT:
      if (ptr + MINBPC == end)
	return XML_TOK_PARTIAL;
      /* don't allow <!ENTITY% foo "whatever"> */
      switch (BYTE_TYPE(enc, ptr + MINBPC)) {
      case BT_S: case BT_CR: case BT_LF: case BT_PERCNT:
	*nextTokPtr = ptr;
	return XML_TOK_INVALID;
      }
      /* fall through */
    case BT_S: case BT_CR: case BT_LF:
      *nextTokPtr = ptr;
      return XML_TOK_DECL_OPEN;
    case BT_NMSTRT:
    case BT_HEX:
      ptr += MINBPC;
      break;
    default:
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
  }
  return XML_TOK_PARTIAL;
}

static
int PREFIX(checkPiTarget)(const ENCODING *enc, const char *ptr, const char *end, int *tokPtr)
{
  int upper = 0;
  *tokPtr = XML_TOK_PI;
  if (end - ptr != MINBPC*3)
    return 1;
  switch (BYTE_TO_ASCII(enc, ptr)) {
  case 'x':
    break;
  case 'X':
    upper = 1;
    break;
  default:
    return 1;
  }
  ptr += MINBPC;
  switch (BYTE_TO_ASCII(enc, ptr)) {
  case 'm':
    break;
  case 'M':
    upper = 1;
    break;
  default:
    return 1;
  }
  ptr += MINBPC;
  switch (BYTE_TO_ASCII(enc, ptr)) {
  case 'l':
    break;
  case 'L':
    upper = 1;
    break;
  default:
    return 1;
  }
  if (upper)
    return 0;
  *tokPtr = XML_TOK_XML_DECL;
  return 1;
}

/* ptr points to character following "<?" */

static
int PREFIX(scanPi)(const ENCODING *enc, const char *ptr, const char *end,
		   const char **nextTokPtr)
{
  int tok;
  const char *target = ptr;
  if (ptr == end)
    return XML_TOK_PARTIAL;
  switch (BYTE_TYPE(enc, ptr)) {
  CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
  default:
    *nextTokPtr = ptr;
    return XML_TOK_INVALID;
  }
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
    CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
    case BT_S: case BT_CR: case BT_LF:
      if (!PREFIX(checkPiTarget)(enc, target, ptr, &tok)) {
	*nextTokPtr = ptr;
	return XML_TOK_INVALID;
      }
      ptr += MINBPC;
      while (ptr != end) {
        switch (BYTE_TYPE(enc, ptr)) {
        INVALID_CASES(ptr, nextTokPtr)
	case BT_QUEST:
	  ptr += MINBPC;
	  if (ptr == end)
	    return XML_TOK_PARTIAL;
	  if (CHAR_MATCHES(enc, ptr, '>')) {
	    *nextTokPtr = ptr + MINBPC;
	    return tok;
	  }
	  break;
	default:
	  ptr += MINBPC;
	  break;
	}
      }
      return XML_TOK_PARTIAL;
    case BT_QUEST:
      if (!PREFIX(checkPiTarget)(enc, target, ptr, &tok)) {
	*nextTokPtr = ptr;
	return XML_TOK_INVALID;
      }
      ptr += MINBPC;
      if (ptr == end)
	return XML_TOK_PARTIAL;
      if (CHAR_MATCHES(enc, ptr, '>')) {
	*nextTokPtr = ptr + MINBPC;
	return tok;
      }
      /* fall through */
    default:
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
  }
  return XML_TOK_PARTIAL;
}


static
int PREFIX(scanCdataSection)(const ENCODING *enc, const char *ptr, const char *end,
			     const char **nextTokPtr)
{
  int i;
  /* CDATA[ */
  if (end - ptr < 6 * MINBPC)
    return XML_TOK_PARTIAL;
  for (i = 0; i < 6; i++, ptr += MINBPC) {
    if (!CHAR_MATCHES(enc, ptr, "CDATA["[i])) {
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
  }
  *nextTokPtr = ptr;
  return XML_TOK_CDATA_SECT_OPEN;
}

static
int PREFIX(cdataSectionTok)(const ENCODING *enc, const char *ptr, const char *end,
			    const char **nextTokPtr)
{
  if (ptr == end)
    return XML_TOK_NONE;
#if MINBPC > 1
  {
    size_t n = end - ptr;
    if (n & (MINBPC - 1)) {
      n &= ~(MINBPC - 1);
      if (n == 0)
	return XML_TOK_PARTIAL;
      end = ptr + n;
    }
  }
#endif
  switch (BYTE_TYPE(enc, ptr)) {
  case BT_RSQB:
    ptr += MINBPC;
    if (ptr == end)
      return XML_TOK_PARTIAL;
    if (!CHAR_MATCHES(enc, ptr, ']'))
      break;
    ptr += MINBPC;
    if (ptr == end)
      return XML_TOK_PARTIAL;
    if (!CHAR_MATCHES(enc, ptr, '>')) {
      ptr -= MINBPC;
      break;
    }
    *nextTokPtr = ptr + MINBPC;
    return XML_TOK_CDATA_SECT_CLOSE;
  case BT_CR:
    ptr += MINBPC;
    if (ptr == end)
      return XML_TOK_PARTIAL;
    if (BYTE_TYPE(enc, ptr) == BT_LF)
      ptr += MINBPC;
    *nextTokPtr = ptr;
    return XML_TOK_DATA_NEWLINE;
  case BT_LF:
    *nextTokPtr = ptr + MINBPC;
    return XML_TOK_DATA_NEWLINE;
  INVALID_CASES(ptr, nextTokPtr)
  default:
    ptr += MINBPC;
    break;
  }
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
#define LEAD_CASE(n) \
    case BT_LEAD ## n: \
      if (end - ptr < n || IS_INVALID_CHAR(enc, ptr, n)) { \
	*nextTokPtr = ptr; \
	return XML_TOK_DATA_CHARS; \
      } \
      ptr += n; \
      break;
    LEAD_CASE(2) LEAD_CASE(3) LEAD_CASE(4)
#undef LEAD_CASE
    case BT_NONXML:
    case BT_MALFORM:
    case BT_TRAIL:
    case BT_CR:
    case BT_LF:
    case BT_RSQB:
      *nextTokPtr = ptr;
      return XML_TOK_DATA_CHARS;
    default:
      ptr += MINBPC;
      break;
    }
  }
  *nextTokPtr = ptr;
  return XML_TOK_DATA_CHARS;
}

/* ptr points to character following "</" */

static
int PREFIX(scanEndTag)(const ENCODING *enc, const char *ptr, const char *end,
		       const char **nextTokPtr)
{
  if (ptr == end)
    return XML_TOK_PARTIAL;
  switch (BYTE_TYPE(enc, ptr)) {
  CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
  default:
    *nextTokPtr = ptr;
    return XML_TOK_INVALID;
  }
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
    CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
    case BT_S: case BT_CR: case BT_LF:
      for (ptr += MINBPC; ptr != end; ptr += MINBPC) {
	switch (BYTE_TYPE(enc, ptr)) {
	case BT_S: case BT_CR: case BT_LF:
	  break;
	case BT_GT:
	  *nextTokPtr = ptr + MINBPC;
          return XML_TOK_END_TAG;
	default:
	  *nextTokPtr = ptr;
	  return XML_TOK_INVALID;
	}
      }
      return XML_TOK_PARTIAL;
    case BT_GT:
      *nextTokPtr = ptr + MINBPC;
      return XML_TOK_END_TAG;
    default:
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
  }
  return XML_TOK_PARTIAL;
}

/* ptr points to character following "&#X" */

static
int PREFIX(scanHexCharRef)(const ENCODING *enc, const char *ptr, const char *end,
			   const char **nextTokPtr)
{
  if (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
    case BT_DIGIT:
    case BT_HEX:
      break;
    default:
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
    for (ptr += MINBPC; ptr != end; ptr += MINBPC) {
      switch (BYTE_TYPE(enc, ptr)) {
      case BT_DIGIT:
      case BT_HEX:
	break;
      case BT_SEMI:
	*nextTokPtr = ptr + MINBPC;
	return XML_TOK_CHAR_REF;
      default:
	*nextTokPtr = ptr;
	return XML_TOK_INVALID;
      }
    }
  }
  return XML_TOK_PARTIAL;
}

/* ptr points to character following "&#" */

static
int PREFIX(scanCharRef)(const ENCODING *enc, const char *ptr, const char *end,
			const char **nextTokPtr)
{
  if (ptr != end) {
    if (CHAR_MATCHES(enc, ptr, 'x'))
      return PREFIX(scanHexCharRef)(enc, ptr + MINBPC, end, nextTokPtr);
    switch (BYTE_TYPE(enc, ptr)) {
    case BT_DIGIT:
      break;
    default:
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
    for (ptr += MINBPC; ptr != end; ptr += MINBPC) {
      switch (BYTE_TYPE(enc, ptr)) {
      case BT_DIGIT:
	break;
      case BT_SEMI:
	*nextTokPtr = ptr + MINBPC;
	return XML_TOK_CHAR_REF;
      default:
	*nextTokPtr = ptr;
	return XML_TOK_INVALID;
      }
    }
  }
  return XML_TOK_PARTIAL;
}

/* ptr points to character following "&" */

static
int PREFIX(scanRef)(const ENCODING *enc, const char *ptr, const char *end,
		    const char **nextTokPtr)
{
  if (ptr == end)
    return XML_TOK_PARTIAL;
  switch (BYTE_TYPE(enc, ptr)) {
  CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
  case BT_NUM:
    return PREFIX(scanCharRef)(enc, ptr + MINBPC, end, nextTokPtr);
  default:
    *nextTokPtr = ptr;
    return XML_TOK_INVALID;
  }
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
    CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
    case BT_SEMI:
      *nextTokPtr = ptr + MINBPC;
      return XML_TOK_ENTITY_REF;
    default:
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
  }
  return XML_TOK_PARTIAL;
}

/* ptr points to character following first character of attribute name */

static
int PREFIX(scanAtts)(const ENCODING *enc, const char *ptr, const char *end,
		     const char **nextTokPtr)
{
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
    CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
    case BT_S: case BT_CR: case BT_LF:
      for (;;) {
	int t;

	ptr += MINBPC;
	if (ptr == end)
	  return XML_TOK_PARTIAL;
	t = BYTE_TYPE(enc, ptr);
	if (t == BT_EQUALS)
	  break;
	switch (t) {
	case BT_S:
	case BT_LF:
	case BT_CR:
	  break;
	default:
	  *nextTokPtr = ptr;
	  return XML_TOK_INVALID;
	}
      }
    /* fall through */
    case BT_EQUALS:
      {
	int open;
	for (;;) {
	  
	  ptr += MINBPC;
	  if (ptr == end)
	    return XML_TOK_PARTIAL;
	  open = BYTE_TYPE(enc, ptr);
	  if (open == BT_QUOT || open == BT_APOS)
	    break;
	  switch (open) {
	  case BT_S:
	  case BT_LF:
	  case BT_CR:
	    break;
	  default:
	    *nextTokPtr = ptr;
	    return XML_TOK_INVALID;
	  }
	}
	ptr += MINBPC;
	/* in attribute value */
	for (;;) {
	  int t;
	  if (ptr == end)
	    return XML_TOK_PARTIAL;
	  t = BYTE_TYPE(enc, ptr);
	  if (t == open)
	    break;
	  switch (t) {
	  INVALID_CASES(ptr, nextTokPtr)
	  case BT_AMP:
	    {
	      int tok = PREFIX(scanRef)(enc, ptr + MINBPC, end, &ptr);
	      if (tok <= 0) {
		if (tok == XML_TOK_INVALID)
		  *nextTokPtr = ptr;
		return tok;
	      }
	      break;
	    }
	  case BT_LT:
	    *nextTokPtr = ptr;
	    return XML_TOK_INVALID;
	  default:
	    ptr += MINBPC;
	    break;
	  }
	}
	ptr += MINBPC;
	if (ptr == end)
	  return XML_TOK_PARTIAL;
	switch (BYTE_TYPE(enc, ptr)) {
	case BT_S:
	case BT_CR:
	case BT_LF:
	  break;
	case BT_SOL:
	  goto sol;
	case BT_GT:
	  goto gt;
	default:
	  *nextTokPtr = ptr;
	  return XML_TOK_INVALID;
	}
	/* ptr points to closing quote */
	for (;;) {
	  ptr += MINBPC;
	  if (ptr == end)
	    return XML_TOK_PARTIAL;
	  switch (BYTE_TYPE(enc, ptr)) {
	  CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
	  case BT_S: case BT_CR: case BT_LF:
	    continue;
	  case BT_GT:
          gt:
	    *nextTokPtr = ptr + MINBPC;
	    return XML_TOK_START_TAG_WITH_ATTS;
	  case BT_SOL:
          sol:
	    ptr += MINBPC;
	    if (ptr == end)
	      return XML_TOK_PARTIAL;
	    if (!CHAR_MATCHES(enc, ptr, '>')) {
	      *nextTokPtr = ptr;
	      return XML_TOK_INVALID;
	    }
	    *nextTokPtr = ptr + MINBPC;
	    return XML_TOK_EMPTY_ELEMENT_WITH_ATTS;
	  default:
	    *nextTokPtr = ptr;
	    return XML_TOK_INVALID;
	  }
	  break;
	}
	break;
      }
    default:
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
  }
  return XML_TOK_PARTIAL;
}

/* ptr points to character following "<" */

static
int PREFIX(scanLt)(const ENCODING *enc, const char *ptr, const char *end,
		   const char **nextTokPtr)
{
  if (ptr == end)
    return XML_TOK_PARTIAL;
  switch (BYTE_TYPE(enc, ptr)) {
  CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
  case BT_EXCL:
    if ((ptr += MINBPC) == end)
      return XML_TOK_PARTIAL;
    switch (BYTE_TYPE(enc, ptr)) {
    case BT_MINUS:
      return PREFIX(scanComment)(enc, ptr + MINBPC, end, nextTokPtr);
    case BT_LSQB:
      return PREFIX(scanCdataSection)(enc, ptr + MINBPC, end, nextTokPtr);
    }
    *nextTokPtr = ptr;
    return XML_TOK_INVALID;
  case BT_QUEST:
    return PREFIX(scanPi)(enc, ptr + MINBPC, end, nextTokPtr);
  case BT_SOL:
    return PREFIX(scanEndTag)(enc, ptr + MINBPC, end, nextTokPtr);
  default:
    *nextTokPtr = ptr;
    return XML_TOK_INVALID;
  }
  /* we have a start-tag */
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
    CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
    case BT_S: case BT_CR: case BT_LF:
      {
        ptr += MINBPC;
	while (ptr != end) {
	  switch (BYTE_TYPE(enc, ptr)) {
	  CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
	  case BT_GT:
	    goto gt;
	  case BT_SOL:
	    goto sol;
	  case BT_S: case BT_CR: case BT_LF:
	    ptr += MINBPC;
	    continue;
	  default:
	    *nextTokPtr = ptr;
	    return XML_TOK_INVALID;
	  }
	  return PREFIX(scanAtts)(enc, ptr, end, nextTokPtr);
	}
	return XML_TOK_PARTIAL;
      }
    case BT_GT:
    gt:
      *nextTokPtr = ptr + MINBPC;
      return XML_TOK_START_TAG_NO_ATTS;
    case BT_SOL:
    sol:
      ptr += MINBPC;
      if (ptr == end)
	return XML_TOK_PARTIAL;
      if (!CHAR_MATCHES(enc, ptr, '>')) {
	*nextTokPtr = ptr;
	return XML_TOK_INVALID;
      }
      *nextTokPtr = ptr + MINBPC;
      return XML_TOK_EMPTY_ELEMENT_NO_ATTS;
    default:
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
  }
  return XML_TOK_PARTIAL;
}

static
int PREFIX(contentTok)(const ENCODING *enc, const char *ptr, const char *end,
		       const char **nextTokPtr)
{
  if (ptr == end)
    return XML_TOK_NONE;
#if MINBPC > 1
  {
    size_t n = end - ptr;
    if (n & (MINBPC - 1)) {
      n &= ~(MINBPC - 1);
      if (n == 0)
	return XML_TOK_PARTIAL;
      end = ptr + n;
    }
  }
#endif
  switch (BYTE_TYPE(enc, ptr)) {
  case BT_LT:
    return PREFIX(scanLt)(enc, ptr + MINBPC, end, nextTokPtr);
  case BT_AMP:
    return PREFIX(scanRef)(enc, ptr + MINBPC, end, nextTokPtr);
  case BT_CR:
    ptr += MINBPC;
    if (ptr == end)
      return XML_TOK_TRAILING_CR;
    if (BYTE_TYPE(enc, ptr) == BT_LF)
      ptr += MINBPC;
    *nextTokPtr = ptr;
    return XML_TOK_DATA_NEWLINE;
  case BT_LF:
    *nextTokPtr = ptr + MINBPC;
    return XML_TOK_DATA_NEWLINE;
  case BT_RSQB:
    ptr += MINBPC;
    if (ptr == end)
      return XML_TOK_TRAILING_RSQB;
    if (!CHAR_MATCHES(enc, ptr, ']'))
      break;
    ptr += MINBPC;
    if (ptr == end)
      return XML_TOK_TRAILING_RSQB;
    if (!CHAR_MATCHES(enc, ptr, '>')) {
      ptr -= MINBPC;
      break;
    }
    *nextTokPtr = ptr;
    return XML_TOK_INVALID;
  INVALID_CASES(ptr, nextTokPtr)
  default:
    ptr += MINBPC;
    break;
  }
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
#define LEAD_CASE(n) \
    case BT_LEAD ## n: \
      if (end - ptr < n || IS_INVALID_CHAR(enc, ptr, n)) { \
	*nextTokPtr = ptr; \
	return XML_TOK_DATA_CHARS; \
      } \
      ptr += n; \
      break;
    LEAD_CASE(2) LEAD_CASE(3) LEAD_CASE(4)
#undef LEAD_CASE
    case BT_RSQB:
      if (ptr + MINBPC != end) {
	 if (!CHAR_MATCHES(enc, ptr + MINBPC, ']')) {
	   ptr += MINBPC;
	   break;
	 }
	 if (ptr + 2*MINBPC != end) {
	   if (!CHAR_MATCHES(enc, ptr + 2*MINBPC, '>')) {
	     ptr += MINBPC;
	     break;
	   }
	   *nextTokPtr = ptr + 2*MINBPC;
	   return XML_TOK_INVALID;
	 }
      }
      /* fall through */
    case BT_AMP:
    case BT_LT:
    case BT_NONXML:
    case BT_MALFORM:
    case BT_TRAIL:
    case BT_CR:
    case BT_LF:
      *nextTokPtr = ptr;
      return XML_TOK_DATA_CHARS;
    default:
      ptr += MINBPC;
      break;
    }
  }
  *nextTokPtr = ptr;
  return XML_TOK_DATA_CHARS;
}

/* ptr points to character following "%" */

static
int PREFIX(scanPercent)(const ENCODING *enc, const char *ptr, const char *end,
			const char **nextTokPtr)
{
  if (ptr == end)
    return XML_TOK_PARTIAL;
  switch (BYTE_TYPE(enc, ptr)) {
  CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
  case BT_S: case BT_LF: case BT_CR: case BT_PERCNT:
    *nextTokPtr = ptr;
    return XML_TOK_PERCENT;
  default:
    *nextTokPtr = ptr;
    return XML_TOK_INVALID;
  }
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
    CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
    case BT_SEMI:
      *nextTokPtr = ptr + MINBPC;
      return XML_TOK_PARAM_ENTITY_REF;
    default:
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
  }
  return XML_TOK_PARTIAL;
}

static
int PREFIX(scanPoundName)(const ENCODING *enc, const char *ptr, const char *end,
			  const char **nextTokPtr)
{
  if (ptr == end)
    return XML_TOK_PARTIAL;
  switch (BYTE_TYPE(enc, ptr)) {
  CHECK_NMSTRT_CASES(enc, ptr, end, nextTokPtr)
  default:
    *nextTokPtr = ptr;
    return XML_TOK_INVALID;
  }
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
    CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
    case BT_CR: case BT_LF: case BT_S:
    case BT_RPAR: case BT_GT: case BT_PERCNT: case BT_VERBAR:
      *nextTokPtr = ptr;
      return XML_TOK_POUND_NAME;
    default:
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
  }
  return XML_TOK_PARTIAL;
}

static
int PREFIX(scanLit)(int open, const ENCODING *enc,
		    const char *ptr, const char *end,
		    const char **nextTokPtr)
{
  while (ptr != end) {
    int t = BYTE_TYPE(enc, ptr);
    switch (t) {
    INVALID_CASES(ptr, nextTokPtr)
    case BT_QUOT:
    case BT_APOS:
      ptr += MINBPC;
      if (t != open)
	break;
      if (ptr == end)
	return XML_TOK_PARTIAL;
      *nextTokPtr = ptr;
      switch (BYTE_TYPE(enc, ptr)) {
      case BT_S: case BT_CR: case BT_LF:
      case BT_GT: case BT_PERCNT: case BT_LSQB:
	return XML_TOK_LITERAL;
      default:
	return XML_TOK_INVALID;
      }
    default:
      ptr += MINBPC;
      break;
    }
  }
  return XML_TOK_PARTIAL;
}

static
int PREFIX(prologTok)(const ENCODING *enc, const char *ptr, const char *end,
		      const char **nextTokPtr)
{
  int tok;
  if (ptr == end)
    return XML_TOK_NONE;
#if MINBPC > 1
  {
    size_t n = end - ptr;
    if (n & (MINBPC - 1)) {
      n &= ~(MINBPC - 1);
      if (n == 0)
	return XML_TOK_PARTIAL;
      end = ptr + n;
    }
  }
#endif
  switch (BYTE_TYPE(enc, ptr)) {
  case BT_QUOT:
    return PREFIX(scanLit)(BT_QUOT, enc, ptr + MINBPC, end, nextTokPtr);
  case BT_APOS:
    return PREFIX(scanLit)(BT_APOS, enc, ptr + MINBPC, end, nextTokPtr);
  case BT_LT:
    {
      ptr += MINBPC;
      if (ptr == end)
	return XML_TOK_PARTIAL;
      switch (BYTE_TYPE(enc, ptr)) {
      case BT_EXCL:
	return PREFIX(scanDecl)(enc, ptr + MINBPC, end, nextTokPtr);
      case BT_QUEST:
	return PREFIX(scanPi)(enc, ptr + MINBPC, end, nextTokPtr);
      case BT_NMSTRT:
      case BT_HEX:
      case BT_NONASCII:
      case BT_LEAD2:
      case BT_LEAD3:
      case BT_LEAD4:
	*nextTokPtr = ptr - MINBPC;
	return XML_TOK_INSTANCE_START;
      }
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
  case BT_CR:
    if (ptr + MINBPC == end)
      return XML_TOK_TRAILING_CR;
    /* fall through */
  case BT_S: case BT_LF:
    for (;;) {
      ptr += MINBPC;
      if (ptr == end)
	break;
      switch (BYTE_TYPE(enc, ptr)) {
      case BT_S: case BT_LF:
	break;
      case BT_CR:
	/* don't split CR/LF pair */
	if (ptr + MINBPC != end)
	  break;
	/* fall through */
      default:
	*nextTokPtr = ptr;
	return XML_TOK_PROLOG_S;
      }
    }
    *nextTokPtr = ptr;
    return XML_TOK_PROLOG_S;
  case BT_PERCNT:
    return PREFIX(scanPercent)(enc, ptr + MINBPC, end, nextTokPtr);
  case BT_COMMA:
    *nextTokPtr = ptr + MINBPC;
    return XML_TOK_COMMA;
  case BT_LSQB:
    *nextTokPtr = ptr + MINBPC;
    return XML_TOK_OPEN_BRACKET;
  case BT_RSQB:
    ptr += MINBPC;
    if (ptr == end)
      return XML_TOK_PARTIAL;
    if (CHAR_MATCHES(enc, ptr, ']')) {
      if (ptr + MINBPC == end)
	return XML_TOK_PARTIAL;
      if (CHAR_MATCHES(enc, ptr + MINBPC, '>')) {
	*nextTokPtr = ptr + 2*MINBPC;
	return XML_TOK_COND_SECT_CLOSE;
      }
    }
    *nextTokPtr = ptr;
    return XML_TOK_CLOSE_BRACKET;
  case BT_LPAR:
    *nextTokPtr = ptr + MINBPC;
    return XML_TOK_OPEN_PAREN;
  case BT_RPAR:
    ptr += MINBPC;
    if (ptr == end)
      return XML_TOK_PARTIAL;
    switch (BYTE_TYPE(enc, ptr)) {
    case BT_AST:
      *nextTokPtr = ptr + MINBPC;
      return XML_TOK_CLOSE_PAREN_ASTERISK;
    case BT_QUEST:
      *nextTokPtr = ptr + MINBPC;
      return XML_TOK_CLOSE_PAREN_QUESTION;
    case BT_PLUS:
      *nextTokPtr = ptr + MINBPC;
      return XML_TOK_CLOSE_PAREN_PLUS;
    case BT_CR: case BT_LF: case BT_S:
    case BT_GT: case BT_COMMA: case BT_VERBAR:
    case BT_RPAR:
      *nextTokPtr = ptr;
      return XML_TOK_CLOSE_PAREN;
    }
    *nextTokPtr = ptr;
    return XML_TOK_INVALID;
  case BT_VERBAR:
    *nextTokPtr = ptr + MINBPC;
    return XML_TOK_OR;
  case BT_GT:
    *nextTokPtr = ptr + MINBPC;
    return XML_TOK_DECL_CLOSE;
  case BT_NUM:
    return PREFIX(scanPoundName)(enc, ptr + MINBPC, end, nextTokPtr);
#define LEAD_CASE(n) \
  case BT_LEAD ## n: \
    if (end - ptr < n) \
      return XML_TOK_PARTIAL_CHAR; \
    if (IS_NMSTRT_CHAR(enc, ptr, n)) { \
      ptr += n; \
      tok = XML_TOK_NAME; \
      break; \
    } \
    if (IS_NAME_CHAR(enc, ptr, n)) { \
      ptr += n; \
      tok = XML_TOK_NMTOKEN; \
      break; \
    } \
    *nextTokPtr = ptr; \
    return XML_TOK_INVALID;
    LEAD_CASE(2) LEAD_CASE(3) LEAD_CASE(4)
#undef LEAD_CASE
  case BT_NMSTRT:
  case BT_HEX:
    tok = XML_TOK_NAME;
    ptr += MINBPC;
    break;
  case BT_DIGIT:
  case BT_NAME:
  case BT_MINUS:
    tok = XML_TOK_NMTOKEN;
    ptr += MINBPC;
    break;
  case BT_NONASCII:
    if (IS_NMSTRT_CHAR_MINBPC(enc, ptr)) {
      ptr += MINBPC;
      tok = XML_TOK_NAME;
      break;
    }
    if (IS_NAME_CHAR_MINBPC(enc, ptr)) {
      ptr += MINBPC;
      tok = XML_TOK_NMTOKEN;
      break;
    }
    /* fall through */
  default:
    *nextTokPtr = ptr;
    return XML_TOK_INVALID;
  }
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
    CHECK_NAME_CASES(enc, ptr, end, nextTokPtr)
    case BT_GT: case BT_RPAR: case BT_COMMA:
    case BT_VERBAR: case BT_LSQB: case BT_PERCNT:
    case BT_S: case BT_CR: case BT_LF:
      *nextTokPtr = ptr;
      return tok;
    case BT_PLUS:
      if (tok != XML_TOK_NAME)  {
	*nextTokPtr = ptr;
	return XML_TOK_INVALID;
      }
      *nextTokPtr = ptr + MINBPC;
      return XML_TOK_NAME_PLUS;
    case BT_AST:
      if (tok != XML_TOK_NAME)  {
	*nextTokPtr = ptr;
	return XML_TOK_INVALID;
      }
      *nextTokPtr = ptr + MINBPC;
      return XML_TOK_NAME_ASTERISK;
    case BT_QUEST:
      if (tok != XML_TOK_NAME)  {
	*nextTokPtr = ptr;
	return XML_TOK_INVALID;
      }
      *nextTokPtr = ptr + MINBPC;
      return XML_TOK_NAME_QUESTION;
    default:
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    }
  }
  return XML_TOK_PARTIAL;
}

static
int PREFIX(attributeValueTok)(const ENCODING *enc, const char *ptr, const char *end,
			      const char **nextTokPtr)
{
  const char *start;
  if (ptr == end)
    return XML_TOK_NONE;
  start = ptr;
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
#define LEAD_CASE(n) \
    case BT_LEAD ## n: ptr += n; break;
    LEAD_CASE(2) LEAD_CASE(3) LEAD_CASE(4)
#undef LEAD_CASE
    case BT_AMP:
      if (ptr == start)
	return PREFIX(scanRef)(enc, ptr + MINBPC, end, nextTokPtr);
      *nextTokPtr = ptr;
      return XML_TOK_DATA_CHARS;
    case BT_LT:
      /* this is for inside entity references */
      *nextTokPtr = ptr;
      return XML_TOK_INVALID;
    case BT_LF:
      if (ptr == start) {
	*nextTokPtr = ptr + MINBPC;
	return XML_TOK_DATA_NEWLINE;
      }
      *nextTokPtr = ptr;
      return XML_TOK_DATA_CHARS;
    case BT_CR:
      if (ptr == start) {
	ptr += MINBPC;
	if (ptr == end)
	  return XML_TOK_TRAILING_CR;
	if (BYTE_TYPE(enc, ptr) == BT_LF)
	  ptr += MINBPC;
	*nextTokPtr = ptr;
	return XML_TOK_DATA_NEWLINE;
      }
      *nextTokPtr = ptr;
      return XML_TOK_DATA_CHARS;
    case BT_S:
      if (ptr == start) {
	*nextTokPtr = ptr + MINBPC;
	return XML_TOK_ATTRIBUTE_VALUE_S;
      }
      *nextTokPtr = ptr;
      return XML_TOK_DATA_CHARS;
    default:
      ptr += MINBPC;
      break;
    }
  }
  *nextTokPtr = ptr;
  return XML_TOK_DATA_CHARS;
}

static
int PREFIX(entityValueTok)(const ENCODING *enc, const char *ptr, const char *end,
			   const char **nextTokPtr)
{
  const char *start;
  if (ptr == end)
    return XML_TOK_NONE;
  start = ptr;
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
#define LEAD_CASE(n) \
    case BT_LEAD ## n: ptr += n; break;
    LEAD_CASE(2) LEAD_CASE(3) LEAD_CASE(4)
#undef LEAD_CASE
    case BT_AMP:
      if (ptr == start)
	return PREFIX(scanRef)(enc, ptr + MINBPC, end, nextTokPtr);
      *nextTokPtr = ptr;
      return XML_TOK_DATA_CHARS;
    case BT_PERCNT:
      if (ptr == start)
	return PREFIX(scanPercent)(enc, ptr + MINBPC, end, nextTokPtr);
      *nextTokPtr = ptr;
      return XML_TOK_DATA_CHARS;
    case BT_LF:
      if (ptr == start) {
	*nextTokPtr = ptr + MINBPC;
	return XML_TOK_DATA_NEWLINE;
      }
      *nextTokPtr = ptr;
      return XML_TOK_DATA_CHARS;
    case BT_CR:
      if (ptr == start) {
	ptr += MINBPC;
	if (ptr == end)
	  return XML_TOK_TRAILING_CR;
	if (BYTE_TYPE(enc, ptr) == BT_LF)
	  ptr += MINBPC;
	*nextTokPtr = ptr;
	return XML_TOK_DATA_NEWLINE;
      }
      *nextTokPtr = ptr;
      return XML_TOK_DATA_CHARS;
    default:
      ptr += MINBPC;
      break;
    }
  }
  *nextTokPtr = ptr;
  return XML_TOK_DATA_CHARS;
}

static
int PREFIX(isPublicId)(const ENCODING *enc, const char *ptr, const char *end,
		       const char **badPtr)
{
  ptr += MINBPC;
  end -= MINBPC;
  for (; ptr != end; ptr += MINBPC) {
    switch (BYTE_TYPE(enc, ptr)) {
    case BT_DIGIT:
    case BT_HEX:
    case BT_MINUS:
    case BT_APOS:
    case BT_LPAR:
    case BT_RPAR:
    case BT_PLUS:
    case BT_COMMA:
    case BT_SOL:
    case BT_EQUALS:
    case BT_QUEST:
    case BT_CR:
    case BT_LF:
    case BT_SEMI:
    case BT_EXCL:
    case BT_AST:
    case BT_PERCNT:
    case BT_NUM:
      break;
    case BT_S:
      if (CHAR_MATCHES(enc, ptr, '\t')) {
	*badPtr = ptr;
	return 0;
      }
      break;
    case BT_NAME:
    case BT_NMSTRT:
      if (!(BYTE_TO_ASCII(enc, ptr) & ~0x7f))
	break;
    default:
      switch (BYTE_TO_ASCII(enc, ptr)) {
      case 0x24: /* $ */
      case 0x40: /* @ */
	break;
      default:
	*badPtr = ptr;
	return 0;
      }
      break;
    }
  }
  return 1;
}

/* This must only be called for a well-formed start-tag or empty element tag.
Returns the number of attributes.  Pointers to the first attsMax attributes 
are stored in atts. */

static
int PREFIX(getAtts)(const ENCODING *enc, const char *ptr,
		    int attsMax, ATTRIBUTE *atts)
{
  enum { other, inName, inValue } state = inName;
  int nAtts = 0;
  int open;

  for (ptr += MINBPC;; ptr += MINBPC) {
    switch (BYTE_TYPE(enc, ptr)) {
#define START_NAME \
      if (state == other) { \
	if (nAtts < attsMax) { \
	  atts[nAtts].name = ptr; \
	  atts[nAtts].normalized = 1; \
	} \
	state = inName; \
      }
#define LEAD_CASE(n) \
    case BT_LEAD ## n: START_NAME ptr += (n - MINBPC); break;
    LEAD_CASE(2) LEAD_CASE(3) LEAD_CASE(4)
#undef LEAD_CASE
    case BT_NONASCII:
    case BT_NMSTRT:
    case BT_HEX:
      START_NAME
      break;
#undef START_NAME
    case BT_QUOT:
      if (state != inValue) {
	atts[nAtts].valuePtr = ptr + MINBPC;
        state = inValue;
        open = BT_QUOT;
      }
      else if (open == BT_QUOT) {
        state = other;
	atts[nAtts++].valueEnd = ptr;
      }
      break;
    case BT_APOS:
      if (state != inValue) {
	atts[nAtts].valuePtr = ptr + MINBPC;
        state = inValue;
        open = BT_APOS;
      }
      else if (open == BT_APOS) {
        state = other;
	atts[nAtts++].valueEnd = ptr;
      }
      break;
    case BT_AMP:
      atts[nAtts].normalized = 0;
      break;
    case BT_S:
      if (state == inName)
        state = other;
      else if (state == inValue
	       && atts[nAtts].normalized
	       && (ptr == atts[nAtts].valuePtr
		   || BYTE_TO_ASCII(enc, ptr) != ' '
		   || BYTE_TO_ASCII(enc, ptr + MINBPC) == ' '
	           || BYTE_TYPE(enc, ptr + MINBPC) == open))
	atts[nAtts].normalized = 0;
      break;
    case BT_CR: case BT_LF:
      /* This case ensures that the first attribute name is counted
         Apart from that we could just change state on the quote. */
      if (state == inName)
        state = other;
      else if (state == inValue)
	atts[nAtts].normalized = 0;
      break;
    case BT_GT:
    case BT_SOL:
      if (state != inValue)
	return nAtts;
      break;
    default:
      break;
    }
  }
  /* not reached */
}

static
int PREFIX(charRefNumber)(const ENCODING *enc, const char *ptr)
{
  int result = 0;
  /* skip &# */
  ptr += 2*MINBPC;
  if (CHAR_MATCHES(enc, ptr, 'x')) {
    for (ptr += MINBPC; !CHAR_MATCHES(enc, ptr, ';'); ptr += MINBPC) {
      int c = BYTE_TO_ASCII(enc, ptr);
      switch (c) {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	result <<= 4;
	result |= (c - '0');
	break;
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
	result <<= 4;
	result += 10 + (c - 'A');
	break;
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	result <<= 4;
	result += 10 + (c - 'a');
	break;
      }
      if (result >= 0x110000)
	return -1;
    }
  }
  else {
    for (; !CHAR_MATCHES(enc, ptr, ';'); ptr += MINBPC) {
      int c = BYTE_TO_ASCII(enc, ptr);
      result *= 10;
      result += (c - '0');
      if (result >= 0x110000)
	return -1;
    }
  }
  return checkCharRefNumber(result);
}

static
int PREFIX(predefinedEntityName)(const ENCODING *enc, const char *ptr, const char *end)
{
  switch (end - ptr) {
  case 2 * MINBPC:
    if (CHAR_MATCHES(enc, ptr + MINBPC, 't')) {
      switch (BYTE_TO_ASCII(enc, ptr)) {
      case 'l':
	return '<';
      case 'g':
	return '>';
      }
    }
    break;
  case 3 * MINBPC:
    if (CHAR_MATCHES(enc, ptr, 'a')) {
      ptr += MINBPC;
      if (CHAR_MATCHES(enc, ptr, 'm')) {
	ptr += MINBPC;
	if (CHAR_MATCHES(enc, ptr, 'p'))
	  return '&';
      }
    }
    break;
  case 4 * MINBPC:
    switch (BYTE_TO_ASCII(enc, ptr)) {
    case 'q':
      ptr += MINBPC;
      if (CHAR_MATCHES(enc, ptr, 'u')) {
	ptr += MINBPC;
	if (CHAR_MATCHES(enc, ptr, 'o')) {
	  ptr += MINBPC;
  	  if (CHAR_MATCHES(enc, ptr, 't'))
	    return '"';
	}
      }
      break;
    case 'a':
      ptr += MINBPC;
      if (CHAR_MATCHES(enc, ptr, 'p')) {
	ptr += MINBPC;
	if (CHAR_MATCHES(enc, ptr, 'o')) {
	  ptr += MINBPC;
  	  if (CHAR_MATCHES(enc, ptr, 's'))
	    return '\'';
	}
      }
      break;
    }
  }
  return 0;
}

static
int PREFIX(sameName)(const ENCODING *enc, const char *ptr1, const char *ptr2)
{
  for (;;) {
    switch (BYTE_TYPE(enc, ptr1)) {
#define LEAD_CASE(n) \
    case BT_LEAD ## n: \
      if (*ptr1++ != *ptr2++) \
	return 0;
    LEAD_CASE(4) LEAD_CASE(3) LEAD_CASE(2)
#undef LEAD_CASE
      /* fall through */
      if (*ptr1++ != *ptr2++)
	return 0;
      break;
    case BT_NONASCII:
    case BT_NMSTRT:
    case BT_HEX:
    case BT_DIGIT:
    case BT_NAME:
    case BT_MINUS:
      if (*ptr2++ != *ptr1++)
	return 0;
#if MINBPC > 1
      if (*ptr2++ != *ptr1++)
	return 0;
#if MINBPC > 2
      if (*ptr2++ != *ptr1++)
	return 0;
#if MINBPC > 3
      if (*ptr2++ != *ptr1++)
	return 0;
#endif
#endif
#endif
      break;
    default:
#if MINBPC == 1
      if (*ptr1 == *ptr2)
	return 1;
#endif
      switch (BYTE_TYPE(enc, ptr2)) {
      case BT_LEAD2:
      case BT_LEAD3:
      case BT_LEAD4:
      case BT_NONASCII:
      case BT_NMSTRT:
      case BT_HEX:
      case BT_DIGIT:
      case BT_NAME:
      case BT_MINUS:
	return 0;
      default:
	return 1;
      }
    }
  }
  /* not reached */
}

static
int PREFIX(nameMatchesAscii)(const ENCODING *enc, const char *ptr1, const char *ptr2)
{
  for (; *ptr2; ptr1 += MINBPC, ptr2++) {
    if (!CHAR_MATCHES(end, ptr1, *ptr2))
      return 0;
  }
  switch (BYTE_TYPE(enc, ptr1)) {
  case BT_LEAD2:
  case BT_LEAD3:
  case BT_LEAD4:
  case BT_NONASCII:
  case BT_NMSTRT:
  case BT_HEX:
  case BT_DIGIT:
  case BT_NAME:
  case BT_MINUS:
    return 0;
  default:
    return 1;
  }
}

static
int PREFIX(nameLength)(const ENCODING *enc, const char *ptr)
{
  const char *start = ptr;
  for (;;) {
    switch (BYTE_TYPE(enc, ptr)) {
#define LEAD_CASE(n) \
    case BT_LEAD ## n: ptr += n; break;
    LEAD_CASE(2) LEAD_CASE(3) LEAD_CASE(4)
#undef LEAD_CASE
    case BT_NONASCII:
    case BT_NMSTRT:
    case BT_HEX:
    case BT_DIGIT:
    case BT_NAME:
    case BT_MINUS:
      ptr += MINBPC;
      break;
    default:
      return ptr - start;
    }
  }
}

static
const char *PREFIX(skipS)(const ENCODING *enc, const char *ptr)
{
  for (;;) {
    switch (BYTE_TYPE(enc, ptr)) {
    case BT_LF:
    case BT_CR:
    case BT_S:
      ptr += MINBPC;
      break;
    default:
      return ptr;
    }
  }
}

static
void PREFIX(updatePosition)(const ENCODING *enc,
			    const char *ptr,
			    const char *end,
			    POSITION *pos)
{
  while (ptr != end) {
    switch (BYTE_TYPE(enc, ptr)) {
#define LEAD_CASE(n) \
    case BT_LEAD ## n: \
      ptr += n; \
      break;
    LEAD_CASE(2) LEAD_CASE(3) LEAD_CASE(4)
#undef LEAD_CASE
    case BT_LF:
      pos->columnNumber = (unsigned)-1;
      pos->lineNumber++;
      ptr += MINBPC;
      break;
    case BT_CR:
      pos->lineNumber++;
      ptr += MINBPC;
      if (ptr != end && BYTE_TYPE(enc, ptr) == BT_LF)
	ptr += MINBPC;
      pos->columnNumber = (unsigned)-1;
      break;
    default:
      ptr += MINBPC;
      break;
    }
    pos->columnNumber++;
  }
}

#undef DO_LEAD_CASE
#undef MULTIBYTE_CASES
#undef INVALID_CASES
#undef CHECK_NAME_CASE
#undef CHECK_NAME_CASES
#undef CHECK_NMSTRT_CASE
#undef CHECK_NMSTRT_CASES
