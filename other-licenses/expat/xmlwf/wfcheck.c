#include <stdlib.h>
#include <string.h>

#include "wfcheck.h"
#include "hashtable.h"

#include "xmltok.h"
#include "xmlrole.h"

typedef struct {
  const char *name;
  const char *textPtr;
  size_t textLen;
  const char *docTextPtr;
  const char *systemId;
  const char *publicId;
  const char *notation;
  char open;
  char wfInContent;
  char wfInAttribute;
  char magic;
} ENTITY;

#define INIT_BLOCK_SIZE 1024

typedef struct block {
  struct block *next;
  char s[1];
} BLOCK;

typedef struct {
  BLOCK *blocks;
  const char *end;
  char *ptr;
  char *start;
} STRING_POOL;

typedef struct {
  HASH_TABLE generalEntities;
  HASH_TABLE paramEntities;
  STRING_POOL pool;
  int containsRef;
  int standalone;
  char *groupConnector;
  size_t groupSize;
} DTD;

typedef struct {
  DTD dtd;
  size_t stackSize;
  const char **startName;
  int attsSize;
  ATTRIBUTE *atts;
} CONTEXT;

static void poolInit(STRING_POOL *);
static void poolDestroy(STRING_POOL *);
static const char *poolAppend(STRING_POOL *pool, const ENCODING *enc,
			      const char *ptr, const char *end);
static const char *poolStoreString(STRING_POOL *pool, const ENCODING *enc,
			    const char *ptr, const char *end);
static int poolGrow(STRING_POOL *);
static int dtdInit(DTD *);
static void dtdDestroy(DTD *);
static int contextInit(CONTEXT *);
static void contextDestroy(CONTEXT *);

#define poolStart(pool) ((pool)->start)
#define poolDiscard(pool) ((pool)->ptr = (pool)->start)
#define poolFinish(pool) ((pool)->start = (pool)->ptr)

static enum WfCheckResult
checkProlog(DTD *, const char *s, const char *end, const char **, const ENCODING **enc);
static enum WfCheckResult
checkContent(size_t level, CONTEXT *context, const ENCODING *enc,
  	     const char *s, const char *end, const char **badPtr);
static enum WfCheckResult
checkGeneralTextEntity(CONTEXT *context,
		       const char *s, const char *end,
		       const char **nextPtr,
		       const ENCODING **enc);
static enum WfCheckResult
checkAttributeValue(DTD *, const ENCODING *, const char *, const char *, const char **);
static enum WfCheckResult
checkAttributeUniqueness(CONTEXT *context, const ENCODING *enc, int nAtts,
			 const char **badPtr);
static enum WfCheckResult
checkParsedEntities(CONTEXT *context, const char **badPtr);

static
enum WfCheckResult storeEntity(DTD *dtd,
			       const ENCODING *enc,
			       int isParam,
			       const char *entityNamePtr,
			       const char *entityNameEnd,
			       const char *entityTextPtr,
			       const char *entityTextEnd,
			       const char **badPtr);


enum WfCheckResult
wfCheck(enum EntityType entityType, const char *s, size_t n,
	const char **badPtr, unsigned long *badLine, unsigned long *badCol)
{
  CONTEXT context;
  const ENCODING *enc;
  const char *start = s;
  const char *end = s + n;
  const char *next = 0;
  enum WfCheckResult result;

  if (!contextInit(&context)) {
    contextDestroy(&context);
    return noMemory;
  }
  if (entityType == documentEntity) {
    result = checkProlog(&context.dtd, s, end, &next, &enc);
    s = next;
    if (!result) {
      result = checkParsedEntities(&context, &next);
      s = next;
      if (!result) {
	result = checkContent(0, &context, enc, s, end, &next);
	s = next;
      }
    }
  }
  else {
    result = checkGeneralTextEntity(&context, s, end, &next, &enc);
    s = next;
  }
  if (result && s) {
    POSITION pos;
    memset(&pos, 0, sizeof(POSITION));
    XmlUpdatePosition(enc, start, s, &pos);
    *badPtr = s;
    *badLine = pos.lineNumber;
    *badCol = pos.columnNumber;
  }
  contextDestroy(&context);
  return result;
}

