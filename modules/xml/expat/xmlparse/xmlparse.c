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
#include "xmlparse.h"
#include "xmltok.h"
#include "xmlrole.h"
#include "hashtable.h"

#include <stdlib.h>
#include <string.h>

#define INIT_TAG_BUF_SIZE 32
#define INIT_DATA_BUF_SIZE 1024
#define INIT_ATTS_SIZE 16
#define INIT_BLOCK_SIZE 1024
#define INIT_BUFFER_SIZE 1024

typedef struct tag {
  struct tag *parent;
  const char *rawName;
  int rawNameLength;
  const char *name;
  char *buf;
  char *bufEnd;
} TAG;

typedef struct {
  const char *name;
  const char *textPtr;
  int textLen;
  const char *systemId;
  const char *publicId;
  const char *notation;
  char open;
  char magic;
} ENTITY;

typedef struct block {
  struct block *next;
  int size;
  char s[1];
} BLOCK;

typedef struct {
  BLOCK *blocks;
  BLOCK *freeBlocks;
  const char *end;
  char *ptr;
  char *start;
} STRING_POOL;

/* The byte before the name is a scratch byte used to determine whether
an attribute has been specified. */
typedef struct {
  char *name;
  char maybeTokenized;
} ATTRIBUTE_ID;

typedef struct {
  const ATTRIBUTE_ID *id;
  char isCdata;
  const char *value;
} DEFAULT_ATTRIBUTE;

typedef struct {
  const char *name;
  int nDefaultAtts;
  int allocDefaultAtts;
  DEFAULT_ATTRIBUTE *defaultAtts;
} ELEMENT_TYPE;

typedef struct {
  HASH_TABLE generalEntities;
  HASH_TABLE elementTypes;
  HASH_TABLE attributeIds;
  STRING_POOL pool;
  int complete;
  int standalone;
} DTD;

typedef enum XML_Error Processor(XML_Parser parser,
				 const char *start,
				 const char *end,
				 const char **endPtr);

static Processor prologProcessor;
static Processor contentProcessor;
static Processor epilogProcessor;
static Processor errorProcessor;

static enum XML_Error
doContent(XML_Parser parser, int startTagLevel, const ENCODING *enc,
	  const char *start, const char *end, const char **endPtr);
static enum XML_Error storeAtts(XML_Parser parser, const ENCODING *, const char *tagName, const char *s);
static int
defineAttribute(ELEMENT_TYPE *type, ATTRIBUTE_ID *, int isCdata, const char *dfltValue);
static enum XML_Error
storeAttributeValue(XML_Parser parser, const ENCODING *, int isCdata, const char *, const char *,
		    STRING_POOL *);
static enum XML_Error
appendAttributeValue(XML_Parser parser, const ENCODING *, int isCdata, const char *, const char *,
		    STRING_POOL *);
static ATTRIBUTE_ID *
getAttributeId(XML_Parser parser, const ENCODING *enc, const char *start, const char *end);
static enum XML_Error
storeEntityValue(XML_Parser parser, const char *start, const char *end);
static int
reportProcessingInstruction(XML_Parser parser, const ENCODING *enc, const char *start, const char *end);

static int dtdInit(DTD *);
static void dtdDestroy(DTD *);
static void poolInit(STRING_POOL *);
static void poolClear(STRING_POOL *);
static void poolDestroy(STRING_POOL *);
static char *poolAppend(STRING_POOL *pool, const ENCODING *enc,
			const char *ptr, const char *end);
static char *poolStoreString(STRING_POOL *pool, const ENCODING *enc,
			     const char *ptr, const char *end);
static int poolGrow(STRING_POOL *pool);

#define poolStart(pool) ((pool)->start)
#define poolEnd(pool) ((pool)->ptr)
#define poolLength(pool) ((pool)->ptr - (pool)->start)
#define poolChop(pool) ((void)--(pool->ptr))
#define poolLastByte(pool) (((pool)->ptr)[-1])
#define poolDiscard(pool) ((pool)->ptr = (pool)->start)
#define poolFinish(pool) ((pool)->start = (pool)->ptr)
#define poolAppendByte(pool, c) \
  (((pool)->ptr == (pool)->end && !poolGrow(pool)) \
   ? 0 \
   : ((*((pool)->ptr)++ = c), 1))

typedef struct {
  char *buffer;
  /* first character to be parsed */
  const char *bufferPtr;
  /* past last character to be parsed */
  char *bufferEnd;
  /* allocated end of buffer */
  const char *bufferLim;
  long bufferEndByteIndex;
  char *dataBuf;
  char *dataBufEnd;
  void *userData;
  XML_StartElementHandler startElementHandler;
  XML_EndElementHandler endElementHandler;
  XML_CharacterDataHandler characterDataHandler;
  XML_ProcessingInstructionHandler processingInstructionHandler;
  const ENCODING *encoding;
  INIT_ENCODING initEncoding;
  PROLOG_STATE prologState;
  Processor *processor;
  enum XML_Error errorCode;
  const char *errorPtr;
  int tagLevel;
  ENTITY *declEntity;
  ELEMENT_TYPE *declElementType;
  ATTRIBUTE_ID *declAttributeId;
  char declAttributeIsCdata;
  DTD dtd;
  TAG *tagStack;
  TAG *freeTagList;
  int attsSize;
  ATTRIBUTE *atts;
  POSITION position;
  long errorByteIndex;
  STRING_POOL tempPool;
  STRING_POOL temp2Pool;
  char *groupConnector;
  unsigned groupSize;
  int hadExternalDoctype;
} Parser;

