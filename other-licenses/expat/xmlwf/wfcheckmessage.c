#include "wfcheck.h"

const char *wfCheckMessage(enum WfCheckResult result)
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
    "xml pi not at start of external entity",
    "unknown encoding",
    "encoding specified in XML declaration is incorrect"
  };
  if (result > 0 && result < sizeof(message)/sizeof(message[0]))
    return message[result];
  return 0;
}