static
int contextInit(CONTEXT *p)
{
  p->stackSize = 1024;
  p->startName = malloc(p->stackSize * sizeof(char *));
  p->attsSize = 1024;
  p->atts = malloc(p->attsSize * sizeof(ATTRIBUTE));
  return dtdInit(&(p->dtd)) && p->atts && p->startName;
}

static
void contextDestroy(CONTEXT *p)
{
  dtdDestroy(&(p->dtd));
  free((void *)p->startName);
  free((void *)p->atts);
}

static enum WfCheckResult
checkContent(size_t level, CONTEXT *context, const ENCODING *enc,
	     const char *s, const char *end, const char **badPtr)
{
  size_t startLevel = level;
  const char *next;
  int tok = XmlContentTok(enc, s, end, &next);
  for (;;) {
    switch (tok) {
    case XML_TOK_TRAILING_CR:
    case XML_TOK_NONE:
      if (startLevel > 0) {
	if (level != startLevel) {
	  *badPtr = s;
	  return asyncEntity;
        }
	return wellFormed;
      }
      *badPtr = s;
      return noElements;
    case XML_TOK_INVALID:
      *badPtr = next;
      return invalidToken;
    case XML_TOK_PARTIAL:
      *badPtr = s;
      return unclosedToken;
    case XML_TOK_PARTIAL_CHAR:
      *badPtr = s;
      return partialChar;
    case XML_TOK_EMPTY_ELEMENT_NO_ATTS:
      break;
    case XML_TOK_ENTITY_REF:
      {
	const char *name = poolStoreString(&context->dtd.pool, enc,
					   s + enc->minBytesPerChar,
					   next - enc->minBytesPerChar);
	ENTITY *entity = (ENTITY *)lookup(&context->dtd.generalEntities, name, 0);
	poolDiscard(&context->dtd.pool);
	if (!entity) {
	  if (!context->dtd.containsRef || context->dtd.standalone) {
	    *badPtr = s;
	    return undefinedEntity;
	  }
	  break;
	}
	if (entity->wfInContent)
	  break;
	if (entity->open) {
	  *badPtr = s;
	  return recursiveEntityRef;
	}
	if (entity->notation) {
	  *badPtr = s;
	  return binaryEntityRef;
	}
	if (entity) {
	  if (entity->textPtr) {
	    enum WfCheckResult result;
	    const ENCODING *internalEnc = XmlGetInternalEncoding(XML_UTF8_ENCODING);
	    entity->open = 1;
	    result = checkContent(level, context, internalEnc,
				  entity->textPtr, entity->textPtr + entity->textLen,
				  badPtr);
	    entity->open = 0;
	    if (result && *badPtr) {
	      *badPtr = s;
	      return result;
	    }
	    entity->wfInContent = 1;
	  }
	}
	break;
      }
    case XML_TOK_START_TAG_NO_ATTS:
      if (level == context->stackSize) {
	context->startName
	  = realloc((void *)context->startName, (context->stackSize *= 2) * sizeof(char *));
	if (!context->startName)
	  return noMemory;
      }
      context->startName[level++] = s + enc->minBytesPerChar;
      break;
    case XML_TOK_START_TAG_WITH_ATTS:
      if (level == context->stackSize) {
	context->startName = realloc((void *)context->startName, (context->stackSize *= 2) * sizeof(char *));
	if (!context->startName)
	  return noMemory;
      }
      context->startName[level++] = s + enc->minBytesPerChar;
      /* fall through */
    case XML_TOK_EMPTY_ELEMENT_WITH_ATTS:
      {
	int i;
	int n = XmlGetAttributes(enc, s, context->attsSize, context->atts);
	if (n > context->attsSize) {
	  context->attsSize = 2*n;
	  context->atts = realloc((void *)context->atts, context->attsSize * sizeof(ATTRIBUTE));
	  if (!context->atts)
	    return noMemory;
	  XmlGetAttributes(enc, s, n, context->atts);
	}
	for (i = 0; i < n; i++) {
	  if (!context->atts[i].normalized) {
	    enum WfCheckResult result
	      = checkAttributeValue(&context->dtd, enc,
			  	    context->atts[i].valuePtr,
				    context->atts[i].valueEnd,
				    badPtr);
	    if (result)
	      return result;
	  }
	}
	if (i > 1) {
	  enum WfCheckResult result = checkAttributeUniqueness(context, enc, n, badPtr);
	  if (result)
	    return result;
	}
      }
      break;
    case XML_TOK_END_TAG:
      if (level == startLevel) {
        *badPtr = s;
        return asyncEntity;
      }
      --level;
      if (!XmlSameName(enc, context->startName[level], s + enc->minBytesPerChar * 2)) {
	*badPtr = s;
	return tagMismatch;
      }
      break;
    case XML_TOK_CHAR_REF:
      if (XmlCharRefNumber(enc, s) < 0) {
	*badPtr = s;
	return badCharRef;
      }
      break;
    case XML_TOK_XML_DECL:
      *badPtr = s;
      return misplacedXmlPi;
    }
    s = next;
    if (level == 0) {
      do {
	tok = XmlPrologTok(enc, s, end, &next);
	switch (tok) {
	case XML_TOK_TRAILING_CR:
	case XML_TOK_NONE:
	  return wellFormed;
	case XML_TOK_PROLOG_S:
	case XML_TOK_COMMENT:
	case XML_TOK_PI:
	  s = next;
	  break;
	default:
	  if (tok > 0) {
	    *badPtr = s;
	    return junkAfterDocElement;
	  }
	  break;
	}
      } while (tok > 0);
    }
    else
      tok = XmlContentTok(enc, s, end, &next);
  }
  /* not reached */
}