#define userData (((Parser *)parser)->userData)
#define startElementHandler (((Parser *)parser)->startElementHandler)
#define endElementHandler (((Parser *)parser)->endElementHandler)
#define characterDataHandler (((Parser *)parser)->characterDataHandler)
#define processingInstructionHandler (((Parser *)parser)->processingInstructionHandler)
#define encoding (((Parser *)parser)->encoding)
#define initEncoding (((Parser *)parser)->initEncoding)
#define prologState (((Parser *)parser)->prologState)
#define processor (((Parser *)parser)->processor)
#define errorCode (((Parser *)parser)->errorCode)
#define errorPtr (((Parser *)parser)->errorPtr)
#define errorByteIndex (((Parser *)parser)->errorByteIndex)
#define position (((Parser *)parser)->position)
#define tagLevel (((Parser *)parser)->tagLevel)
#define buffer (((Parser *)parser)->buffer)
#define bufferPtr (((Parser *)parser)->bufferPtr)
#define bufferEnd (((Parser *)parser)->bufferEnd)
#define bufferEndByteIndex (((Parser *)parser)->bufferEndByteIndex)
#define bufferLim (((Parser *)parser)->bufferLim)
#define dataBuf (((Parser *)parser)->dataBuf)
#define dataBufEnd (((Parser *)parser)->dataBufEnd)
#define dtd (((Parser *)parser)->dtd)
#define declEntity (((Parser *)parser)->declEntity)
#define declElementType (((Parser *)parser)->declElementType)
#define declAttributeId (((Parser *)parser)->declAttributeId)
#define declAttributeIsCdata (((Parser *)parser)->declAttributeIsCdata)
#define freeTagList (((Parser *)parser)->freeTagList)
#define tagStack (((Parser *)parser)->tagStack)
#define atts (((Parser *)parser)->atts)
#define attsSize (((Parser *)parser)->attsSize)
#define tempPool (((Parser *)parser)->tempPool)
#define temp2Pool (((Parser *)parser)->temp2Pool)
#define groupConnector (((Parser *)parser)->groupConnector)
#define groupSize (((Parser *)parser)->groupSize)
#define hadExternalDoctype (((Parser *)parser)->hadExternalDoctype)

XML_Parser XML_ParserCreate(const char *encodingName)
{
  XML_Parser parser = malloc(sizeof(Parser));
  if (!parser)
    return parser;
  processor = prologProcessor;
  XmlPrologStateInit(&prologState);
  userData = 0;
  startElementHandler = 0;
  endElementHandler = 0;
  characterDataHandler = 0;
  processingInstructionHandler = 0;
  buffer = 0;
  bufferPtr = 0;
  bufferEnd = 0;
  bufferEndByteIndex = 0;
  bufferLim = 0;
  declElementType = 0;
  declAttributeId = 0;
  declEntity = 0;
  memset(&position, 0, sizeof(POSITION));
  errorCode = XML_ERROR_NONE;
  errorByteIndex = 0;
  errorPtr = 0;
  tagLevel = 0;
  tagStack = 0;
  freeTagList = 0;
  attsSize = INIT_ATTS_SIZE;
  atts = malloc(attsSize * sizeof(ATTRIBUTE));
  dataBuf = malloc(INIT_DATA_BUF_SIZE);
  groupSize = 0;
  groupConnector = 0;
  hadExternalDoctype = 0;
  poolInit(&tempPool);
  poolInit(&temp2Pool);
  if (!dtdInit(&dtd) || !atts || !dataBuf) {
    XML_ParserFree(parser);
    return 0;
  }
  dataBufEnd = dataBuf + INIT_DATA_BUF_SIZE;
  if (!XmlInitEncoding(&initEncoding, &encoding, encodingName)) {
    errorCode = XML_ERROR_UNKNOWN_ENCODING;
    processor = errorProcessor;
  }
  return parser;
}

void XML_ParserFree(XML_Parser parser)
{
  for (;;) {
    TAG *p;
    if (tagStack == 0) {
      if (freeTagList == 0)
	break;
      tagStack = freeTagList;
      freeTagList = 0;
    }
    p = tagStack;
    tagStack = tagStack->parent;
    free(p->buf);
    free(p);
  }
  poolDestroy(&tempPool);
  poolDestroy(&temp2Pool);
  dtdDestroy(&dtd);
  free((void *)atts);
  free(groupConnector);
  free(buffer);
  free(dataBuf);
  free(parser);
}

void XML_SetUserData(XML_Parser parser, void *p)
{
  userData = p;
}

void XML_SetElementHandler(XML_Parser parser,
			   XML_StartElementHandler start,
			   XML_EndElementHandler end)
{
  startElementHandler = start;
  endElementHandler = end;
}

void XML_SetCharacterDataHandler(XML_Parser parser,
				 XML_CharacterDataHandler handler)
{
  characterDataHandler = handler;
}

void XML_SetProcessingInstructionHandler(XML_Parser parser,
					 XML_ProcessingInstructionHandler handler)
{
  processingInstructionHandler = handler;
}

int XML_Parse(XML_Parser parser, const char *s, int len, int isFinal)
{
  bufferEndByteIndex += len;
  if (len == 0) {
    if (!isFinal)
      return 1;
    errorCode = processor(parser, bufferPtr, bufferEnd, 0);
    return errorCode == XML_ERROR_NONE;
  }
  else if (bufferPtr == bufferEnd) {
    const char *end;
    int nLeftOver;
    if (isFinal) {
      errorCode = processor(parser, s, s + len, 0);
      if (errorCode == XML_ERROR_NONE)
	return 1;
      if (errorPtr) {
	errorByteIndex = bufferEndByteIndex - (s + len - errorPtr);
	XmlUpdatePosition(encoding, s, errorPtr, &position);
      }
      return 0;
    }
    errorCode = processor(parser, s, s + len, &end);
    if (errorCode != XML_ERROR_NONE) {
      if (errorPtr) {
	errorByteIndex = bufferEndByteIndex - (s + len - errorPtr);
	XmlUpdatePosition(encoding, s, errorPtr, &position);
      }
      return 0;
    }
    XmlUpdatePosition(encoding, s, end, &position);
    nLeftOver = s + len - end;
    if (nLeftOver) {
      if (buffer == 0 || nLeftOver > bufferLim - buffer) {
	/* FIXME avoid integer overflow */
	buffer = realloc(buffer, len * 2);
	if (!buffer) {
	  errorCode = XML_ERROR_NO_MEMORY;
	  return 0;
	}
	bufferLim = buffer + len * 2;
      }
      memcpy(buffer, end, nLeftOver);
      bufferPtr = buffer;
      bufferEnd = buffer + nLeftOver;
    }
    return 1;
  }
  else {
    memcpy(XML_GetBuffer(parser, len), s, len);
    return XML_ParseBuffer(parser, len, isFinal);
  }
}

int XML_ParseBuffer(XML_Parser parser, int len, int isFinal)
{
  const char *start = bufferPtr;
  bufferEnd += len;
  errorCode = processor(parser, bufferPtr, bufferEnd,
			isFinal ? (const char **)0 : &bufferPtr);
  if (errorCode == XML_ERROR_NONE) {
    if (!isFinal)
      XmlUpdatePosition(encoding, start, bufferPtr, &position);
    return 1;
  }
  else {
    if (errorPtr) {
      errorByteIndex = bufferEndByteIndex - (bufferEnd - errorPtr);
      XmlUpdatePosition(encoding, start, errorPtr, &position);
    }
    return 0;
  }
}

