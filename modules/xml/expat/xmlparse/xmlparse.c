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

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "xmldef.h"

#ifdef XML_UNICODE
#define XML_ENCODE_MAX XML_UTF16_ENCODE_MAX
#define XmlConvert XmlUtf16Convert
#define XmlGetInternalEncoding XmlGetUtf16InternalEncoding
#define XmlEncode XmlUtf16Encode
#define MUST_CONVERT(enc, s) (!(enc)->isUtf16 || (((unsigned long)s) & 1))
typedef unsigned short ICHAR;
#else
#define XML_ENCODE_MAX XML_UTF8_ENCODE_MAX
#define XmlConvert XmlUtf8Convert
#define XmlGetInternalEncoding XmlGetUtf8InternalEncoding
#define XmlEncode XmlUtf8Encode
#define MUST_CONVERT(enc, s) (!(enc)->isUtf8)
typedef char ICHAR;
#endif

#ifdef XML_UNICODE_WCHAR_T
#define XML_T(x) L ## x
#else
#define XML_T(x) x
#endif

/* Round up n to be a multiple of sz, where sz is a power of 2. */
#define ROUND_UP(n, sz) (((n) + ((sz) - 1)) & ~((sz) - 1))

#include "xmlparse.h"
#include "xmltok.h"
#include "xmlrole.h"
#include "hashtable.h"

#define INIT_TAG_BUF_SIZE 32  /* must be a multiple of sizeof(XML_Char) */
#define INIT_DATA_BUF_SIZE 1024
#define INIT_ATTS_SIZE 16
#define INIT_BLOCK_SIZE 1024
#define INIT_BUFFER_SIZE 1024

typedef struct tag {
  struct tag *parent;
  const char *rawName;
  int rawNameLength;
  const XML_Char *name;
  char *buf;
  char *bufEnd;
} TAG;

typedef struct {
  const XML_Char *name;
  const XML_Char *textPtr;
  int textLen;
  const XML_Char *systemId;
  const XML_Char *base;
  const XML_Char *publicId;
  const XML_Char *notation;
  char open;
} ENTITY;

typedef struct block {
  struct block *next;
  int size;
  XML_Char s[1];
} BLOCK;

typedef struct {
  BLOCK *blocks;
  BLOCK *freeBlocks;
  const XML_Char *end;
  XML_Char *ptr;
  XML_Char *start;
} STRING_POOL;

/* The XML_Char before the name is used to determine whether
an attribute has been specified. */
typedef struct {
  XML_Char *name;
  char maybeTokenized;
} ATTRIBUTE_ID;

typedef struct {
  const ATTRIBUTE_ID *id;
  char isCdata;
  const XML_Char *value;
} DEFAULT_ATTRIBUTE;

typedef struct {
  const XML_Char *name;
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
  const XML_Char *base;
} DTD;

typedef enum XML_Error Processor(XML_Parser parser,
				 const char *start,
				 const char *end,
				 const char **endPtr);

static Processor prologProcessor;
static Processor prologInitProcessor;
static Processor contentProcessor;
static Processor cdataSectionProcessor;
static Processor epilogProcessor;
static Processor errorProcessor;
static Processor externalEntityInitProcessor;
static Processor externalEntityInitProcessor2;
static Processor externalEntityInitProcessor3;
static Processor externalEntityContentProcessor;

static enum XML_Error
handleUnknownEncoding(XML_Parser parser, const XML_Char *encodingName);
static enum XML_Error
processXmlDecl(XML_Parser parser, int isGeneralTextEntity, const char *, const char *);
static enum XML_Error
initializeEncoding(XML_Parser parser);
static enum XML_Error
doContent(XML_Parser parser, int startTagLevel, const ENCODING *enc,
	  const char *start, const char *end, const char **endPtr);
static enum XML_Error
doCdataSection(XML_Parser parser, const ENCODING *, const char **startPtr, const char *end, const char **nextPtr);
static enum XML_Error storeAtts(XML_Parser parser, const ENCODING *, const XML_Char *tagName, const char *s);
static int
defineAttribute(ELEMENT_TYPE *type, ATTRIBUTE_ID *, int isCdata, const XML_Char *dfltValue);
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
static void
reportDefault(XML_Parser parser, const ENCODING *enc, const char *start, const char *end);

static const XML_Char *getOpenEntityNames(XML_Parser parser);
static int setOpenEntityNames(XML_Parser parser, const XML_Char *openEntityNames);
static void normalizePublicId(XML_Char *s);
static int dtdInit(DTD *);
static void dtdDestroy(DTD *);
static int dtdCopy(DTD *newDtd, const DTD *oldDtd);
static void poolInit(STRING_POOL *);
static void poolClear(STRING_POOL *);
static void poolDestroy(STRING_POOL *);
static XML_Char *poolAppend(STRING_POOL *pool, const ENCODING *enc,
			    const char *ptr, const char *end);
static XML_Char *poolStoreString(STRING_POOL *pool, const ENCODING *enc,
				  const char *ptr, const char *end);
static int poolGrow(STRING_POOL *pool);
static const XML_Char *poolCopyString(STRING_POOL *pool, const XML_Char *s);
static const XML_Char *poolCopyStringN(STRING_POOL *pool, const XML_Char *s, int n);

#define poolStart(pool) ((pool)->start)
#define poolEnd(pool) ((pool)->ptr)
#define poolLength(pool) ((pool)->ptr - (pool)->start)
#define poolChop(pool) ((void)--(pool->ptr))
#define poolLastChar(pool) (((pool)->ptr)[-1])
#define poolDiscard(pool) ((pool)->ptr = (pool)->start)
#define poolFinish(pool) ((pool)->start = (pool)->ptr)
#define poolAppendChar(pool, c) \
  (((pool)->ptr == (pool)->end && !poolGrow(pool)) \
   ? 0 \
   : ((*((pool)->ptr)++ = c), 1))

typedef struct {
  /* The first member must be userData so that the XML_GetUserData macro works. */
  void *userData;
  void *handlerArg;
  char *buffer;
  /* first character to be parsed */
  const char *bufferPtr;
  /* past last character to be parsed */
  char *bufferEnd;
  /* allocated end of buffer */
  const char *bufferLim;
  long parseEndByteIndex;
  const char *parseEndPtr;
  XML_Char *dataBuf;
  XML_Char *dataBufEnd;
  XML_StartElementHandler startElementHandler;
  XML_EndElementHandler endElementHandler;
  XML_CharacterDataHandler characterDataHandler;
  XML_ProcessingInstructionHandler processingInstructionHandler;
  XML_DefaultHandler defaultHandler;
  XML_UnparsedEntityDeclHandler unparsedEntityDeclHandler;
  XML_NotationDeclHandler notationDeclHandler;
  XML_ExternalEntityRefHandler externalEntityRefHandler;
  XML_UnknownEncodingHandler unknownEncodingHandler;
  const ENCODING *encoding;
  INIT_ENCODING initEncoding;
  const XML_Char *protocolEncodingName;
  void *unknownEncodingMem;
  void *unknownEncodingData;
  void *unknownEncodingHandlerData;
  void (*unknownEncodingRelease)(void *);
  PROLOG_STATE prologState;
  Processor *processor;
  enum XML_Error errorCode;
  const char *eventPtr;
  const char *eventEndPtr;
  const char *positionPtr;
  int tagLevel;
  ENTITY *declEntity;
  const XML_Char *declNotationName;
  const XML_Char *declNotationPublicId;
  ELEMENT_TYPE *declElementType;
  ATTRIBUTE_ID *declAttributeId;
  char declAttributeIsCdata;
  DTD dtd;
  TAG *tagStack;
  TAG *freeTagList;
  int attsSize;
  ATTRIBUTE *atts;
  POSITION position;
  STRING_POOL tempPool;
  STRING_POOL temp2Pool;
  char *groupConnector;
  unsigned groupSize;
  int hadExternalDoctype;
} Parser;

