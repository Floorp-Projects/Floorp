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

#ifndef XmlParse_INCLUDED
#define XmlParse_INCLUDED 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XMLPARSEAPI
#define XMLPARSEAPI /* as nothing */
#endif

typedef void *XML_Parser;

/* Constructs a new parser; encoding should be the name of the charset from
the Content-Type header if the Content-Type is text/xml, or null otherwise. */

XML_Parser XMLPARSEAPI
XML_ParserCreate(const char *encoding);

/* Information is UTF-8 encoded. */

/* atts is array of name/value pairs, terminated by NULL;
   names and values are '\0' terminated. */

typedef void (*XML_StartElementHandler)(void *userData,
					const char *name,
					const char **atts);

typedef void (*XML_EndElementHandler)(void *userData,
				      const char *name);

typedef void (*XML_CharacterDataHandler)(void *userData,
					 const char *s,
					 int len);

/* target and data are '\0' terminated */
typedef void (*XML_ProcessingInstructionHandler)(void *userData,
						 const char *target,
						 const char *data);

void XMLPARSEAPI
XML_SetElementHandler(XML_Parser parser,
		      XML_StartElementHandler start,
		      XML_EndElementHandler end);

void XMLPARSEAPI
XML_SetCharacterDataHandler(XML_Parser parser,
			    XML_CharacterDataHandler handler);

void XMLPARSEAPI
XML_SetProcessingInstructionHandler(XML_Parser parser,
				    XML_ProcessingInstructionHandler handler);

/* This value is passed as the userData argument to callbacks. */
void XMLPARSEAPI
XML_SetUserData(XML_Parser parser, void *userData);

/* Parses some input. Returns 0 if a fatal error is detected.
The last call to XML_Parse must have isFinal true;
len may be zero for this call (or any other). */
int XMLPARSEAPI
XML_Parse(XML_Parser parser, const char *s, int len, int isFinal);

void XMLPARSEAPI *
XML_GetBuffer(XML_Parser parser, int len);

int XMLPARSEAPI
XML_ParseBuffer(XML_Parser parser, int len, int isFinal);

/* If XML_Parser or XML_ParseEnd have returned 0, then XML_GetError*
returns information about the error. */

enum XML_Error {
  XML_ERROR_NONE,
  XML_ERROR_NO_MEMORY,
  XML_ERROR_SYNTAX,
  XML_ERROR_NO_ELEMENTS,
  XML_ERROR_INVALID_TOKEN,
  XML_ERROR_UNCLOSED_TOKEN,
  XML_ERROR_PARTIAL_CHAR,
  XML_ERROR_TAG_MISMATCH,
  XML_ERROR_DUPLICATE_ATTRIBUTE,
  XML_ERROR_JUNK_AFTER_DOC_ELEMENT,
  XML_ERROR_PARAM_ENTITY_REF,
  XML_ERROR_UNDEFINED_ENTITY,
  XML_ERROR_RECURSIVE_ENTITY_REF,
  XML_ERROR_ASYNC_ENTITY,
  XML_ERROR_BAD_CHAR_REF,
  XML_ERROR_BINARY_ENTITY_REF,
  XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF,
  XML_ERROR_MISPLACED_XML_PI,
  XML_ERROR_UNKNOWN_ENCODING,
  XML_ERROR_INCORRECT_ENCODING
};

int XMLPARSEAPI XML_GetErrorCode(XML_Parser parser);
int XMLPARSEAPI XML_GetErrorLineNumber(XML_Parser parser);
int XMLPARSEAPI XML_GetErrorColumnNumber(XML_Parser parser);
long XMLPARSEAPI XML_GetErrorByteIndex(XML_Parser parser);

void XMLPARSEAPI
XML_ParserFree(XML_Parser parser);

const char XMLPARSEAPI *
XML_ErrorString(int code);

#ifdef __cplusplus
}
#endif

#endif /* not XmlParse_INCLUDED */