void *XML_GetBuffer(XML_Parser parser, int len)
{
  if (len > bufferLim - bufferEnd) {
    /* FIXME avoid integer overflow */
    int neededSize = len + (bufferEnd - bufferPtr);
    if (neededSize  <= bufferLim - buffer) {
      memmove(buffer, bufferPtr, bufferEnd - bufferPtr);
      bufferEnd = buffer + (bufferEnd - bufferPtr);
      bufferPtr = buffer;
    }
    else {
      char *newBuf;
      int bufferSize = bufferLim - bufferPtr;
      if (bufferSize == 0)
	bufferSize = INIT_BUFFER_SIZE;
      do {
	bufferSize *= 2;
      } while (bufferSize < neededSize);
      newBuf = malloc(bufferSize);
      if (newBuf == 0) {
	errorCode = XML_ERROR_NO_MEMORY;
	return 0;
      }
      bufferLim = newBuf + bufferSize;
      if (bufferPtr) {
	memcpy(newBuf, bufferPtr, bufferEnd - bufferPtr);
	free(buffer);
      }
      bufferEnd = newBuf + (bufferEnd - bufferPtr);
      bufferPtr = buffer = newBuf;
    }
  }
  return bufferEnd;
}

int XML_GetErrorCode(XML_Parser parser)
{
  return errorCode;
}

int XML_GetErrorLineNumber(XML_Parser parser)
{
  return position.lineNumber + 1;
}

int XML_GetErrorColumnNumber(XML_Parser parser)
{
  return position.columnNumber;
}

long XML_GetErrorByteIndex(XML_Parser parser)
{
  return errorByteIndex;
}

const char *XML_ErrorString(int code)
{
  static const char *message[] = {
    0,
    "out of memory",
    "syntax error",
    "no element found",
    "not well-formed",
    "unclosed token",
    "unclosed token",
    "mismatched tag",
    "duplicate attribute",
    "junk after document element",
    "parameter entity reference not allowed within declaration in internal subset",
    "undefined entity",
    "recursive entity reference",
    "asynchronous entity",
    "reference to invalid character number",
    "reference to binary entity",
    "reference to external entity in attribute",
    "xml processing instruction not at start of external entity",
    "unknown encoding",
    "encoding specified in XML declaration is incorrect"
  };
  if (code > 0 && code < sizeof(message)/sizeof(message[0]))
    return message[code];
  return 0;
}

static
enum XML_Error contentProcessor(XML_Parser parser,
				const char *start,
				const char *end,
				const char **endPtr)
{
  return doContent(parser, 0, encoding, start, end, endPtr);
}