static
int attcmp(const void *p1, const void *p2)
{
  const ATTRIBUTE *a1 = p1;
  const ATTRIBUTE *a2 = p2;
  size_t n1 = a1->valuePtr - a1->name;
  size_t n2 = a2->valuePtr - a2->name;

  if (n1 == n2) {
    int n = memcmp(a1->name, a2->name, n1);
    if (n)
      return n;
    /* Sort identical attribute names by position, so that we always
       report the first duplicate attribute. */
    if (a1->name < a2->name)
      return -1;
    else if (a1->name > a2->name)
      return 1;
    else
      return 0;
  }
  else if (n1 < n2)
    return -1;
  else
    return 1;
}

/* Note that this trashes the attribute values. */

static enum WfCheckResult
checkAttributeUniqueness(CONTEXT *context, const ENCODING *enc, int nAtts,
			 const char **badPtr)
{
#define QSORT_MIN_ATTS 10
  if (nAtts < QSORT_MIN_ATTS) {
    int i;
    for (i = 1; i < nAtts; i++) {
      int j;
      for (j = 0; j < i; j++) {
	if (XmlSameName(enc, context->atts[i].name, context->atts[j].name)) {
	  *badPtr = context->atts[i].name;
  	  return duplicateAttribute;
	}
      }
    }
  }
  else {
    int i;
    const char *dup = 0;
    /* Store the end of the name in valuePtr */
    for (i = 0; i < nAtts; i++) {
      ATTRIBUTE *a = context->atts + i;
      a->valuePtr = a->name + XmlNameLength(enc, a->name);
    }
    qsort(context->atts, nAtts, sizeof(ATTRIBUTE), attcmp);
    for (i = 1; i < nAtts; i++) {
      ATTRIBUTE *a = context->atts + i;
      if (XmlSameName(enc, a->name, a[-1].name)) {
	if (!dup || a->name < dup)
	  dup = a->name;
      }
    }
    if (dup) {
      *badPtr = dup;
      return duplicateAttribute;
    }
  }
  return wellFormed;
}