#define userData (((Parser *)parser)->userData)
#define handlerArg (((Parser *)parser)->handlerArg)
#define startElementHandler (((Parser *)parser)->startElementHandler)
#define endElementHandler (((Parser *)parser)->endElementHandler)
#define characterDataHandler (((Parser *)parser)->characterDataHandler)
#define processingInstructionHandler (((Parser *)parser)->processingInstructionHandler)
#define defaultHandler (((Parser *)parser)->defaultHandler)
#define unparsedEntityDeclHandler (((Parser *)parser)->unparsedEntityDeclHandler)
#define notationDeclHandler (((Parser *)parser)->notationDeclHandler)
#define externalEntityRefHandler (((Parser *)parser)->externalEntityRefHandler)
#define unknownEncodingHandler (((Parser *)parser)->unknownEncodingHandler)
#define encoding (((Parser *)parser)->encoding)
#define initEncoding (((Parser *)parser)->initEncoding)
#define unknownEncodingMem (((Parser *)parser)->unknownEncodingMem)
#define unknownEncodingData (((Parser *)parser)->unknownEncodingData)
#define unknownEncodingHandlerData \
  (((Parser *)parser)->unknownEncodingHandlerData)
#define unknownEncodingRelease (((Parser *)parser)->unknownEncodingRelease)
#define protocolEncodingName (((Parser *)parser)->protocolEncodingName)
#define prologState (((Parser *)parser)->prologState)
#define processor (((Parser *)parser)->processor)
#define errorCode (((Parser *)parser)->errorCode)
#define eventPtr (((Parser *)parser)->eventPtr)
#define eventEndPtr (((Parser *)parser)->eventEndPtr)
#define positionPtr (((Parser *)parser)->positionPtr)
#define position (((Parser *)parser)->position)
#define tagLevel (((Parser *)parser)->tagLevel)
#define buffer (((Parser *)parser)->buffer)
#define bufferPtr (((Parser *)parser)->bufferPtr)
#define bufferEnd (((Parser *)parser)->bufferEnd)
#define parseEndByteIndex (((Parser *)parser)->parseEndByteIndex)
#define parseEndPtr (((Parser *)parser)->parseEndPtr)
#define bufferLim (((Parser *)parser)->bufferLim)
#define dataBuf (((Parser *)parser)->dataBuf)
#define dataBufEnd (((Parser *)parser)->dataBufEnd)
#define dtd (((Parser *)parser)->dtd)
#define declEntity (((Parser *)parser)->declEntity)
#define declNotationName (((Parser *)parser)->declNotationName)
#define declNotationPublicId (((Parser *)parser)->declNotationPublicId)
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

XML_Parser XML_ParserCreate(const XML_Char *encodingName)
{
  XML_Parser parser = malloc(sizeof(Parser));
  if (!parser)
    return parser;
  processor = prologInitProcessor;
  XmlPrologStateInit(&prologState);
  userData = 0;
  handlerArg = 0;
  startElementHandler = 0;
  endElementHandler = 0;
  characterDataHandler = 0;
  processingInstructionHandler = 0;
  defaultHandler = 0;
  unparsedEntityDeclHandler = 0;
  notationDeclHandler = 0;
  externalEntityRefHandler = 0;
  unknownEncodingHandler = 0;
  buffer = 0;
  bufferPtr = 0;
  bufferEnd = 0;
  parseEndByteIndex = 0;
  parseEndPtr = 0;
  bufferLim = 0;
  declElementType = 0;
  declAttributeId = 0;
  declEntity = 0;
  declNotationName = 0;
  declNotationPublicId = 0;
  memset(&position, 0, sizeof(POSITION));
  errorCode = XML_ERROR_NONE;
  eventPtr = 0;
  eventEndPtr = 0;
  positionPtr = 0;
  tagLevel = 0;
  tagStack = 0;
  freeTagList = 0;
  attsSize = INIT_ATTS_SIZE;
  atts = malloc(attsSize * sizeof(ATTRIBUTE));
  dataBuf = malloc(INIT_DATA_BUF_SIZE * sizeof(XML_Char));
  groupSize = 0;
  groupConnector = 0;
  hadExternalDoctype = 0;
  unknownEncodingMem = 0;
  unknownEncodingRelease = 0;
  unknownEncodingData = 0;
  unknownEncodingHandlerData = 0;
  poolInit(&tempPool);
  poolInit(&temp2Pool);
  protocolEncodingName = encodingName ? poolCopyString(&tempPool, encodingName) : 0;
  if (!dtdInit(&dtd) || !atts || !dataBuf
      || (encodingName && !protocolEncodingName)) {
    XML_ParserFree(parser);
    return 0;
  }
  dataBufEnd = dataBuf + INIT_DATA_BUF_SIZE;
  XmlInitEncoding(&initEncoding, &encoding, 0);
  return parser;
}

XML_Parser XML_ExternalEntityParserCreate(XML_Parser oldParser,
					  const XML_Char *openEntityNames,
					  const XML_Char *encodingName)
{
  XML_Parser parser = oldParser;
  DTD *oldDtd = &dtd;
  XML_StartElementHandler oldStartElementHandler = startElementHandler;
  XML_EndElementHandler oldEndElementHandler = endElementHandler;
  XML_CharacterDataHandler oldCharacterDataHandler = characterDataHandler;
  XML_ProcessingInstructionHandler oldProcessingInstructionHandler = processingInstructionHandler;
  XML_DefaultHandler oldDefaultHandler = defaultHandler;
  XML_ExternalEntityRefHandler oldExternalEntityRefHandler = externalEntityRefHandler;
  XML_UnknownEncodingHandler oldUnknownEncodingHandler = unknownEncodingHandler;
  void *oldUserData = userData;
  void *oldHandlerArg = handlerArg;
 
  parser = XML_ParserCreate(encodingName);
  if (!parser)
    return 0;
  startElementHandler = oldStartElementHandler;
  endElementHandler = oldEndElementHandler;
  characterDataHandler = oldCharacterDataHandler;
  processingInstructionHandler = oldProcessingInstructionHandler;
  defaultHandler = oldDefaultHandler;
  externalEntityRefHandler = oldExternalEntityRefHandler;
  unknownEncodingHandler = oldUnknownEncodingHandler;
  userData = oldUserData;
  if (oldUserData == oldHandlerArg)
    handlerArg = userData;
  else
    handlerArg = parser;
  if (!dtdCopy(&dtd, oldDtd) || !setOpenEntityNames(parser, openEntityNames)) {
    XML_ParserFree(parser);
    return 0;
  }
  processor = externalEntityInitProcessor;
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
  free(unknownEncodingMem);
  if (unknownEncodingRelease)
    unknownEncodingRelease(unknownEncodingData);
  free(parser);
}

void XML_UseParserAsHandlerArg(XML_Parser parser)
{
  handlerArg = parser;
}

void XML_SetUserData(XML_Parser parser, void *p)
{
  if (handlerArg == userData)
    handlerArg = userData = p;
  else
    userData = p;
}

int XML_SetBase(XML_Parser parser, const XML_Char *p)
{
  if (p) {
    p = poolCopyString(&dtd.pool, p);
    if (!p)
      return 0;
    dtd.base = p;
  }
  else
    dtd.base = 0;
  return 1;
}