static enum XML_Error
doContent(XML_Parser parser,
	  int startTagLevel,
	  const ENCODING *enc,
	  const char *s,
	  const char *end,
	  const char **nextPtr)
{
  const ENCODING *utf8 = XmlGetInternalEncoding(XML_UTF8_ENCODING);
  for (;;) {
    const char *next;
    int tok = XmlContentTok(enc, s, end, &next);
    switch (tok) {
    case XML_TOK_TRAILING_CR:
    case XML_TOK_NONE:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      if (startTagLevel > 0) {
	if (tagLevel != startTagLevel) {
	  errorPtr = s;
	  return XML_ERROR_ASYNC_ENTITY;
        }
	return XML_ERROR_NONE;
      }
      errorPtr = s;
      return XML_ERROR_NO_ELEMENTS;
    case XML_TOK_INVALID:
      errorPtr = next;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_PARTIAL:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      errorPtr = s;
      return XML_ERROR_UNCLOSED_TOKEN;
    case XML_TOK_PARTIAL_CHAR:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      errorPtr = s;
      return XML_ERROR_PARTIAL_CHAR;
    case XML_TOK_ENTITY_REF:
      {
	const char *name = poolStoreString(&dtd.pool, enc,
					   s + enc->minBytesPerChar,
					   next - enc->minBytesPerChar);
	ENTITY *entity;
	if (!name)
	  return XML_ERROR_NO_MEMORY;
	entity = (ENTITY *)lookup(&dtd.generalEntities, name, 0);
	poolDiscard(&dtd.pool);
	if (!entity) {
	  if (dtd.complete || dtd.standalone) {
	    errorPtr = s;
	    return XML_ERROR_UNDEFINED_ENTITY;
	  }
	  break;
	}
	if (entity->magic) {
	  if (characterDataHandler)
	    characterDataHandler(userData, entity->textPtr, entity->textLen);
	  break;
	}
	if (entity->open) {
	  errorPtr = s;
	  return XML_ERROR_RECURSIVE_ENTITY_REF;
	}
	if (entity->notation) {
	  errorPtr = s;
	  return XML_ERROR_BINARY_ENTITY_REF;
	}
	if (entity) {
	  if (entity->textPtr) {
	    enum XML_Error result;
	    entity->open = 1;
	    result = doContent(parser,
			       tagLevel,
			       utf8,
			       entity->textPtr,
			       entity->textPtr + entity->textLen,
			       0);
	    entity->open = 0;
	    if (result) {
	      errorPtr = s;
	      return result;
	    }
	  }
	}
	break;
      }
    case XML_TOK_START_TAG_WITH_ATTS:
      if (!startElementHandler) {
	enum XML_Error result = storeAtts(parser, enc, 0, s);
	if (result)
	  return result;
      }
      /* fall through */
    case XML_TOK_START_TAG_NO_ATTS:
      {
	TAG *tag;
	if (freeTagList) {
	  tag = freeTagList;
	  freeTagList = freeTagList->parent;
	}
	else {
	  tag = malloc(sizeof(TAG));
	  if (!tag)
	    return XML_ERROR_NO_MEMORY;
	  tag->buf = malloc(INIT_TAG_BUF_SIZE);
	  if (!tag->buf)
	    return XML_ERROR_NO_MEMORY;
	  tag->bufEnd = tag->buf + INIT_TAG_BUF_SIZE;
	}
	tag->parent = tagStack;
	tagStack = tag;
	tag->rawName = s + enc->minBytesPerChar;
	tag->rawNameLength = XmlNameLength(enc, tag->rawName);
	if (nextPtr) {
	  if (tag->rawNameLength > tag->bufEnd - tag->buf) {
	    int bufSize = tag->rawNameLength * 4;
	    tag->buf = realloc(tag->buf, bufSize);
	    if (!tag->buf)
	      return XML_ERROR_NO_MEMORY;
	    tag->bufEnd = tag->buf + bufSize;
	  }
	  memcpy(tag->buf, tag->rawName, tag->rawNameLength);
	  tag->rawName = tag->buf;
	}
	++tagLevel;
	if (startElementHandler) {
	  enum XML_Error result;
	  char *toPtr;
	  const char *rawNameEnd = tag->rawName + tag->rawNameLength;
	  for (;;) {
	    const char *fromPtr = tag->rawName;
	    int bufSize;
	    toPtr = tag->buf;
	    if (nextPtr)
	      toPtr += tag->rawNameLength;
	    tag->name = toPtr;
	    XmlConvert(enc, XML_UTF8_ENCODING,
		       &fromPtr, rawNameEnd,
	               &toPtr, tag->bufEnd - 1);
	    if (fromPtr == rawNameEnd)
	      break;
	    bufSize = (tag->bufEnd - tag->buf) << 1;
	    tag->buf = realloc(tag->buf, bufSize);
	    if (!tag->buf)
	      return XML_ERROR_NO_MEMORY;
	    tag->bufEnd = tag->buf + bufSize;
	  }
	  *toPtr = 0;
	  result = storeAtts(parser, enc, tag->name, s);
	  if (result)
	    return result;
	  startElementHandler(userData, tag->name, (const char **)atts);
	  poolClear(&tempPool);
	}
	else
	  tag->name = 0;
	break;
      }
    case XML_TOK_EMPTY_ELEMENT_WITH_ATTS:
      if (!startElementHandler) {
	enum XML_Error result = storeAtts(parser, enc, 0, s);
	if (result)
	  return result;
      }
      /* fall through */
    case XML_TOK_EMPTY_ELEMENT_NO_ATTS:
      if (startElementHandler || endElementHandler) {
	const char *rawName = s + enc->minBytesPerChar;
	const char *name = poolStoreString(&tempPool, enc, rawName,
					   rawName
					   + XmlNameLength(enc, rawName));
	if (!name)
	  return XML_ERROR_NO_MEMORY;
	poolFinish(&tempPool);
	if (startElementHandler) {
	  enum XML_Error result = storeAtts(parser, enc, name, s);
	  if (result)
	    return result;
	  startElementHandler(userData, name, (const char **)atts);
	}
	if (endElementHandler)
	  endElementHandler(userData, name);
	poolClear(&tempPool);
      }
      if (tagLevel == 0)
	return epilogProcessor(parser, next, end, nextPtr);
      break;
    case XML_TOK_END_TAG:
      if (tagLevel == startTagLevel) {
        errorPtr = s;
        return XML_ERROR_ASYNC_ENTITY;
      }
      else {
	int len;
	const char *rawName;
	TAG *tag = tagStack;
	tagStack = tag->parent;
	tag->parent = freeTagList;
	freeTagList = tag;
	rawName = s + enc->minBytesPerChar*2;
	len = XmlNameLength(enc, rawName);
	if (len != tag->rawNameLength
	    || memcmp(tag->rawName, rawName, len) != 0) {
	  errorPtr = rawName;
	  return XML_ERROR_TAG_MISMATCH;
	}
	--tagLevel;
	if (endElementHandler) {
	  if (tag->name)
	    endElementHandler(userData, tag->name);
	  else {
	    const char *name = poolStoreString(&tempPool, enc, rawName,
	                                       rawName + len);
	    if (!name)
	    return XML_ERROR_NO_MEMORY;
	    endElementHandler(userData, name);
	    poolClear(&tempPool);
	  }
	}
	if (tagLevel == 0)
	  return epilogProcessor(parser, next, end, nextPtr);
      }
      break;
    case XML_TOK_CHAR_REF:
      {
	int n = XmlCharRefNumber(enc, s);
	if (n < 0) {
	  errorPtr = s;
	  return XML_ERROR_BAD_CHAR_REF;
	}
	if (characterDataHandler) {
	  char buf[XML_MAX_BYTES_PER_CHAR];
	  characterDataHandler(userData, buf, XmlEncode(utf8, n, buf));
	}
      }
      break;
    case XML_TOK_XML_DECL:
      errorPtr = s;
      return XML_ERROR_MISPLACED_XML_PI;
    case XML_TOK_DATA_NEWLINE:
      if (characterDataHandler) {
	char c = '\n';
	characterDataHandler(userData, &c, 1);
      }
      break;
    case XML_TOK_CDATA_SECTION:
      if (characterDataHandler) {
	const char *lim = next - enc->minBytesPerChar * 3;
	s += enc->minBytesPerChar * 9;
	do {
	  char *dataPtr = dataBuf;
	  XmlConvert(enc, XML_UTF8_ENCODING, &s, lim, &dataPtr, dataBufEnd);
	  characterDataHandler(userData, dataBuf, dataPtr - dataBuf);
	} while (s != lim);
      }
      break;
    case XML_TOK_DATA_CHARS:
      if (characterDataHandler) {
	do {
	  char *dataPtr = dataBuf;
	  XmlConvert(enc, XML_UTF8_ENCODING, &s, next, &dataPtr, dataBufEnd);
	  characterDataHandler(userData, dataBuf, dataPtr - dataBuf);
	} while (s != next);
      }
      break;
    case XML_TOK_PI:
      if (!reportProcessingInstruction(parser, enc, s, next))
	return XML_ERROR_NO_MEMORY;
      break;
    }
    s = next;
  }
  /* not reached */
}

/* If tagName is non-null, build a real list of attributes,
otherwise just check the attributes for well-formedness. */