static enum WfCheckResult
checkProlog(DTD *dtd, const char *s, const char *end,
	    const char **nextPtr, const ENCODING **enc)
{
  const char *entityNamePtr, *entityNameEnd;
  int entityIsParam;
  PROLOG_STATE state;
  ENTITY *entity;
  INIT_ENCODING initEnc;
  XmlInitEncoding(&initEnc, enc);
  XmlPrologStateInit(&state);
  for (;;) {
    const char *next;
    int tok = XmlPrologTok(*enc, s, end, &next);
    switch (XmlTokenRole(&state, tok, s, next, *enc)) {
    case XML_ROLE_XML_DECL:
      {
	const char *encodingName = 0;
	const ENCODING *encoding = 0;
	const char *version;
	int standalone = -1;
	if (!XmlParseXmlDecl(0,
			     *enc,
			     s,
			     next,
			     nextPtr,
			     &version,
			     &encodingName,
			     &encoding,
			     &standalone))
	  return syntaxError;
	if (encoding) {
	  if (encoding->minBytesPerChar != (*enc)->minBytesPerChar) {
	    *nextPtr = encodingName;
	    return incorrectEncoding;
	  }
	  *enc = encoding;
	}
	else if (encodingName) {
	  *nextPtr = encodingName;
	  return unknownEncoding;
	}
	if (standalone == 1)
	  dtd->standalone = 1;
	break;
      }
    case XML_ROLE_DOCTYPE_SYSTEM_ID:
      dtd->containsRef = 1;
      break;
    case XML_ROLE_DOCTYPE_PUBLIC_ID:
    case XML_ROLE_ENTITY_PUBLIC_ID:
    case XML_ROLE_NOTATION_PUBLIC_ID:
      if (!XmlIsPublicId(*enc, s, next, nextPtr))
	return syntaxError;
      break;
    case XML_ROLE_INSTANCE_START:
      *nextPtr = s;
      return wellFormed;
    case XML_ROLE_DEFAULT_ATTRIBUTE_VALUE:
    case XML_ROLE_FIXED_ATTRIBUTE_VALUE:
      {
	const char *tem = 0;
	enum WfCheckResult result
	  = checkAttributeValue(dtd, *enc, s + (*enc)->minBytesPerChar,
				next - (*enc)->minBytesPerChar,
				&tem);
	if (result) {
	  if (tem)
	    *nextPtr = tem;
	  return result;
	}
	break;
      }
    case XML_ROLE_ENTITY_VALUE:
      {
	enum WfCheckResult result
	  = storeEntity(dtd,
			*enc,
			entityIsParam,
			entityNamePtr,
			entityNameEnd,
			s,
			next,
			nextPtr);
	if (result != wellFormed)
	  return result;
      }
      break;
    case XML_ROLE_ENTITY_SYSTEM_ID:
      {
	const char *name = poolStoreString(&dtd->pool, *enc, entityNamePtr, entityNameEnd);
	entity = (ENTITY *)lookup(entityIsParam ? &dtd->paramEntities : &dtd->generalEntities,
				  name, sizeof(ENTITY));
	if (entity->name != name) {
	  poolDiscard(&dtd->pool);
	  entity = 0;
	}
	else {
	  poolFinish(&dtd->pool);
	  entity->systemId = poolStoreString(&dtd->pool, *enc,
					     s + (*enc)->minBytesPerChar,
					     next - (*enc)->minBytesPerChar);
	  poolFinish(&dtd->pool);
	}
      }
      break;
    case XML_ROLE_PARAM_ENTITY_REF:
      {
	const char *name = poolStoreString(&dtd->pool, *enc,
					   s + (*enc)->minBytesPerChar,
					   next - (*enc)->minBytesPerChar);
	ENTITY *entity = (ENTITY *)lookup(&dtd->paramEntities, name, 0);
	poolDiscard(&dtd->pool);
	if (!entity) {
	  if (!dtd->containsRef || dtd->standalone) {
	    *nextPtr = s;
	    return undefinedEntity;
	  }
	}
      }
      break;
    case XML_ROLE_ENTITY_NOTATION_NAME:
      if (entity) {
	entity->notation = poolStoreString(&dtd->pool, *enc, s, next);
	poolFinish(&dtd->pool);
      }
      break;
    case XML_ROLE_GENERAL_ENTITY_NAME:
      entityNamePtr = s;
      entityNameEnd = next;
      entityIsParam = 0;
      break;
    case XML_ROLE_PARAM_ENTITY_NAME:
      entityNamePtr = s;
      entityNameEnd = next;
      entityIsParam = 1;
      break;
    case XML_ROLE_ERROR:
      *nextPtr = s;
      switch (tok) {
      case XML_TOK_PARAM_ENTITY_REF:
	return paramEntityRef;
      case XML_TOK_INVALID:
	*nextPtr = next;
	return invalidToken;
      case XML_TOK_NONE:
	return noElements;
      case XML_TOK_PARTIAL:
	return unclosedToken;
      case XML_TOK_PARTIAL_CHAR:
	return partialChar;
      case XML_TOK_TRAILING_CR:
	*nextPtr = s + (*enc)->minBytesPerChar;
	return noElements;
      case XML_TOK_XML_DECL:
	return misplacedXmlPi;
      default:
	return syntaxError;
      }
    case XML_ROLE_GROUP_OPEN:
      if (state.level >= dtd->groupSize) {
	if (dtd->groupSize)
	  dtd->groupConnector = realloc(dtd->groupConnector, dtd->groupSize *= 2);
	else
	  dtd->groupConnector = malloc(dtd->groupSize = 32);
	if (!dtd->groupConnector)
	  return noMemory;
      }
      dtd->groupConnector[state.level] = 0;
      break;
    case XML_ROLE_GROUP_SEQUENCE:
      if (dtd->groupConnector[state.level] == '|') {
	*nextPtr = s;
	return syntaxError;
      }
      dtd->groupConnector[state.level] = ',';
      break;
    case XML_ROLE_GROUP_CHOICE:
      if (dtd->groupConnector[state.level] == ',') {
	*nextPtr = s;
	return syntaxError;
      }
      dtd->groupConnector[state.level] = '|';
      break;
    case XML_ROLE_NONE:
      if (tok == XML_TOK_PARAM_ENTITY_REF)
	dtd->containsRef = 1;
      break;
    }
    s = next;
  }
  /* not reached */
}