const XML_Char *XML_GetBase(XML_Parser parser)
{
  return dtd.base;
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

void XML_SetDefaultHandler(XML_Parser parser,
			   XML_DefaultHandler handler)
{
  defaultHandler = handler;
}

void XML_SetUnparsedEntityDeclHandler(XML_Parser parser,
				      XML_UnparsedEntityDeclHandler handler)
{
  unparsedEntityDeclHandler = handler;
}

void XML_SetNotationDeclHandler(XML_Parser parser,
				XML_NotationDeclHandler handler)
{
  notationDeclHandler = handler;
}

void XML_SetExternalEntityRefHandler(XML_Parser parser,
				     XML_ExternalEntityRefHandler handler)
{
  externalEntityRefHandler = handler;
}

void XML_SetUnknownEncodingHandler(XML_Parser parser,
				   XML_UnknownEncodingHandler handler,
				   void *data)
{
  unknownEncodingHandler = handler;
  unknownEncodingHandlerData = data;
}

int XML_Parse(XML_Parser parser, const char *s, int len, int isFinal)
{
  if (len == 0) {
    if (!isFinal)
      return 1;
    errorCode = processor(parser, bufferPtr, parseEndPtr = bufferEnd, 0);
    if (errorCode == XML_ERROR_NONE)
      return 1;
    eventEndPtr = eventPtr;
    return 0;
  }
  else if (bufferPtr == bufferEnd) {
    const char *end;
    int nLeftOver;
    parseEndByteIndex += len;
    positionPtr = s;
    if (isFinal) {
      errorCode = processor(parser, s, parseEndPtr = s + len, 0);
      if (errorCode == XML_ERROR_NONE)
	return 1;
      eventEndPtr = eventPtr;
      return 0;
    }
    errorCode = processor(parser, s, parseEndPtr = s + len, &end);
    if (errorCode != XML_ERROR_NONE) {
      eventEndPtr = eventPtr;
      return 0;
    }
    XmlUpdatePosition(encoding, positionPtr, end, &position);
    nLeftOver = s + len - end;
    if (nLeftOver) {
      if (buffer == 0 || nLeftOver > bufferLim - buffer) {
	/* FIXME avoid integer overflow */
	buffer = buffer == 0 ? malloc(len * 2) : realloc(buffer, len * 2);
	if (!buffer) {
	  errorCode = XML_ERROR_NO_MEMORY;
	  eventPtr = eventEndPtr = 0;
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
  positionPtr = start;
  bufferEnd += len;
  parseEndByteIndex += len;
  errorCode = processor(parser, start, parseEndPtr = bufferEnd,
			isFinal ? (const char **)0 : &bufferPtr);
  if (errorCode == XML_ERROR_NONE) {
    if (!isFinal)
      XmlUpdatePosition(encoding, positionPtr, bufferPtr, &position);
    return 1;
  }
  else {
    eventEndPtr = eventPtr;
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

enum XML_Error XML_GetErrorCode(XML_Parser parser)
{
  return errorCode;
}

long XML_GetCurrentByteIndex(XML_Parser parser)
{
  if (eventPtr)
    return parseEndByteIndex - (parseEndPtr - eventPtr);
  return -1;
}

int XML_GetCurrentLineNumber(XML_Parser parser)
{
  if (eventPtr) {
    XmlUpdatePosition(encoding, positionPtr, eventPtr, &position);
    positionPtr = eventPtr;
  }
  return position.lineNumber + 1;
}

int XML_GetCurrentColumnNumber(XML_Parser parser)
{
  if (eventPtr) {
    XmlUpdatePosition(encoding, positionPtr, eventPtr, &position);
    positionPtr = eventPtr;
  }
  return position.columnNumber;
}

void XML_DefaultCurrent(XML_Parser parser)
{
  if (defaultHandler)
    reportDefault(parser, encoding, eventPtr, eventEndPtr);
}

const XML_LChar *XML_ErrorString(int code)
{
  static const XML_LChar *message[] = {
    0,
    XML_T("out of memory"),
    XML_T("syntax error"),
    XML_T("no element found"),
    XML_T("not well-formed"),
    XML_T("unclosed token"),
    XML_T("unclosed token"),
    XML_T("mismatched tag"),
    XML_T("duplicate attribute"),
    XML_T("junk after document element"),
    XML_T("illegal parameter entity reference"),
    XML_T("undefined entity"),
    XML_T("recursive entity reference"),
    XML_T("asynchronous entity"),
    XML_T("reference to invalid character number"),
    XML_T("reference to binary entity"),
    XML_T("reference to external entity in attribute"),
    XML_T("xml processing instruction not at start of external entity"),
    XML_T("unknown encoding"),
    XML_T("encoding specified in XML declaration is incorrect"),
    XML_T("unclosed CDATA section"),
    XML_T("error in processing external entity reference")
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

static
enum XML_Error externalEntityInitProcessor(XML_Parser parser,
					   const char *start,
					   const char *end,
					   const char **endPtr)
{
  enum XML_Error result = initializeEncoding(parser);
  if (result != XML_ERROR_NONE)
    return result;
  processor = externalEntityInitProcessor2;
  return externalEntityInitProcessor2(parser, start, end, endPtr);
}

static
enum XML_Error externalEntityInitProcessor2(XML_Parser parser,
					    const char *start,
					    const char *end,
					    const char **endPtr)
{
  const char *next;
  int tok = XmlContentTok(encoding, start, end, &next);
  switch (tok) {
  case XML_TOK_BOM:
    start = next;
    break;
  case XML_TOK_PARTIAL:
    if (endPtr) {
      *endPtr = start;
      return XML_ERROR_NONE;
    }
    eventPtr = start;
    return XML_ERROR_UNCLOSED_TOKEN;
  case XML_TOK_PARTIAL_CHAR:
    if (endPtr) {
      *endPtr = start;
      return XML_ERROR_NONE;
    }
    eventPtr = start;
    return XML_ERROR_PARTIAL_CHAR;
  }
  processor = externalEntityInitProcessor3;
  return externalEntityInitProcessor3(parser, start, end, endPtr);
}

static
enum XML_Error externalEntityInitProcessor3(XML_Parser parser,
					    const char *start,
					    const char *end,
					    const char **endPtr)
{
  const char *next;
  int tok = XmlContentTok(encoding, start, end, &next);
  switch (tok) {
  case XML_TOK_XML_DECL:
    {
      enum XML_Error result = processXmlDecl(parser, 1, start, next);
      if (result != XML_ERROR_NONE)
	return result;
      start = next;
    }
    break;
  case XML_TOK_PARTIAL:
    if (endPtr) {
      *endPtr = start;
      return XML_ERROR_NONE;
    }
    eventPtr = start;
    return XML_ERROR_UNCLOSED_TOKEN;
  case XML_TOK_PARTIAL_CHAR:
    if (endPtr) {
      *endPtr = start;
      return XML_ERROR_NONE;
    }
    eventPtr = start;
    return XML_ERROR_PARTIAL_CHAR;
  }
  processor = externalEntityContentProcessor;
  tagLevel = 1;
  return doContent(parser, 1, encoding, start, end, endPtr);
}

static
enum XML_Error externalEntityContentProcessor(XML_Parser parser,
					      const char *start,
					      const char *end,
					      const char **endPtr)
{
  return doContent(parser, 1, encoding, start, end, endPtr);
}

static enum XML_Error
doContent(XML_Parser parser,
	  int startTagLevel,
	  const ENCODING *enc,
	  const char *s,
	  const char *end,
	  const char **nextPtr)
{
  const ENCODING *internalEnc = XmlGetInternalEncoding();
  const char *dummy;
  const char **eventPP;
  const char **eventEndPP;
  if (enc == encoding) {
    eventPP = &eventPtr;
    *eventPP = s;
    eventEndPP = &eventEndPtr;
  }
  else
    eventPP = eventEndPP = &dummy;
  for (;;) {
    const char *next;
    int tok = XmlContentTok(enc, s, end, &next);
    *eventEndPP = next;
    switch (tok) {
    case XML_TOK_TRAILING_CR:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      *eventEndPP = end;
      if (characterDataHandler) {
	XML_Char c = XML_T('\n');
	characterDataHandler(handlerArg, &c, 1);
      }
      else if (defaultHandler)
	reportDefault(parser, enc, s, end);
      if (startTagLevel == 0)
	return XML_ERROR_NO_ELEMENTS;
      if (tagLevel != startTagLevel)
	return XML_ERROR_ASYNC_ENTITY;
      return XML_ERROR_NONE;
    case XML_TOK_NONE:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      if (startTagLevel > 0) {
	if (tagLevel != startTagLevel)
	  return XML_ERROR_ASYNC_ENTITY;
	return XML_ERROR_NONE;
      }
      return XML_ERROR_NO_ELEMENTS;
    case XML_TOK_INVALID:
      *eventPP = next;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_PARTIAL:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      return XML_ERROR_UNCLOSED_TOKEN;
    case XML_TOK_PARTIAL_CHAR:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      return XML_ERROR_PARTIAL_CHAR;
    case XML_TOK_ENTITY_REF:
      {
	const XML_Char *name;
	ENTITY *entity;
	XML_Char ch = XmlPredefinedEntityName(enc,
					      s + enc->minBytesPerChar,
					      next - enc->minBytesPerChar);
	if (ch) {
	  if (characterDataHandler)
	    characterDataHandler(handlerArg, &ch, 1);
	  else if (defaultHandler)
	    reportDefault(parser, enc, s, next);
	  break;
	}
	name = poolStoreString(&dtd.pool, enc,
				s + enc->minBytesPerChar,
				next - enc->minBytesPerChar);
	if (!name)
	  return XML_ERROR_NO_MEMORY;
	entity = (ENTITY *)lookup(&dtd.generalEntities, name, 0);
	poolDiscard(&dtd.pool);
	if (!entity) {
	  if (dtd.complete || dtd.standalone)
	    return XML_ERROR_UNDEFINED_ENTITY;
	  if (defaultHandler)
	    reportDefault(parser, enc, s, next);
	  break;
	}
	if (entity->open)
	  return XML_ERROR_RECURSIVE_ENTITY_REF;
	if (entity->notation)
	  return XML_ERROR_BINARY_ENTITY_REF;
	if (entity) {
	  if (entity->textPtr) {
	    enum XML_Error result;
	    if (defaultHandler) {
	      reportDefault(parser, enc, s, next);
	      break;
	    }
	    /* Protect against the possibility that somebody sets
	       the defaultHandler from inside another handler. */
	    *eventEndPP = *eventPP;
	    entity->open = 1;
	    result = doContent(parser,
			       tagLevel,
			       internalEnc,
			       (char *)entity->textPtr,
			       (char *)(entity->textPtr + entity->textLen),
			       0);
	    entity->open = 0;
	    if (result)
	      return result;
	  }
	  else if (externalEntityRefHandler) {
	    const XML_Char *openEntityNames;
	    entity->open = 1;
	    openEntityNames = getOpenEntityNames(parser);
	    entity->open = 0;
	    if (!openEntityNames)
	      return XML_ERROR_NO_MEMORY;
	    if (!externalEntityRefHandler(parser, openEntityNames, dtd.base, entity->systemId, entity->publicId))
	      return XML_ERROR_EXTERNAL_ENTITY_HANDLING;
	  }
	  else if (defaultHandler)
	    reportDefault(parser, enc, s, next);
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
	    bufSize = ROUND_UP(bufSize, sizeof(XML_Char));
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
	  XML_Char *toPtr;
	  for (;;) {
	    const char *rawNameEnd = tag->rawName + tag->rawNameLength;
	    const char *fromPtr = tag->rawName;
	    int bufSize;
	    if (nextPtr)
	      toPtr = (XML_Char *)(tag->buf + ROUND_UP(tag->rawNameLength, sizeof(XML_Char)));
	    else
	      toPtr = (XML_Char *)tag->buf;
	    tag->name = toPtr;
	    XmlConvert(enc,
		       &fromPtr, rawNameEnd,
		       (ICHAR **)&toPtr, (ICHAR *)tag->bufEnd - 1);
	    if (fromPtr == rawNameEnd)
	      break;
	    bufSize = (tag->bufEnd - tag->buf) << 1;
	    tag->buf = realloc(tag->buf, bufSize);
	    if (!tag->buf)
	      return XML_ERROR_NO_MEMORY;
	    tag->bufEnd = tag->buf + bufSize;
	    if (nextPtr)
	      tag->rawName = tag->buf;
	  }
	  *toPtr = XML_T('\0');
	  result = storeAtts(parser, enc, tag->name, s);
	  if (result)
	    return result;
	  startElementHandler(handlerArg, tag->name, (const XML_Char **)atts);
	  poolClear(&tempPool);
	}
	else {
	  tag->name = 0;
	  if (defaultHandler)
	    reportDefault(parser, enc, s, next);
	}
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
	const XML_Char *name = poolStoreString(&tempPool, enc, rawName,
					       rawName
					       + XmlNameLength(enc, rawName));
	if (!name)
	  return XML_ERROR_NO_MEMORY;
	poolFinish(&tempPool);
	if (startElementHandler) {
	  enum XML_Error result = storeAtts(parser, enc, name, s);
	  if (result)
	    return result;
	  startElementHandler(handlerArg, name, (const XML_Char **)atts);
	}
	if (endElementHandler) {
	  if (startElementHandler)
	    *eventEndPP = *eventPP;
	  endElementHandler(handlerArg, name);
	}
	poolClear(&tempPool);
      }
      else if (defaultHandler)
	reportDefault(parser, enc, s, next);
      if (tagLevel == 0)
	return epilogProcessor(parser, next, end, nextPtr);
      break;
    case XML_TOK_END_TAG:
      if (tagLevel == startTagLevel)
        return XML_ERROR_ASYNC_ENTITY;
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
	  *eventPP = rawName;
	  return XML_ERROR_TAG_MISMATCH;
	}
	--tagLevel;
	if (endElementHandler) {
	  if (tag->name)
	    endElementHandler(handlerArg, tag->name);
	  else {
	    const XML_Char *name = poolStoreString(&tempPool, enc, rawName,
	                                           rawName + len);
	    if (!name)
	      return XML_ERROR_NO_MEMORY;
	    endElementHandler(handlerArg, name);
	    poolClear(&tempPool);
	  }
	}
	else if (defaultHandler)
	  reportDefault(parser, enc, s, next);
	if (tagLevel == 0)
	  return epilogProcessor(parser, next, end, nextPtr);
      }
      break;
    case XML_TOK_CHAR_REF:
      {
	int n = XmlCharRefNumber(enc, s);
	if (n < 0)
	  return XML_ERROR_BAD_CHAR_REF;
	if (characterDataHandler) {
	  XML_Char buf[XML_ENCODE_MAX];
	  characterDataHandler(handlerArg, buf, XmlEncode(n, (ICHAR *)buf));
	}
	else if (defaultHandler)
	  reportDefault(parser, enc, s, next);
      }
      break;
    case XML_TOK_XML_DECL:
      return XML_ERROR_MISPLACED_XML_PI;
    case XML_TOK_DATA_NEWLINE:
      if (characterDataHandler) {
	XML_Char c = XML_T('\n');
	characterDataHandler(handlerArg, &c, 1);
      }
      else if (defaultHandler)
	reportDefault(parser, enc, s, next);
      break;
    case XML_TOK_CDATA_SECT_OPEN:
      {
	enum XML_Error result;
	if (characterDataHandler)
  	  characterDataHandler(handlerArg, dataBuf, 0);
	else if (defaultHandler)
	  reportDefault(parser, enc, s, next);
	result = doCdataSection(parser, enc, &next, end, nextPtr);
	if (!next) {
	  processor = cdataSectionProcessor;
	  return result;
	}
      }
      break;
    case XML_TOK_TRAILING_RSQB:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      if (characterDataHandler) {
	if (MUST_CONVERT(enc, s)) {
	  ICHAR *dataPtr = (ICHAR *)dataBuf;
	  XmlConvert(enc, &s, end, &dataPtr, (ICHAR *)dataBufEnd);
	  characterDataHandler(handlerArg, dataBuf, dataPtr - (ICHAR *)dataBuf);
	}
	else
	  characterDataHandler(handlerArg,
		  	       (XML_Char *)s,
			       (XML_Char *)end - (XML_Char *)s);
      }
      else if (defaultHandler)
	reportDefault(parser, enc, s, end);
      if (startTagLevel == 0) {
        *eventPP = end;
	return XML_ERROR_NO_ELEMENTS;
      }
      if (tagLevel != startTagLevel) {
	*eventPP = end;
	return XML_ERROR_ASYNC_ENTITY;
      }
      return XML_ERROR_NONE;
    case XML_TOK_DATA_CHARS:
      if (characterDataHandler) {
	if (MUST_CONVERT(enc, s)) {
	  for (;;) {
	    ICHAR *dataPtr = (ICHAR *)dataBuf;
	    XmlConvert(enc, &s, next, &dataPtr, (ICHAR *)dataBufEnd);
	    *eventEndPP = s;
	    characterDataHandler(handlerArg, dataBuf, dataPtr - (ICHAR *)dataBuf);
	    if (s == next)
	      break;
	    *eventPP = s;
	  }
	}
	else
	  characterDataHandler(handlerArg,
			       (XML_Char *)s,
			       (XML_Char *)next - (XML_Char *)s);
      }
      else if (defaultHandler)
	reportDefault(parser, enc, s, next);
      break;
    case XML_TOK_PI:
      if (!reportProcessingInstruction(parser, enc, s, next))
	return XML_ERROR_NO_MEMORY;
      break;
    default:
      if (defaultHandler)
	reportDefault(parser, enc, s, next);
      break;
    }
    *eventPP = s = next;
  }
  /* not reached */
}

/* If tagName is non-null, build a real list of attributes,
otherwise just check the attributes for well-formedness. */

static enum XML_Error storeAtts(XML_Parser parser, const ENCODING *enc,
				const XML_Char *tagName, const char *s)
{
  ELEMENT_TYPE *elementType = 0;
  int nDefaultAtts = 0;
  const XML_Char **appAtts;
  int i;
  int n;

  if (tagName) {
    elementType = (ELEMENT_TYPE *)lookup(&dtd.elementTypes, tagName, 0);
    if (elementType)
      nDefaultAtts = elementType->nDefaultAtts;
  }
  
  n = XmlGetAttributes(enc, s, attsSize, atts);
  if (n + nDefaultAtts > attsSize) {
    int oldAttsSize = attsSize;
    attsSize = n + nDefaultAtts + INIT_ATTS_SIZE;
    atts = realloc((void *)atts, attsSize * sizeof(ATTRIBUTE));
    if (!atts)
      return XML_ERROR_NO_MEMORY;
    if (n > oldAttsSize)
      XmlGetAttributes(enc, s, n, atts);
  }
  appAtts = (const XML_Char **)atts;
  for (i = 0; i < n; i++) {
    ATTRIBUTE_ID *attId = getAttributeId(parser, enc, atts[i].name,
					  atts[i].name
					  + XmlNameLength(enc, atts[i].name));
    if (!attId)
      return XML_ERROR_NO_MEMORY;
    if ((attId->name)[-1]) {
      if (enc == encoding)
	eventPtr = atts[i].name;
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
    ((XML_Char *)appAtts[i << 1])[-1] = 0;
  return XML_ERROR_NONE;
}

/* The idea here is to avoid using stack for each CDATA section when
the whole file is parsed with one call. */

static
enum XML_Error cdataSectionProcessor(XML_Parser parser,
				     const char *start,
			    	     const char *end,
				     const char **endPtr)
{
  enum XML_Error result = doCdataSection(parser, encoding, &start, end, endPtr);
  if (start) {
    processor = contentProcessor;
    return contentProcessor(parser, start, end, endPtr);
  }
  return result;
}

/* startPtr gets set to non-null is the section is closed, and to null if
the section is not yet closed. */

static
enum XML_Error doCdataSection(XML_Parser parser,
			      const ENCODING *enc,
			      const char **startPtr,
			      const char *end,
			      const char **nextPtr)
{
  const char *s = *startPtr;
  const char *dummy;
  const char **eventPP;
  const char **eventEndPP;
  if (enc == encoding) {
    eventPP = &eventPtr;
    *eventPP = s;
    eventEndPP = &eventEndPtr;
  }
  else
    eventPP = eventEndPP = &dummy;
  *startPtr = 0;
  for (;;) {
    const char *next;
    int tok = XmlCdataSectionTok(enc, s, end, &next);
    *eventEndPP = next;
    switch (tok) {
    case XML_TOK_CDATA_SECT_CLOSE:
      if (characterDataHandler)
	characterDataHandler(handlerArg, dataBuf, 0);
      else if (defaultHandler)
	reportDefault(parser, enc, s, next);
      *startPtr = next;
      return XML_ERROR_NONE;
    case XML_TOK_DATA_NEWLINE:
      if (characterDataHandler) {
	XML_Char c = XML_T('\n');
	characterDataHandler(handlerArg, &c, 1);
      }
      else if (defaultHandler)
	reportDefault(parser, enc, s, next);
      break;
    case XML_TOK_DATA_CHARS:
      if (characterDataHandler) {
	if (MUST_CONVERT(enc, s)) {
	  for (;;) {
  	    ICHAR *dataPtr = (ICHAR *)dataBuf;
	    XmlConvert(enc, &s, next, &dataPtr, (ICHAR *)dataBufEnd);
	    *eventEndPP = next;
	    characterDataHandler(handlerArg, dataBuf, dataPtr - (ICHAR *)dataBuf);
	    if (s == next)
	      break;
	    *eventPP = s;
	  }
	}
	else
	  characterDataHandler(handlerArg,
		  	       (XML_Char *)s,
			       (XML_Char *)next - (XML_Char *)s);
      }
      else if (defaultHandler)
	reportDefault(parser, enc, s, next);
      break;
    case XML_TOK_INVALID:
      *eventPP = next;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_PARTIAL_CHAR:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      return XML_ERROR_PARTIAL_CHAR;
    case XML_TOK_PARTIAL:
    case XML_TOK_NONE:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      return XML_ERROR_UNCLOSED_CDATA_SECTION;
    default:
      abort();
    }
    *eventPP = s = next;
  }
  /* not reached */
}

static enum XML_Error
initializeEncoding(XML_Parser parser)
{
  const char *s;
#ifdef XML_UNICODE
  char encodingBuf[128];
  if (!protocolEncodingName)
    s = 0;
  else {
    int i;
    for (i = 0; protocolEncodingName[i]; i++) {
      if (i == sizeof(encodingBuf) - 1
	  || protocolEncodingName[i] >= 0x80
	  || protocolEncodingName[i] < 0) {
	encodingBuf[0] = '\0';
	break;
      }
      encodingBuf[i] = (char)protocolEncodingName[i];
    }
    encodingBuf[i] = '\0';
    s = encodingBuf;
  }
#else
  s = protocolEncodingName;
#endif
  if (XmlInitEncoding(&initEncoding, &encoding, s))
    return XML_ERROR_NONE;
  return handleUnknownEncoding(parser, protocolEncodingName);
}

static enum XML_Error
processXmlDecl(XML_Parser parser, int isGeneralTextEntity,
	       const char *s, const char *next)
{
  const char *encodingName = 0;
  const ENCODING *newEncoding = 0;
  const char *version;
  int standalone = -1;
  if (!XmlParseXmlDecl(isGeneralTextEntity,
		       encoding,
		       s,
		       next,
		       &eventPtr,
		       &version,
		       &encodingName,
		       &newEncoding,
		       &standalone))
    return XML_ERROR_SYNTAX;
  if (defaultHandler)
    reportDefault(parser, encoding, s, next);
  if (!protocolEncodingName) {
    if (newEncoding) {
      if (newEncoding->minBytesPerChar != encoding->minBytesPerChar) {
	eventPtr = encodingName;
	return XML_ERROR_INCORRECT_ENCODING;
      }
      encoding = newEncoding;
    }
    else if (encodingName) {
      enum XML_Error result;
      const XML_Char *s = poolStoreString(&tempPool,
					  encoding,
					  encodingName,
					  encodingName
					  + XmlNameLength(encoding, encodingName));
      if (!s)
	return XML_ERROR_NO_MEMORY;
      result = handleUnknownEncoding(parser, s);
      poolDiscard(&tempPool);
      if (result == XML_ERROR_UNKNOWN_ENCODING)
	eventPtr = encodingName;
      return result;
    }
  }
  if (!isGeneralTextEntity && standalone == 1)
    dtd.standalone = 1;
  return XML_ERROR_NONE;
}

static enum XML_Error
handleUnknownEncoding(XML_Parser parser, const XML_Char *encodingName)
{
  if (unknownEncodingHandler) {
    XML_Encoding info;
    int i;
    for (i = 0; i < 256; i++)
      info.map[i] = -1;
    info.convert = 0;
    info.data = 0;
    info.release = 0;
    if (unknownEncodingHandler(unknownEncodingHandlerData, encodingName, &info)) {
      ENCODING *enc;
      unknownEncodingMem = malloc(XmlSizeOfUnknownEncoding());
      if (!unknownEncodingMem) {
	if (info.release)
	  info.release(info.data);
	return XML_ERROR_NO_MEMORY;
      }
      enc = XmlInitUnknownEncoding(unknownEncodingMem,
				   info.map,
				   info.convert,
				   info.data);
      if (enc) {
	unknownEncodingData = info.data;
	unknownEncodingRelease = info.release;
	encoding = enc;
	return XML_ERROR_NONE;
      }
    }
    if (info.release)
      info.release(info.data);
  }
  return XML_ERROR_UNKNOWN_ENCODING;
}

static enum XML_Error
prologInitProcessor(XML_Parser parser,
		    const char *s,
		    const char *end,
		    const char **nextPtr)
{
  enum XML_Error result = initializeEncoding(parser);
  if (result != XML_ERROR_NONE)
    return result;
  processor = prologProcessor;
  return prologProcessor(parser, s, end, nextPtr);
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
	eventPtr = next;
	return XML_ERROR_INVALID_TOKEN;
      case XML_TOK_NONE:
	return XML_ERROR_NO_ELEMENTS;
      case XML_TOK_PARTIAL:
	return XML_ERROR_UNCLOSED_TOKEN;
      case XML_TOK_PARTIAL_CHAR:
	return XML_ERROR_PARTIAL_CHAR;
      case XML_TOK_TRAILING_CR:
	eventPtr = s + encoding->minBytesPerChar;
	return XML_ERROR_NO_ELEMENTS;
      default:
	abort();
      }
    }
    switch (XmlTokenRole(&prologState, tok, s, next, encoding)) {
    case XML_ROLE_XML_DECL:
      {
	enum XML_Error result = processXmlDecl(parser, 0, s, next);
	if (result != XML_ERROR_NONE)
	  return result;
      }
      break;
    case XML_ROLE_DOCTYPE_SYSTEM_ID:
      hadExternalDoctype = 1;
      break;
    case XML_ROLE_DOCTYPE_PUBLIC_ID:
    case XML_ROLE_ENTITY_PUBLIC_ID:
      if (!XmlIsPublicId(encoding, s, next, &eventPtr))
	return XML_ERROR_SYNTAX;
      if (declEntity) {
	XML_Char *tem = poolStoreString(&dtd.pool,
	                                encoding,
					s + encoding->minBytesPerChar,
	  				next - encoding->minBytesPerChar);
	if (!tem)
	  return XML_ERROR_NO_MEMORY;
	normalizePublicId(tem);
	declEntity->publicId = tem;
	poolFinish(&dtd.pool);
      }
      break;
    case XML_ROLE_INSTANCE_START:
      processor = contentProcessor;
      if (hadExternalDoctype)
	dtd.complete = 0;
      return contentProcessor(parser, s, end, nextPtr);
    case XML_ROLE_ATTLIST_ELEMENT_NAME:
      {
	const XML_Char *name = poolStoreString(&dtd.pool, encoding, s, next);
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
	const XML_Char *attVal;
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
	declEntity->base = dtd.base;
	poolFinish(&dtd.pool);
      }
      break;
    case XML_ROLE_ENTITY_NOTATION_NAME:
      if (declEntity) {
	declEntity->notation = poolStoreString(&dtd.pool, encoding, s, next);
	if (!declEntity->notation)
	  return XML_ERROR_NO_MEMORY;
	poolFinish(&dtd.pool);
	if (unparsedEntityDeclHandler) {
	  eventPtr = eventEndPtr = s;
	  unparsedEntityDeclHandler(handlerArg,
				    declEntity->name,
				    declEntity->base,
				    declEntity->systemId,
				    declEntity->publicId,
				    declEntity->notation);
	}

      }
      break;
    case XML_ROLE_GENERAL_ENTITY_NAME:
      {
	const XML_Char *name;
	if (XmlPredefinedEntityName(encoding, s, next)) {
	  declEntity = 0;
	  break;
	}
	name = poolStoreString(&dtd.pool, encoding, s, next);
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
    case XML_ROLE_NOTATION_NAME:
      declNotationPublicId = 0;
      declNotationName = 0;
      if (notationDeclHandler) {
	declNotationName = poolStoreString(&tempPool, encoding, s, next);
	if (!declNotationName)
	  return XML_ERROR_NO_MEMORY;
	poolFinish(&tempPool);
      }
      break;
    case XML_ROLE_NOTATION_PUBLIC_ID:
      if (!XmlIsPublicId(encoding, s, next, &eventPtr))
	return XML_ERROR_SYNTAX;
      if (declNotationName) {
	XML_Char *tem = poolStoreString(&tempPool,
	                                encoding,
					s + encoding->minBytesPerChar,
	  				next - encoding->minBytesPerChar);
	if (!tem)
	  return XML_ERROR_NO_MEMORY;
	normalizePublicId(tem);
	declNotationPublicId = tem;
	poolFinish(&tempPool);
      }
      break;
    case XML_ROLE_NOTATION_SYSTEM_ID:
      if (declNotationName && notationDeclHandler) {
	const XML_Char *systemId
	  = poolStoreString(&tempPool, encoding,
			    s + encoding->minBytesPerChar,
	  		    next - encoding->minBytesPerChar);
	if (!systemId)
	  return XML_ERROR_NO_MEMORY;
	eventPtr = eventEndPtr = s;
	notationDeclHandler(handlerArg,
			    declNotationName,
			    dtd.base,
			    systemId,
			    declNotationPublicId);
      }
      poolClear(&tempPool);
      break;
    case XML_ROLE_NOTATION_NO_SYSTEM_ID:
      if (declNotationPublicId && notationDeclHandler) {
	eventPtr = eventEndPtr = s;
	notationDeclHandler(handlerArg,
			    declNotationName,
			    dtd.base,
			    0,
			    declNotationPublicId);
      }
      poolClear(&tempPool);
      break;
    case XML_ROLE_ERROR:
      eventPtr = s;
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
	eventPtr = s;
	return XML_ERROR_SYNTAX;
      }
      groupConnector[prologState.level] = ',';
      break;
    case XML_ROLE_GROUP_CHOICE:
      if (groupConnector[prologState.level] == ',') {
	eventPtr = s;
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
	eventPtr = s;
	eventEndPtr = next;
	if (!reportProcessingInstruction(parser, encoding, s, next))
	  return XML_ERROR_NO_MEMORY;
	break;
      }
      break;
    }
    if (defaultHandler) {
      switch (tok) {
      case XML_TOK_PI:
      case XML_TOK_BOM:
      case XML_TOK_XML_DECL:
	break;
      default:
	eventPtr = s;
	eventEndPtr = next;
	reportDefault(parser, encoding, s, next);
      }
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
  eventPtr = s;
  for (;;) {
    const char *next;
    int tok = XmlPrologTok(encoding, s, end, &next);
    eventEndPtr = next;
    switch (tok) {
    case XML_TOK_TRAILING_CR:
      if (defaultHandler) {
	eventEndPtr = end;
	reportDefault(parser, encoding, s, end);
      }
      /* fall through */
    case XML_TOK_NONE:
      if (nextPtr)
	*nextPtr = end;
      return XML_ERROR_NONE;
    case XML_TOK_PROLOG_S:
    case XML_TOK_COMMENT:
      if (defaultHandler)
	reportDefault(parser, encoding, s, next);
      break;
    case XML_TOK_PI:
      if (!reportProcessingInstruction(parser, encoding, s, next))
	return XML_ERROR_NO_MEMORY;
      break;
    case XML_TOK_INVALID:
      eventPtr = next;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_PARTIAL:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      return XML_ERROR_UNCLOSED_TOKEN;
    case XML_TOK_PARTIAL_CHAR:
      if (nextPtr) {
	*nextPtr = s;
	return XML_ERROR_NONE;
      }
      return XML_ERROR_PARTIAL_CHAR;
    default:
      return XML_ERROR_JUNK_AFTER_DOC_ELEMENT;
    }
    eventPtr = s = next;
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
  if (!isCdata && poolLength(pool) && poolLastChar(pool) == XML_T(' '))
    poolChop(pool);
  if (!poolAppendChar(pool, XML_T('\0')))
    return XML_ERROR_NO_MEMORY;
  return XML_ERROR_NONE;
}

static enum XML_Error
appendAttributeValue(XML_Parser parser, const ENCODING *enc, int isCdata,
		     const char *ptr, const char *end,
		     STRING_POOL *pool)
{
  const ENCODING *internalEnc = XmlGetInternalEncoding();
  for (;;) {
    const char *next;
    int tok = XmlAttributeValueTok(enc, ptr, end, &next);
    switch (tok) {
    case XML_TOK_NONE:
      return XML_ERROR_NONE;
    case XML_TOK_INVALID:
      if (enc == encoding)
	eventPtr = next;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_PARTIAL:
      if (enc == encoding)
	eventPtr = ptr;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_CHAR_REF:
      {
	XML_Char buf[XML_ENCODE_MAX];
	int i;
	int n = XmlCharRefNumber(enc, ptr);
	if (n < 0) {
	  if (enc == encoding)
	    eventPtr = ptr;
      	  return XML_ERROR_BAD_CHAR_REF;
	}
	if (!isCdata
	    && n == 0x20 /* space */
	    && (poolLength(pool) == 0 || poolLastChar(pool) == XML_T(' ')))
	  break;
	n = XmlEncode(n, (ICHAR *)buf);
	if (!n) {
	  if (enc == encoding)
	    eventPtr = ptr;
	  return XML_ERROR_BAD_CHAR_REF;
	}
	for (i = 0; i < n; i++) {
	  if (!poolAppendChar(pool, buf[i]))
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
      if (!isCdata && (poolLength(pool) == 0 || poolLastChar(pool) == XML_T(' ')))
	break;
      if (!poolAppendChar(pool, XML_T(' ')))
	return XML_ERROR_NO_MEMORY;
      break;
    case XML_TOK_ENTITY_REF:
      {
	const XML_Char *name;
	ENTITY *entity;
	XML_Char ch = XmlPredefinedEntityName(enc,
					      ptr + enc->minBytesPerChar,
					      next - enc->minBytesPerChar);
	if (ch) {
	  if (!poolAppendChar(pool, ch))
  	    return XML_ERROR_NO_MEMORY;
	  break;
	}
	name = poolStoreString(&temp2Pool, enc,
			       ptr + enc->minBytesPerChar,
			       next - enc->minBytesPerChar);
	if (!name)
	  return XML_ERROR_NO_MEMORY;
	entity = (ENTITY *)lookup(&dtd.generalEntities, name, 0);
	poolDiscard(&temp2Pool);
	if (!entity) {
	  if (dtd.complete) {
	    if (enc == encoding)
	      eventPtr = ptr;
	    return XML_ERROR_UNDEFINED_ENTITY;
	  }
	}
	else if (entity->open) {
	  if (enc == encoding)
	    eventPtr = ptr;
	  return XML_ERROR_RECURSIVE_ENTITY_REF;
	}
	else if (entity->notation) {
	  if (enc == encoding)
	    eventPtr = ptr;
	  return XML_ERROR_BINARY_ENTITY_REF;
	}
	else if (!entity->textPtr) {
	  if (enc == encoding)
	    eventPtr = ptr;
  	  return XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF;
	}
	else {
	  enum XML_Error result;
	  const XML_Char *textEnd = entity->textPtr + entity->textLen;
	  entity->open = 1;
	  result = appendAttributeValue(parser, internalEnc, isCdata, (char *)entity->textPtr, (char *)textEnd, pool);
	  entity->open = 0;
	  if (result)
	    return result;
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
  const ENCODING *internalEnc = XmlGetInternalEncoding();
  STRING_POOL *pool = &(dtd.pool);
  entityTextPtr += encoding->minBytesPerChar;
  entityTextEnd -= encoding->minBytesPerChar;
  for (;;) {
    const char *next;
    int tok = XmlEntityValueTok(encoding, entityTextPtr, entityTextEnd, &next);
    switch (tok) {
    case XML_TOK_PARAM_ENTITY_REF:
      eventPtr = entityTextPtr;
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
      *(pool->ptr)++ = XML_T('\n');
      break;
    case XML_TOK_CHAR_REF:
      {
	XML_Char buf[XML_ENCODE_MAX];
	int i;
	int n = XmlCharRefNumber(encoding, entityTextPtr);
	if (n < 0) {
	  eventPtr = entityTextPtr;
	  return XML_ERROR_BAD_CHAR_REF;
	}
	n = XmlEncode(n, (ICHAR *)buf);
	if (!n) {
	  eventPtr = entityTextPtr;
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
      eventPtr = entityTextPtr;
      return XML_ERROR_INVALID_TOKEN;
    case XML_TOK_INVALID:
      eventPtr = next;
      return XML_ERROR_INVALID_TOKEN;
    default:
      abort();
    }
    entityTextPtr = next;
  }
  /* not reached */
}

static void
normalizeLines(XML_Char *s)
{
  XML_Char *p;
  for (;; s++) {
    if (*s == XML_T('\0'))
      return;
    if (*s == XML_T('\r'))
      break;
  }
  p = s;
  do {
    if (*s == XML_T('\r')) {
      *p++ = XML_T('\n');
      if (*++s == XML_T('\n'))
        s++;
    }
    else
      *p++ = *s++;
  } while (*s);
  *p = XML_T('\0');
}

static int
reportProcessingInstruction(XML_Parser parser, const ENCODING *enc, const char *start, const char *end)
{
  const XML_Char *target;
  XML_Char *data;
  const char *tem;
  if (!processingInstructionHandler) {
    if (defaultHandler)
      reportDefault(parser, enc, start, end);
    return 1;
  }
  start += enc->minBytesPerChar * 2;
  tem = start + XmlNameLength(enc, start);
  target = poolStoreString(&tempPool, enc, start, tem);
  if (!target)
    return 0;
  poolFinish(&tempPool);
  data = poolStoreString(&tempPool, enc,
			XmlSkipS(enc, tem),
			end - enc->minBytesPerChar*2);
  if (!data)
    return 0;
  normalizeLines(data);
  processingInstructionHandler(handlerArg, target, data);
  poolClear(&tempPool);
  return 1;
}

static void
reportDefault(XML_Parser parser, const ENCODING *enc, const char *s, const char *end)
{
  if (MUST_CONVERT(enc, s)) {
    for (;;) {
      ICHAR *dataPtr = (ICHAR *)dataBuf;
      XmlConvert(enc, &s, end, &dataPtr, (ICHAR *)dataBufEnd);
      if (s == end) {
	defaultHandler(handlerArg, dataBuf, dataPtr - (ICHAR *)dataBuf);
	break;
      }
      if (enc == encoding) {
	eventEndPtr = s;
	defaultHandler(handlerArg, dataBuf, dataPtr - (ICHAR *)dataBuf);
	eventPtr = s;
      }
      else
	defaultHandler(handlerArg, dataBuf, dataPtr - (ICHAR *)dataBuf);
    }
  }
  else
    defaultHandler(handlerArg, (XML_Char *)s, (XML_Char *)end - (XML_Char *)s);
}


static int
defineAttribute(ELEMENT_TYPE *type, ATTRIBUTE_ID *attId, int isCdata, const XML_Char *value)
{
  DEFAULT_ATTRIBUTE *att;
  if (type->nDefaultAtts == type->allocDefaultAtts) {
    if (type->allocDefaultAtts == 0) {
      type->allocDefaultAtts = 8;
      type->defaultAtts = malloc(type->allocDefaultAtts*sizeof(DEFAULT_ATTRIBUTE));
    }
    else {
      type->allocDefaultAtts *= 2;
      type->defaultAtts = realloc(type->defaultAtts,
				  type->allocDefaultAtts*sizeof(DEFAULT_ATTRIBUTE));
    }
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
  const XML_Char *name;
  if (!poolAppendChar(&dtd.pool, XML_T('\0')))
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

static
const XML_Char *getOpenEntityNames(XML_Parser parser)
{
  HASH_TABLE_ITER iter;

  hashTableIterInit(&iter, &(dtd.generalEntities));
  for (;;) {
    const XML_Char *s;
    ENTITY *e = (ENTITY *)hashTableIterNext(&iter);
    if (!e)
      break;
    if (!e->open)
      continue;
    if (poolLength(&tempPool) > 0 && !poolAppendChar(&tempPool, XML_T(' ')))
      return 0;
    for (s = e->name; *s; s++)
      if (!poolAppendChar(&tempPool, *s))
        return 0;
  }

  if (!poolAppendChar(&tempPool, XML_T('\0')))
    return 0;
  return tempPool.start;
}

static
int setOpenEntityNames(XML_Parser parser, const XML_Char *openEntityNames)
{
  const XML_Char *s = openEntityNames;
  while (*openEntityNames != XML_T('\0')) {
    if (*s == XML_T(' ') || *s == XML_T('\0')) {
      ENTITY *e;
      if (!poolAppendChar(&tempPool, XML_T('\0')))
	return 0;
      e = (ENTITY *)lookup(&dtd.generalEntities, poolStart(&tempPool), 0);
      if (e)
	e->open = 1;
      if (*s == XML_T(' '))
	s++;
      openEntityNames = s;
      poolDiscard(&tempPool);
    }
    else {
      if (!poolAppendChar(&tempPool, *s))
	return 0;
      s++;
    }
  }
  return 1;
}


static
void normalizePublicId(XML_Char *publicId)
{
  XML_Char *p = publicId;
  XML_Char *s;
  for (s = publicId; *s; s++) {
    switch (*s) {
    case XML_T(' '):
    case XML_T('\r'):
    case XML_T('\n'):
      if (p != publicId && p[-1] != XML_T(' '))
	*p++ = XML_T(' ');
      break;
    default:
      *p++ = *s;
    }
  }
  if (p != publicId && p[-1] == XML_T(' '))
    --p;
  *p = XML_T('\0');
}

static int dtdInit(DTD *p)
{
  poolInit(&(p->pool));
  hashTableInit(&(p->generalEntities));
  hashTableInit(&(p->elementTypes));
  hashTableInit(&(p->attributeIds));
  p->complete = 1;
  p->base = 0;
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
    if (e->allocDefaultAtts != 0)
      free(e->defaultAtts);
  }
  hashTableDestroy(&(p->generalEntities));
  hashTableDestroy(&(p->elementTypes));
  hashTableDestroy(&(p->attributeIds));
  poolDestroy(&(p->pool));
}

/* Do a deep copy of the DTD.  Return 0 for out of memory; non-zero otherwise.
The new DTD has already been initialized. */

static int dtdCopy(DTD *newDtd, const DTD *oldDtd)
{
  HASH_TABLE_ITER iter;

  if (oldDtd->base) {
    const XML_Char *tem = poolCopyString(&(newDtd->pool), oldDtd->base);
    if (!tem)
      return 0;
    newDtd->base = tem;
  }

  hashTableIterInit(&iter, &(oldDtd->attributeIds));

  /* Copy the attribute id table. */

  for (;;) {
    ATTRIBUTE_ID *newA;
    const XML_Char *name;
    const ATTRIBUTE_ID *oldA = (ATTRIBUTE_ID *)hashTableIterNext(&iter);

    if (!oldA)
      break;
    /* Remember to allocate the scratch byte before the name. */
    if (!poolAppendChar(&(newDtd->pool), XML_T('\0')))
      return 0;
    name = poolCopyString(&(newDtd->pool), oldA->name);
    if (!name)
      return 0;
    ++name;
    newA = (ATTRIBUTE_ID *)lookup(&(newDtd->attributeIds), name, sizeof(ATTRIBUTE_ID));
    if (!newA)
      return 0;
    newA->maybeTokenized = oldA->maybeTokenized;
  }

  /* Copy the element type table. */

  hashTableIterInit(&iter, &(oldDtd->elementTypes));

  for (;;) {
    int i;
    ELEMENT_TYPE *newE;
    const XML_Char *name;
    const ELEMENT_TYPE *oldE = (ELEMENT_TYPE *)hashTableIterNext(&iter);
    if (!oldE)
      break;
    name = poolCopyString(&(newDtd->pool), oldE->name);
    if (!name)
      return 0;
    newE = (ELEMENT_TYPE *)lookup(&(newDtd->elementTypes), name, sizeof(ELEMENT_TYPE));
    if (!newE)
      return 0;
    newE->defaultAtts = (DEFAULT_ATTRIBUTE *)malloc(oldE->nDefaultAtts * sizeof(DEFAULT_ATTRIBUTE));
    if (!newE->defaultAtts)
      return 0;
    newE->allocDefaultAtts = newE->nDefaultAtts = oldE->nDefaultAtts;
    for (i = 0; i < newE->nDefaultAtts; i++) {
      newE->defaultAtts[i].id = (ATTRIBUTE_ID *)lookup(&(newDtd->attributeIds), oldE->defaultAtts[i].id->name, 0);
      newE->defaultAtts[i].isCdata = oldE->defaultAtts[i].isCdata;
      newE->defaultAtts[i].value = poolCopyString(&(newDtd->pool), oldE->defaultAtts[i].value);
      if (!newE->defaultAtts[i].value)
	return 0;
    }
  }

  /* Copy the entity table. */

  hashTableIterInit(&iter, &(oldDtd->generalEntities));

  for (;;) {
    ENTITY *newE;
    const XML_Char *name;
    const ENTITY *oldE = (ENTITY *)hashTableIterNext(&iter);
    if (!oldE)
      break;
    name = poolCopyString(&(newDtd->pool), oldE->name);
    if (!name)
      return 0;
    newE = (ENTITY *)lookup(&(newDtd->generalEntities), name, sizeof(ENTITY));
    if (!newE)
      return 0;
    if (oldE->systemId) {
      const XML_Char *tem = poolCopyString(&(newDtd->pool), oldE->systemId);
      if (!tem)
	return 0;
      newE->systemId = tem;
      if (oldE->base) {
	if (oldE->base == oldDtd->base)
	  newE->base = newDtd->base;
	tem = poolCopyString(&(newDtd->pool), oldE->base);
	if (!tem)
	  return 0;
	newE->base = tem;
      }
    }
    else {
      const XML_Char *tem = poolCopyStringN(&(newDtd->pool), oldE->textPtr, oldE->textLen);
      if (!tem)
	return 0;
      newE->textPtr = tem;
      newE->textLen = oldE->textLen;
    }
    if (oldE->notation) {
      const XML_Char *tem = poolCopyString(&(newDtd->pool), oldE->notation);
      if (!tem)
	return 0;
      newE->notation = tem;
    }
  }

  newDtd->complete = oldDtd->complete;
  newDtd->standalone = oldDtd->standalone;
  return 1;
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
XML_Char *poolAppend(STRING_POOL *pool, const ENCODING *enc,
		     const char *ptr, const char *end)
{
  if (!pool->ptr && !poolGrow(pool))
    return 0;
  for (;;) {
    XmlConvert(enc, &ptr, end, (ICHAR **)&(pool->ptr), (ICHAR *)pool->end);
    if (ptr == end)
      break;
    if (!poolGrow(pool))
      return 0;
  }
  return pool->start;
}

static const XML_Char *poolCopyString(STRING_POOL *pool, const XML_Char *s)
{
  do {
    if (!poolAppendChar(pool, *s))
      return 0;
  } while (*s++);
  s = pool->start;
  poolFinish(pool);
  return s;
}

static const XML_Char *poolCopyStringN(STRING_POOL *pool, const XML_Char *s, int n)
{
  if (!pool->ptr && !poolGrow(pool))
    return 0;
  for (; n > 0; --n, s++) {
    if (!poolAppendChar(pool, *s))
      return 0;

  }
  s = pool->start;
  poolFinish(pool);
  return s;
}

static
XML_Char *poolStoreString(STRING_POOL *pool, const ENCODING *enc,
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
      memcpy(pool->blocks->s, pool->start, (pool->end - pool->start) * sizeof(XML_Char));
      pool->ptr = pool->blocks->s + (pool->ptr - pool->start);
      pool->start = pool->blocks->s;
      pool->end = pool->start + pool->blocks->size;
      return 1;
    }
  }
  if (pool->blocks && pool->start == pool->blocks->s) {
    int blockSize = (pool->end - pool->start)*2;
    pool->blocks = realloc(pool->blocks, offsetof(BLOCK, s) + blockSize * sizeof(XML_Char));
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
    tem = malloc(offsetof(BLOCK, s) + blockSize * sizeof(XML_Char));
    if (!tem)
      return 0;
    tem->size = blockSize;
    tem->next = pool->blocks;
    pool->blocks = tem;
    memcpy(tem->s, pool->start, (pool->ptr - pool->start) * sizeof(XML_Char));
    pool->ptr = tem->s + (pool->ptr - pool->start);
    pool->start = tem->s;
    pool->end = tem->s + blockSize;
  }
  return 1;
}