static enum XML_Error storeAtts(XML_Parser parser, const ENCODING *enc,
				const char *tagName, const char *s)
{
  ELEMENT_TYPE *elementType = 0;
  int nDefaultAtts = 0;
  const char **appAtts = (const char **)atts;
  int i;
  int n;

  if (tagName) {
    elementType = (ELEMENT_TYPE *)lookup(&dtd.elementTypes, tagName, 0);
    if (elementType)
      nDefaultAtts = elementType->nDefaultAtts;
  }
  
  n = XmlGetAttributes(enc, s, attsSize, atts);
  if (n + nDefaultAtts > attsSize) {
    attsSize = 2*n;
    atts = realloc((void *)atts, attsSize * sizeof(ATTRIBUTE));
    if (!atts)
      return XML_ERROR_NO_MEMORY;
    if (n > attsSize)
      XmlGetAttributes(enc, s, n, atts);
  }
  for (i = 0; i < n; i++) {
    ATTRIBUTE_ID *attId = getAttributeId(parser, enc, atts[i].name,
					  atts[i].name
					  + XmlNameLength(enc, atts[i].name));
    if (!attId)
      return XML_ERROR_NO_MEMORY;
    if ((attId->name)[-1]) {
      errorPtr = atts[i].name;
      return XML_ERROR_DUPLICATE_ATTRIBUTE;
    }
    (attId->name)[-1] = 1;
    appAtts[i << 1] = attId->name;
    if (!atts[i].normalized) {
      enum XML_Error result;
      int isCdata = 1;

      if (attId->maybeTokenized) {
	int j;
	for (j = 0; j < nDefaultAtts; j++) {
	  if (attId == elementType->defaultAtts[j].id) {
	    isCdata = elementType->defaultAtts[j].isCdata;
	    break;
	  }
	}
      }

      result = storeAttributeValue(parser, enc, isCdata,
				   atts[i].valuePtr, atts[i].valueEnd,
			           &tempPool);
      if (result)
	return result;
      if (tagName) {
	appAtts[(i << 1) + 1] = poolStart(&tempPool);
	poolFinish(&tempPool);
      }
      else
	poolDiscard(&tempPool);
    }
    else if (tagName) {
      appAtts[(i << 1) + 1] = poolStoreString(&tempPool, enc, atts[i].valuePtr, atts[i].valueEnd);
      if (appAtts[(i << 1) + 1] == 0)
	return XML_ERROR_NO_MEMORY;
      poolFinish(&tempPool);
    }
  }
  if (tagName) {
    int j;
    for (j = 0; j < nDefaultAtts; j++) {
      const DEFAULT_ATTRIBUTE *da = elementType->defaultAtts + j;
      if (!(da->id->name)[-1] && da->value) {
	(da->id->name)[-1] = 1;
	appAtts[i << 1] = da->id->name;
	appAtts[(i << 1) + 1] = da->value;
	i++;
      }
    }
    appAtts[i << 1] = 0;
  }
  while (i-- > 0)
    ((char *)appAtts[i << 1])[-1] = 0;
  return XML_ERROR_NONE;
}

static enum XML_Error
prologProcessor(XML_Parser parser,
		const char *s,
		const char *end,
		const char **nextPtr)
{
  for (;;) {
    const char *next;
    int tok = XmlPrologTok(encoding, s, end, &next);
    if (tok <= 0) {
      if (nextPtr != 0 && tok != XML_TOK_INVALID) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      switch (tok) {
      case XML_TOK_INVALID:
	errorPtr = next;
	return XML_ERROR_INVALID_TOKEN;
      case XML_TOK_NONE:
	return XML_ERROR_NO_ELEMENTS;
      case XML_TOK_PARTIAL:
	return XML_ERROR_UNCLOSED_TOKEN;
      case XML_TOK_PARTIAL_CHAR:
	return XML_ERROR_PARTIAL_CHAR;
      case XML_TOK_TRAILING_CR:
	errorPtr = s + encoding->minBytesPerChar;
	return XML_ERROR_NO_ELEMENTS;
      default:
	abort();
      }
    }
    switch (XmlTokenRole(&prologState, tok, s, next, encoding)) {
    case XML_ROLE_XML_DECL:
      {
	const char *encodingName = 0;
	const ENCODING *newEncoding = 0;
	const char *version;
	int standalone = -1;
	if (!XmlParseXmlDecl(0,
			     encoding,
			     s,
			     next,
			     &errorPtr,
			     &version,
			     &encodingName,
			     &newEncoding,
			     &standalone))
	  return XML_ERROR_SYNTAX;
	if (newEncoding) {
	  if (newEncoding->minBytesPerChar != encoding->minBytesPerChar) {
	    errorPtr = encodingName;
	    return XML_ERROR_INCORRECT_ENCODING;
	  }
	  encoding = newEncoding;
	}
	else if (encodingName) {
	  errorPtr = encodingName;
	  return XML_ERROR_UNKNOWN_ENCODING;
	}
	if (standalone == 1)
	  dtd.standalone = 1;
	break;
      }
    case XML_ROLE_DOCTYPE_SYSTEM_ID:
      hadExternalDoctype = 1;
      break;
    case XML_ROLE_DOCTYPE_PUBLIC_ID:
    case XML_ROLE_ENTITY_PUBLIC_ID:
    case XML_ROLE_NOTATION_PUBLIC_ID:
      if (!XmlIsPublicId(encoding, s, next, &errorPtr))
	return XML_ERROR_SYNTAX;
      break;
    case XML_ROLE_INSTANCE_START:
      processor = contentProcessor;
      if (hadExternalDoctype)
	dtd.complete = 0;
      return contentProcessor(parser, s, end, nextPtr);
    case XML_ROLE_ATTLIST_ELEMENT_NAME:
      {
	const char *name = poolStoreString(&dtd.pool, encoding, s, next);
	if (!name)
	  return XML_ERROR_NO_MEMORY;
	declElementType = (ELEMENT_TYPE *)lookup(&dtd.elementTypes, name, sizeof(ELEMENT_TYPE));
	if (!declElementType)
	  return XML_ERROR_NO_MEMORY;
	if (declElementType->name != name)
	  poolDiscard(&dtd.pool);
	else
	  poolFinish(&dtd.pool);
	break;
      }
    case XML_ROLE_ATTRIBUTE_NAME:
      declAttributeId = getAttributeId(parser, encoding, s, next);
      if (!declAttributeId)
	return XML_ERROR_NO_MEMORY;
      declAttributeIsCdata = 0;
      break;
    case XML_ROLE_ATTRIBUTE_TYPE_CDATA:
      declAttributeIsCdata = 1;
      break;
    case XML_ROLE_IMPLIED_ATTRIBUTE_VALUE:
    case XML_ROLE_REQUIRED_ATTRIBUTE_VALUE:
      if (dtd.complete
	  && !defineAttribute(declElementType, declAttributeId, declAttributeIsCdata, 0))
	return XML_ERROR_NO_MEMORY;
      break;
    case XML_ROLE_DEFAULT_ATTRIBUTE_VALUE:
    case XML_ROLE_FIXED_ATTRIBUTE_VALUE:
      {
	const char *attVal;
	enum XML_Error result
	  = storeAttributeValue(parser, encoding, declAttributeIsCdata,
				s + encoding->minBytesPerChar,
			        next - encoding->minBytesPerChar,
			        &dtd.pool);
	if (result)
	  return result;
	attVal = poolStart(&dtd.pool);
	poolFinish(&dtd.pool);
	if (dtd.complete
	    && !defineAttribute(declElementType, declAttributeId, declAttributeIsCdata, attVal))
	  return XML_ERROR_NO_MEMORY;
	break;
      }
    case XML_ROLE_ENTITY_VALUE:
      {
	enum XML_Error result = storeEntityValue(parser, s, next);
	if (result != XML_ERROR_NONE)
	  return result;
      }
      break;
    case XML_ROLE_ENTITY_SYSTEM_ID:
      if (declEntity) {
	declEntity->systemId = poolStoreString(&dtd.pool, encoding,
	                                       s + encoding->minBytesPerChar,
	  				       next - encoding->minBytesPerChar);
	if (!declEntity->systemId)
	  return XML_ERROR_NO_MEMORY;
	poolFinish(&dtd.pool);
      }
      break;
    case XML_ROLE_ENTITY_NOTATION_NAME:
      if (declEntity) {
	declEntity->notation = poolStoreString(&dtd.pool, encoding, s, next);
	if (!declEntity->notation)
	  return XML_ERROR_NO_MEMORY;
	poolFinish(&dtd.pool);
      }
      break;
    case XML_ROLE_GENERAL_ENTITY_NAME:
      {
	const char *name = poolStoreString(&dtd.pool, encoding, s, next);
	if (!name)
	  return XML_ERROR_NO_MEMORY;
	if (dtd.complete) {
	  declEntity = (ENTITY *)lookup(&dtd.generalEntities, name, sizeof(ENTITY));
	  if (!declEntity)
	    return XML_ERROR_NO_MEMORY;
	  if (declEntity->name != name) {
	    poolDiscard(&dtd.pool);
	    declEntity = 0;
	  }
	  else
	    poolFinish(&dtd.pool);
	}
	else {
	  poolDiscard(&dtd.pool);
	  declEntity = 0;
	}
      }
      break;
    case XML_ROLE_PARAM_ENTITY_NAME:
      declEntity = 0;
      break;
    case XML_ROLE_ERROR:
      errorPtr = s;
      switch (tok) {
      case XML_TOK_PARAM_ENTITY_REF:
	return XML_ERROR_PARAM_ENTITY_REF;
      case XML_TOK_XML_DECL:
	return XML_ERROR_MISPLACED_XML_PI;
      default:
	return XML_ERROR_SYNTAX;
      }
    case XML_ROLE_GROUP_OPEN:
      if (prologState.level >= groupSize) {
	if (groupSize)
	  groupConnector = realloc(groupConnector, groupSize *= 2);
	else
	  groupConnector = malloc(groupSize = 32);
	if (!groupConnector)
	  return XML_ERROR_NO_MEMORY;
      }
      groupConnector[prologState.level] = 0;
      break;
    case XML_ROLE_GROUP_SEQUENCE:
      if (groupConnector[prologState.level] == '|') {
	errorPtr = s;
	return XML_ERROR_SYNTAX;
      }
      groupConnector[prologState.level] = ',';
      break;
    case XML_ROLE_GROUP_CHOICE:
      if (groupConnector[prologState.level] == ',') {
	errorPtr = s;
	return XML_ERROR_SYNTAX;
      }
      groupConnector[prologState.level] = '|';
      break;
    case XML_ROLE_PARAM_ENTITY_REF:
      dtd.complete = 0;
      break;
    case XML_ROLE_NONE:
      switch (tok) {
      case XML_TOK_PI:
	if (!reportProcessingInstruction(parser, encoding, s, next))
	  return XML_ERROR_NO_MEMORY;
	break;
      }
      break;
    }
    s = next;
  }
  /* not reached */
}