static enum WfCheckResult
checkParsedEntities(CONTEXT *context, const char **badPtr)
{
  HASH_TABLE_ITER iter;
  hashTableIterInit(&iter, &context->dtd.generalEntities);
  for (;;) {
    ENTITY *entity = (ENTITY *)hashTableIterNext(&iter);
    if (!entity)
      break;
    if (entity->textPtr && !entity->wfInContent && !entity->magic) {
      enum WfCheckResult result;
      const ENCODING *internalEnc = XmlGetInternalEncoding(XML_UTF8_ENCODING);
      entity->open = 1;
      result = checkContent(1, context, internalEnc,
			    entity->textPtr, entity->textPtr + entity->textLen,
			    badPtr);
      entity->open = 0;
      if (result && *badPtr) {
	*badPtr = entity->docTextPtr;
	return result;
      }
      entity->wfInContent = 1;
    }
  }
  return wellFormed;
}

static enum WfCheckResult
checkGeneralTextEntity(CONTEXT *context,
		       const char *s, const char *end,
		       const char **nextPtr,
		       const ENCODING **enc)
{
  INIT_ENCODING initEnc;
  const char *next;
  int tok;

  XmlInitEncoding(&initEnc, enc);
  tok = XmlContentTok(*enc, s, end, &next);

  if (tok == XML_TOK_BOM) {
    s = next;
    tok = XmlContentTok(*enc, s, end, &next);
  }
  if (tok == XML_TOK_XML_DECL) {
    const char *encodingName = 0;
    const ENCODING *encoding = 0;
    const char *version;
    if (!XmlParseXmlDecl(1,
			 *enc,
			 s,
			 next,
			 nextPtr,
			 &version,
			 &encodingName,
			 &encoding,
			 0))
      return syntaxError;
    if (encoding) {
      if (encoding->minBytesPerChar != (*enc)->minBytesPerChar) {
	*nextPtr = encodingName;
	return incorrectEncoding;
      }
      *enc = encoding;
    }
    else if (encodingName) {
      *nextPtr = encodingName;
      return unknownEncoding;
    }
    s = next;
  }
  context->dtd.containsRef = 1;
  return checkContent(1, context, *enc, s, end, nextPtr);
}

