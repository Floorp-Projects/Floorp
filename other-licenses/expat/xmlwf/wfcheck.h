
#include <stddef.h>

enum WfCheckResult {
  wellFormed,
  noMemory,
  syntaxError,
  noElements,
  invalidToken,
  unclosedToken,
  partialChar,
  tagMismatch,
  duplicateAttribute,
  junkAfterDocElement,
  paramEntityRef,
  undefinedEntity,
  recursiveEntityRef,
  asyncEntity,
  badCharRef,
  binaryEntityRef,
  attributeExternalEntityRef,
  misplacedXmlPi,
  unknownEncoding,
  incorrectEncoding
};

enum EntityType {
  documentEntity,
  generalTextEntity
};

enum WfCheckResult wfCheck(enum EntityType entityType,
			   const char *s, size_t n,
			   const char **errorPtr,
			   unsigned long *errorLineNumber,
			   unsigned long *errorColNumber);
const char *wfCheckMessage(enum WfCheckResult);