static
enum XML_Error epilogProcessor(XML_Parser parser,
			       const char *s,
			       const char *end,
			       const char **nextPtr)
{
  processor = epilogProcessor;
  for (;;) {
    const char *next;
    int tok = XmlPrologTok(encoding, s, end, &next);
    switch (tok) {
    case XML_TOK_TRAILING_CR:
    case XML_TOK_NONE:
      if (nextPtr)
	*nextPtr = end;
      return XML_ERROR_NONE;
    case XML_TOK_PROLOG_S:
    case XML_TOK_COMMENT:
      break;
    case XML_TOK_PI:
      if (!reportProcessingInstruction(parser, encoding, s, next))
	return XML_ERROR_NO_MEMORY;
      break;
    case XML_TOK_INVALID:
      errorPtr = next;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_PARTIAL:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      errorPtr = s;
      return XML_ERROR_UNCLOSED_TOKEN;
    case XML_TOK_PARTIAL_CHAR:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      errorPtr = s;
      return XML_ERROR_PARTIAL_CHAR;
    default:
      errorPtr = s;
      return XML_ERROR_JUNK_AFTER_DOC_ELEMENT;
    }
    s = next;
  }
}

static
enum XML_Error errorProcessor(XML_Parser parser,
			      const char *s,
			      const char *end,
			      const char **nextPtr)
{
  return errorCode;
}

static enum XML_Error
storeAttributeValue(XML_Parser parser, const ENCODING *enc, int isCdata,
		    const char *ptr, const char *end,
		    STRING_POOL *pool)
{
  enum XML_Error result = appendAttributeValue(parser, enc, isCdata, ptr, end, pool);
  if (result)
    return result;
  if (!isCdata && poolLength(pool) && poolLastByte(pool) == ' ')
    poolChop(pool);
  if (!poolAppendByte(pool, 0))
    return XML_ERROR_NO_MEMORY;
  return XML_ERROR_NONE;
}