static enum WfCheckResult
checkAttributeValue(DTD *dtd, const ENCODING *enc,
		    const char *ptr, const char *end, const char **badPtr)
{
  for (;;) {
    const char *next;
    int tok = XmlAttributeValueTok(enc, ptr, end, &next);
    switch (tok) {
    case XML_TOK_TRAILING_CR:
    case XML_TOK_NONE:
      return wellFormed;
    case XML_TOK_INVALID:
      *badPtr = next;
      return invalidToken;
    case XML_TOK_PARTIAL:
      *badPtr = ptr;
      return invalidToken;
    case XML_TOK_CHAR_REF:
      if (XmlCharRefNumber(enc, ptr) < 0) {
	*badPtr = ptr;
	return badCharRef;
      }
      break;
    case XML_TOK_DATA_CHARS:
    case XML_TOK_DATA_NEWLINE:
      break;
    case XML_TOK_ENTITY_REF:
      {
	const char *name = poolStoreString(&dtd->pool, enc,
					   ptr + enc->minBytesPerChar,
					   next - enc->minBytesPerChar);
	ENTITY *entity = (ENTITY *)lookup(&dtd->generalEntities, name, 0);
	poolDiscard(&dtd->pool);
	if (!entity) {
	  if (!dtd->containsRef) {
	    *badPtr = ptr;
	    return undefinedEntity;
	  }
	  break;
	}
	if (entity->wfInAttribute)
	  break;
	if (entity->open) {
	  *badPtr = ptr;
	  return recursiveEntityRef;
	}
	if (entity->notation) {
	  *badPtr = ptr;
	  return binaryEntityRef;
	}
	if (entity) {
	  if (entity->textPtr) {
	    enum WfCheckResult result;
	    const ENCODING *internalEnc = XmlGetInternalEncoding(XML_UTF8_ENCODING);
	    const char *textEnd = entity->textPtr + entity->textLen;
	    entity->open = 1;
	    result = checkAttributeValue(dtd, internalEnc, entity->textPtr, textEnd, badPtr);
	    entity->open = 0;
	    if (result && *badPtr) {
	      *badPtr = ptr;
	      return result;
	    }
	    entity->wfInAttribute = 1;
	  }
	  else {
	    *badPtr = ptr;
	    return attributeExternalEntityRef;
	  }
	}
	break;
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
void poolInit(STRING_POOL *pool)
{
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
  pool->ptr = 0;
  pool->start = 0;
  pool->end = 0;
}

static
const char *poolAppend(STRING_POOL *pool, const ENCODING *enc,
		       const char *ptr, const char *end)
{
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
const char *poolStoreString(STRING_POOL *pool, const ENCODING *enc,
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
  if (pool->blocks && pool->start == pool->blocks->s) {
    size_t blockSize = (pool->end - pool->start)*2;
    pool->blocks = realloc(pool->blocks, offsetof(BLOCK, s) + blockSize);
    if (!pool->blocks)
      return 0;
    pool->ptr = pool->blocks->s + (pool->ptr - pool->start);
    pool->start = pool->blocks->s;
    pool->end = pool->start + blockSize;
  }
  else {
    BLOCK *tem;
    size_t blockSize = pool->end - pool->start;
    if (blockSize < INIT_BLOCK_SIZE)
      blockSize = INIT_BLOCK_SIZE;
    else
      blockSize *= 2;
    tem = malloc(offsetof(BLOCK, s) + blockSize);
    if (!tem)
      return 0;
    tem->next = pool->blocks;
    pool->blocks = tem;
    memcpy(tem->s, pool->start, pool->ptr - pool->start);
    pool->ptr = tem->s + (pool->ptr - pool->start);
    pool->start = tem->s;
    pool->end = tem->s + blockSize;
  }
  return 1;
}

static int dtdInit(DTD *dtd)
{
  static const char *names[] = { "lt", "amp", "gt", "quot", "apos" };
  static const char chars[] = { '<', '&', '>', '"', '\'' };
  int i;

  poolInit(&(dtd->pool));
  hashTableInit(&(dtd->generalEntities));
  for (i = 0; i < 5; i++) {
    ENTITY *entity = (ENTITY *)lookup(&(dtd->generalEntities), names[i], sizeof(ENTITY));
    if (!entity)
      return 0;
    entity->textPtr = chars + i;
    entity->textLen = 1;
    entity->magic = 1;
    entity->wfInContent = 1;
    entity->wfInAttribute = 1;
  }
  hashTableInit(&(dtd->paramEntities));
  dtd->containsRef = 0;
  dtd->groupSize = 0;
  dtd->groupConnector = 0;
  return 1;
}

static void dtdDestroy(DTD *dtd)
{
  poolDestroy(&(dtd->pool));
  hashTableDestroy(&(dtd->generalEntities));
  hashTableDestroy(&(dtd->paramEntities));
  free(dtd->groupConnector);
}

static
enum WfCheckResult storeEntity(DTD *dtd,
			       const ENCODING *enc,
			       int isParam,
			       const char *entityNamePtr,
			       const char *entityNameEnd,
			       const char *entityTextPtr,
			       const char *entityTextEnd,
			       const char **badPtr)
{
  ENTITY *entity;
  const ENCODING *utf8 = XmlGetInternalEncoding(XML_UTF8_ENCODING);
  STRING_POOL *pool = &(dtd->pool);
  if (!poolStoreString(pool, enc, entityNamePtr, entityNameEnd))
    return noMemory;
  entity = (ENTITY *)lookup(isParam ? &(dtd->paramEntities) : &(dtd->generalEntities),
			    pool->start,
			    sizeof(ENTITY));
  if (entity->name != pool->start) {
    poolDiscard(pool);
    entityNamePtr = 0;
  }
  else
    poolFinish(pool);
  entityTextPtr += enc->minBytesPerChar;
  entityTextEnd -= enc->minBytesPerChar;
  entity->docTextPtr = entityTextPtr;
  for (;;) {
    const char *next;
    int tok = XmlEntityValueTok(enc, entityTextPtr, entityTextEnd, &next);
    switch (tok) {
    case XML_TOK_PARAM_ENTITY_REF:
      *badPtr = entityTextPtr;
      return syntaxError;
    case XML_TOK_NONE:
      if (entityNamePtr) {
	entity->textPtr = pool->start;
	entity->textLen = pool->ptr - pool->start;
	poolFinish(pool);
      }
      else
	poolDiscard(pool);
      return wellFormed;
    case XML_TOK_ENTITY_REF:
    case XML_TOK_DATA_CHARS:
      if (!poolAppend(pool, enc, entityTextPtr, next))
	return noMemory;
      break;
    case XML_TOK_TRAILING_CR:
      next = entityTextPtr + enc->minBytesPerChar;
      /* fall through */
    case XML_TOK_DATA_NEWLINE:
      if (pool->end == pool->ptr && !poolGrow(pool))
	return noMemory;
      *(pool->ptr)++ = '\n';
      break;
    case XML_TOK_CHAR_REF:
      {
	char buf[XML_MAX_BYTES_PER_CHAR];
	int i;
	int n = XmlCharRefNumber(enc, entityTextPtr);
	if (n < 0) {
	  *badPtr = entityTextPtr;
	  return badCharRef;
	}
	n = XmlEncode(utf8, n, buf);
	if (!n) {
	  *badPtr = entityTextPtr;
	  return badCharRef;
	}
	for (i = 0; i < n; i++) {
	  if (pool->end == pool->ptr && !poolGrow(pool))
	    return noMemory;
	  *(pool->ptr)++ = buf[i];
	}
      }
      break;
    case XML_TOK_PARTIAL:
      *badPtr = entityTextPtr;
      return invalidToken;
    case XML_TOK_INVALID:
      *badPtr = next;
      return invalidToken;
    default:
      abort();
    }
    entityTextPtr = next;
  }
  /* not reached */
}
