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

#ifndef	_RDF_XMLGLUE_H_
#define	_RDF_XMLGLUE_H_

/* Make sure @#$%^&* LAYERS is defined for the Mac */
#ifndef LAYERS
#define	LAYERS
#endif

#include "xmlparse.h"
#define MOZILLA_CLIENT 1
#ifdef XP_MAC
#define MOZILLA_CLIENT 1
#endif

#ifdef MOZILLA_CLIENT
#include "jscompat.h"
#include "lo_ele.h"
#include "libevent.h"
#include "libmocha.h"
#include "net.h"
#include "xp.h"
#include "xp_str.h"
#endif

#ifdef XP_UNIX
#include <sys/fcntl.h>
#elif defined(XP_MAC)
#include <fcntl.h>
#endif



/* xmlglue.c data structures and defines */

#define XML_LINE_SIZE 4096


#include <stdlib.h>
#include <string.h>
#include "nspr.h"
#include "plhash.h"
#include "ntypes.h"
#include "utils.h"
#include "xmlparse.h"

#define getMem(x) PR_Calloc(1,(x))
#define freeMem(x) PR_Free((x))

#define copyString(source) XP_STRDUP(source)
#define stringEquals(x, y) (strcasecomp(x, y) ==0)



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
  int32   status;
  struct _StyleSheetStruct* ss;
  struct _XMLElementStruct*  top;
  struct _XMLElementStruct*  current;
  void*   stream;
  int8    numOpenStreams;
  void*   urls;
#ifdef MOZILLA_CLIENT
  MWContext*   mwcontext;
#endif
  char*   address;
  char*   outputBuffer;
  XML_Parser parser;
  int32   numTransclusions;
  struct _XMLElementStruct** transclusions;
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
  int id;   /* Unique identifier among all style elements in all style sheets */
} StyleElementStruct;

typedef StyleElementStruct* StyleElement;


typedef struct _XMLElementStruct {
  char*  tag;
  char** attributes;
  char* content;
#ifdef MOZILLA_CLIENT
  JSObject *mocha_object;
#endif
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
char *		getAttributeValue (const char** attlist, char* elName);
void		tokenizeXMLElement (char* attr, char** attlist);
void            readCSS(StyleSheet ss);
void            outputToStream(XMLFile f, char* s);


void XMLDOM_CharHandler (XMLFile f, const char* content, int len) ;
void XMLDOM_EndHandler (XMLFile f, const char* elementName) ;
void XMLDOM_StartHandler (XMLFile f, const char* elementName, const char** attlist) ;
void XMLDOM_PIHandler (XMLFile f, const char *elementName, const char *attlist) ;

unsigned int			xml_write_ready(NET_StreamClass *stream);
void				xml_abort(NET_StreamClass *stream, int status);
void				xml_complete (NET_StreamClass *stream);
unsigned int			xmlcss_write_ready(NET_StreamClass *stream);
void				xmlcss_abort(NET_StreamClass *stream, int status);
void				xmlcss_complete (NET_StreamClass *stream);
void				outputToStream (XMLFile f, char* s);
void				xmlcss_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx);
void				readCSS (StyleSheet ss);
int				xmlhtml_write(NET_StreamClass *stream, const char *str, int32 len);
unsigned int			xmlhtml_write_ready(NET_StreamClass *stream);
void				xmlhtml_abort(NET_StreamClass *stream, int status);
void				xmlhtml_complete  (NET_StreamClass *stream);
void				xmlhtml_complete_int (XMLFile xml);
void				xmlhtml_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx);
void				readHTML (char* url, XMLHTMLInclusion ss);

PUBLIC NET_StreamClass *	XML_XMLConverter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id);
PUBLIC NET_StreamClass *	XML_CSSConverter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id);
PUBLIC NET_StreamClass *	XML_HTMLConverter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id);

XP_END_PROTOS

#endif