static enum XML_Error
appendAttributeValue(XML_Parser parser, const ENCODING *enc, int isCdata,
		     const char *ptr, const char *end,
		     STRING_POOL *pool)
{
  const ENCODING *utf8 = XmlGetInternalEncoding(XML_UTF8_ENCODING);
  for (;;) {
    const char *next;
    int tok = XmlAttributeValueTok(enc, ptr, end, &next);
    switch (tok) {
    case XML_TOK_NONE:
      return XML_ERROR_NONE;
    case XML_TOK_INVALID:
      errorPtr = next;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_PARTIAL:
      errorPtr = ptr;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_CHAR_REF:
      {
	char buf[XML_MAX_BYTES_PER_CHAR];
	int i;
	int n = XmlCharRefNumber(enc, ptr);
	if (n < 0) {
	  errorPtr = ptr;
      	  return XML_ERROR_BAD_CHAR_REF;
	}
	if (!isCdata
	    && n == ' '
	    && (poolLength(pool) == 0 || poolLastByte(pool) == ' '))
	  break;
	n = XmlEncode(utf8, n, buf);
	if (!n) {
	  errorPtr = ptr;
	  return XML_ERROR_BAD_CHAR_REF;
	}
	for (i = 0; i < n; i++) {
	  if (!poolAppendByte(pool, buf[i]))
	    return XML_ERROR_NO_MEMORY;
	}
      }
      break;
    case XML_TOK_DATA_CHARS:
      if (!poolAppend(pool, enc, ptr, next))
	return XML_ERROR_NO_MEMORY;
      break;
      break;
    case XML_TOK_TRAILING_CR:
      next = ptr + enc->minBytesPerChar;
      /* fall through */
    case XML_TOK_ATTRIBUTE_VALUE_S:
    case XML_TOK_DATA_NEWLINE:
      if (!isCdata && (poolLength(pool) == 0 || poolLastByte(pool) == ' '))
	break;
      if (!poolAppendByte(pool, ' '))
	return XML_ERROR_NO_MEMORY;
      break;
    case XML_TOK_ENTITY_REF:
      {
	const char *name = poolStoreString(&temp2Pool, enc,
					   ptr + enc->minBytesPerChar,
					   next - enc->minBytesPerChar);
	ENTITY *entity;
	if (!name)
	  return XML_ERROR_NO_MEMORY;
	entity = (ENTITY *)lookup(&dtd.generalEntities, name, 0);
	poolDiscard(&temp2Pool);
	if (!entity) {
	  if (dtd.complete) {
	    errorPtr = ptr;
	    return XML_ERROR_UNDEFINED_ENTITY;
	  }
	}
	else if (entity->open) {
	  errorPtr = ptr;
	  return XML_ERROR_RECURSIVE_ENTITY_REF;
	}
	else if (entity->notation) {
	  errorPtr = ptr;
	  return XML_ERROR_BINARY_ENTITY_REF;
	}
	else if (entity->magic) {
	  int i;
	  for (i = 0; i < entity->textLen; i++)
	    if (!poolAppendByte(pool, entity->textPtr[i]))
	      return XML_ERROR_NO_MEMORY;
	}
	else if (!entity->textPtr) {
	  errorPtr = ptr;
  	  return XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF;
	}
	else {
	  enum XML_Error result;
	  const char *textEnd = entity->textPtr + entity->textLen;
	  entity->open = 1;
	  result = appendAttributeValue(parser, utf8, isCdata, entity->textPtr, textEnd, pool);
	  entity->open = 0;
	  if (result) {
	    errorPtr = ptr;
	    return result;
	  }
	}
      }
      break;
    default:
      abort();
    }
    ptr = next;
  }
  /* not reached */
}

static
enum XML_Error storeEntityValue(XML_Parser parser,
				const char *entityTextPtr,
				const char *entityTextEnd)
{
  const ENCODING *utf8 = XmlGetInternalEncoding(XML_UTF8_ENCODING);
  STRING_POOL *pool = &(dtd.pool);
  entityTextPtr += encoding->minBytesPerChar;
  entityTextEnd -= encoding->minBytesPerChar;
  for (;;) {
    const char *next;
    int tok = XmlEntityValueTok(encoding, entityTextPtr, entityTextEnd, &next);
    switch (tok) {
    case XML_TOK_PARAM_ENTITY_REF:
      errorPtr = entityTextPtr;
      return XML_ERROR_SYNTAX;
    case XML_TOK_NONE:
      if (declEntity) {
	declEntity->textPtr = pool->start;
	declEntity->textLen = pool->ptr - pool->start;
	poolFinish(pool);
      }
      else
	poolDiscard(pool);
      return XML_ERROR_NONE;
    case XML_TOK_ENTITY_REF:
    case XML_TOK_DATA_CHARS:
      if (!poolAppend(pool, encoding, entityTextPtr, next))
	return XML_ERROR_NO_MEMORY;
      break;
    case XML_TOK_TRAILING_CR:
      next = entityTextPtr + encoding->minBytesPerChar;
      /* fall through */
    case XML_TOK_DATA_NEWLINE:
      if (pool->end == pool->ptr && !poolGrow(pool))
	return XML_ERROR_NO_MEMORY;
      *(pool->ptr)++ = '\n';
      break;
    case XML_TOK_CHAR_REF:
      {
	char buf[XML_MAX_BYTES_PER_CHAR];
	int i;
	int n = XmlCharRefNumber(encoding, entityTextPtr);
	if (n < 0) {
	  errorPtr = entityTextPtr;
	  return XML_ERROR_BAD_CHAR_REF;
	}
	n = XmlEncode(utf8, n, buf);
	if (!n) {
	  errorPtr = entityTextPtr;
	  return XML_ERROR_BAD_CHAR_REF;
	}
	for (i = 0; i < n; i++) {
	  if (pool->end == pool->ptr && !poolGrow(pool))
	    return XML_ERROR_NO_MEMORY;
	  *(pool->ptr)++ = buf[i];
	}
      }
      break;
    case XML_TOK_PARTIAL:
      errorPtr = entityTextPtr;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_INVALID:
      errorPtr = next;
      return XML_ERROR_INVALID_TOKEN;
    default:
      abort();
    }
    entityTextPtr = next;
  }
  /* not reached */
}

static void
normalizeLines(char *s)
{
  char *p;
  s = strchr(s, '\r');
  if (!s)
    return;
  p = s;
  while (*s) {
    if (*s == '\r') {
      *p++ = '\n';
      if (*++s == '\n')
        s++;
    }
    else
      *p++ = *s++;
  }
  *p = '\0';
}

static int
reportProcessingInstruction(XML_Parser parser, const ENCODING *enc, const char *start, const char *end)
{
  const char *target;
  char *data;
  const char *tem;
  if (!processingInstructionHandler)
    return 1;
  target = start + enc->minBytesPerChar * 2;
  tem = target + XmlNameLength(enc, target);
  target = poolStoreString(&tempPool, enc, target, tem);
  if (!target)
    return 0;
  poolFinish(&tempPool);
  data = poolStoreString(&tempPool, enc,
			XmlSkipS(enc, tem),
			end - enc->minBytesPerChar*2);
  if (!data)
    return 0;
  normalizeLines(data);
  processingInstructionHandler(userData, target, data);
  poolClear(&tempPool);
  return 1;
}

