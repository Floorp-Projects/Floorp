/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef	_RDF_XMLPARSE_H_
#define	_RDF_XMLPARSE_H_


/* This is a "stop-gap" xml parser that is being shipped with the free
   source. A real xml parser from James Clark will replace this 
   within the next few weeks */


#include <stdlib.h>
#include <string.h>
#include "nspr.h"
#include "plhash.h"
#include "ntypes.h"
#include "net.h"
#include "xp.h"
#include "xp_str.h"
#include "utils.h"
#include "rdf-int.h"



#ifndef true
#define true PR_TRUE
#endif
#ifndef false
#define false PR_FALSE
#endif
#define null NULL
#define nullp(x) (((void*)x) == ((void*)0))



/* xmlparse.c data structures and defines */

#define MAX_TAG_SIZE 100
#define MAX_ATTRIBUTES 10
#define XML_BUF_SIZE 4096
#define wsc(c) ((c == ' ') || (c == '\r') || (c == '\n') || (c == '\t'))
typedef struct _XMLFileStruct {
  struct _StyleSheetStruct* ss;
  struct _XMLElementStruct*  top;
  struct _XMLElementStruct*  current;
  uint32  status;
  void*   stream;
  int8  numOpenStreams;
  void*  urls;
  void* mwcontext;
  char* holdOver;
  int32   lineSize;
  char* line;
  char*  address;
} XMLFileStruct;

typedef XMLFileStruct* XMLFile;

typedef struct _StyleSheetStruct {
  XMLFile xmlFile;
  struct _StyleElementStruct* el;
  struct _StyleSheetStruct* next;
  void*  urls;
  char* holdOver;
  int32   lineSize;
  char* line;
  char* address;
} StyleSheetStruct;

typedef StyleSheetStruct* StyleSheet;

typedef struct _StyleElementStruct {
  char** tagStack;
  char*   style;
  struct _StyleElementStruct* next;
} StyleElementStruct;

typedef StyleElementStruct* StyleElement;


typedef struct _XMLElementStruct {
  char*  tag;
  char** attributes;
  char* content;
  struct _XMLElementStruct* parent;
  struct _XMLElementStruct* child;
  struct _XMLElementStruct* next;
} XMLElementStruct;

typedef XMLElementStruct* XMLElement;

typedef struct _XMLHTMLInclusionStruct {
  char** content;
  XMLFile xml;
  int32 n;
} XMLHTMLInclusionStruct;

typedef  XMLHTMLInclusionStruct *XMLHTMLInclusion;



/* xmlparse.c function prototypes */

XP_BEGIN_PROTOS

int		parseNextXMLBlob (NET_StreamClass *stream, char* blob, int32 size);
void		processNextXMLContent (XMLFile f, char* content);
char **		copyCharStarList (char** list);
char *		makeAbsoluteURL (char* p1, char* p2);
void		processNextXMLElement (XMLFile f, char* attr);
void		addStyleSheet(XMLFile f, StyleSheet ss);
void		addChild (XMLElement parent, XMLElement child);
char *		getTagHTMLEquiv (XMLFile f, XMLElement el);
void		outputAttributes(XMLFile f, char** attlist);
void		outputAsHTML (XMLFile f, XMLElement el);
void		convertToHTML (XMLFile xf);
void		outputStyleSpan (XMLFile f, XMLElement el, PRBool endp);
char *		getAttributeValue (char** attlist, char* elName);
void		tokenizeXMLElement (char* attr, char** attlist, char** elementName);
void            readCSS(StyleSheet ss);
void            outputToStream(XMLFile f, char* s);

XP_END_PROTOS

#endif