static int
defineAttribute(ELEMENT_TYPE *type, ATTRIBUTE_ID *attId, int isCdata, const char *value)
{
  DEFAULT_ATTRIBUTE *att;
  if (type->nDefaultAtts == type->allocDefaultAtts) {
    if (type->allocDefaultAtts == 0)
      type->allocDefaultAtts = 8;
    else
      type->allocDefaultAtts *= 2;
    type->defaultAtts = realloc(type->defaultAtts,
				type->allocDefaultAtts*sizeof(DEFAULT_ATTRIBUTE));
    if (!type->defaultAtts)
      return 0;
  }
  att = type->defaultAtts + type->nDefaultAtts;
  att->id = attId;
  att->value = value;
  att->isCdata = isCdata;
  if (!isCdata)
    attId->maybeTokenized = 1;
  type->nDefaultAtts += 1;
  return 1;
}

static ATTRIBUTE_ID *
getAttributeId(XML_Parser parser, const ENCODING *enc, const char *start, const char *end)
{
  ATTRIBUTE_ID *id;
  const char *name;
  if (!poolAppendByte(&dtd.pool, 0))
    return 0;
  name = poolStoreString(&dtd.pool, enc, start, end);
  if (!name)
    return 0;
  ++name;
  id = (ATTRIBUTE_ID *)lookup(&dtd.attributeIds, name, sizeof(ATTRIBUTE_ID));
  if (!id)
    return 0;
  if (id->name != name)
    poolDiscard(&dtd.pool);
  else
    poolFinish(&dtd.pool);
  return id;
}

static int dtdInit(DTD *p)
{
  static const char *names[] = { "lt", "amp", "gt", "quot", "apos" };
  static const char chars[] = { '<', '&', '>', '"', '\'' };
  int i;

  poolInit(&(p->pool));
  hashTableInit(&(p->generalEntities));
  for (i = 0; i < 5; i++) {
    ENTITY *entity = (ENTITY *)lookup(&(p->generalEntities), names[i], sizeof(ENTITY));
    if (!entity)
      return 0;
    entity->textPtr = chars + i;
    entity->textLen = 1;
    entity->magic = 1;
  }
  hashTableInit(&(p->elementTypes));
  hashTableInit(&(p->attributeIds));
  p->complete = 1;
  return 1;
}

static void dtdDestroy(DTD *p)
{
  HASH_TABLE_ITER iter;
  hashTableIterInit(&iter, &(p->elementTypes));
  for (;;) {
    ELEMENT_TYPE *e = (ELEMENT_TYPE *)hashTableIterNext(&iter);
    if (!e)
      break;
    free(e->defaultAtts);
  }
  hashTableDestroy(&(p->generalEntities));
  hashTableDestroy(&(p->elementTypes));
  hashTableDestroy(&(p->attributeIds));
  poolDestroy(&(p->pool));
}

static
void poolInit(STRING_POOL *pool)
{
  pool->blocks = 0;
  pool->freeBlocks = 0;
  pool->start = 0;
  pool->ptr = 0;
  pool->end = 0;
}

static
void poolClear(STRING_POOL *pool)
{
  if (!pool->freeBlocks)
    pool->freeBlocks = pool->blocks;
  else {
    BLOCK *p = pool->blocks;
    while (p) {
      BLOCK *tem = p->next;
      p->next = pool->freeBlocks;
      pool->freeBlocks = p;
      p = tem;
    }
  }
  pool->blocks = 0;
  pool->start = 0;
  pool->ptr = 0;
  pool->end = 0;
}

static
void poolDestroy(STRING_POOL *pool)
{
  BLOCK *p = pool->blocks;
  while (p) {
    BLOCK *tem = p->next;
    free(p);
    p = tem;
  }
  pool->blocks = 0;
  p = pool->freeBlocks;
  while (p) {
    BLOCK *tem = p->next;
    free(p);
    p = tem;
  }
  pool->freeBlocks = 0;
  pool->ptr = 0;
  pool->start = 0;
  pool->end = 0;
}

static
char *poolAppend(STRING_POOL *pool, const ENCODING *enc,
		 const char *ptr, const char *end)
{
  if (!pool->ptr && !poolGrow(pool))
    return 0;
  for (;;) {
    XmlConvert(enc, XML_UTF8_ENCODING, &ptr, end, &(pool->ptr), pool->end);
    if (ptr == end)
      break;
    if (!poolGrow(pool))
      return 0;
  }
  return pool->start;
}


static
char *poolStoreString(STRING_POOL *pool, const ENCODING *enc,
		      const char *ptr, const char *end)
{
  if (!poolAppend(pool, enc, ptr, end))
    return 0;
  if (pool->ptr == pool->end && !poolGrow(pool))
    return 0;
  *(pool->ptr)++ = 0;
  return pool->start;
}

static
int poolGrow(STRING_POOL *pool)
{
  if (pool->freeBlocks) {
    if (pool->start == 0) {
      pool->blocks = pool->freeBlocks;
      pool->freeBlocks = pool->freeBlocks->next;
      pool->blocks->next = 0;
      pool->start = pool->blocks->s;
      pool->end = pool->start + pool->blocks->size;
      pool->ptr = pool->start;
      return 1;
    }
    if (pool->end - pool->start < pool->freeBlocks->size) {
      BLOCK *tem = pool->freeBlocks->next;
      pool->freeBlocks->next = pool->blocks;
      pool->blocks = pool->freeBlocks;
      pool->freeBlocks = tem;
      memcpy(pool->blocks->s, pool->start, pool->end - pool->start);
      pool->ptr = pool->blocks->s + (pool->ptr - pool->start);
      pool->start = pool->blocks->s;
      pool->end = pool->start + pool->blocks->size;
      return 1;
    }
  }
  if (pool->blocks && pool->start == pool->blocks->s) {
    int blockSize = (pool->end - pool->start)*2;
    pool->blocks = realloc(pool->blocks, offsetof(BLOCK, s) + blockSize);
    if (!pool->blocks)
      return 0;
    pool->blocks->size = blockSize;
    pool->ptr = pool->blocks->s + (pool->ptr - pool->start);
    pool->start = pool->blocks->s;
    pool->end = pool->start + blockSize;
  }
  else {
    BLOCK *tem;
    int blockSize = pool->end - pool->start;
    if (blockSize < INIT_BLOCK_SIZE)
      blockSize = INIT_BLOCK_SIZE;
    else
      blockSize *= 2;
    tem = malloc(offsetof(BLOCK, s) + blockSize);
    if (!tem)
      return 0;
    tem->size = blockSize;
    tem->next = pool->blocks;
    pool->blocks = tem;
    memcpy(tem->s, pool->start, pool->ptr - pool->start);
    pool->ptr = tem->s + (pool->ptr - pool->start);
    pool->start = tem->s;
    pool->end = tem->s + blockSize;
  }
  return 1;
}
