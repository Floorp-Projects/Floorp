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

/* 
This file is used to build a persistent XML parse tree.

Eventually, this file will also contain the code to implement
the DOM APIs for access to this tree (any volunteers?)

If you have questions, send them to guha@netscape.com
*/

#include "xmlglue.h"
#define href "href"
#define XLL   "XML-Link"

void
XMLDOM_CharHandler (XMLFile f, const char* content, int len) {
  XMLElement xmle = (XMLElement)getMem(sizeof(XMLElementStruct));  
  xmle->content = getMem(len+1);
  memcpy(xmle->content, content, len);
  xmle->tag = "xml:content";
  addChild(f->current, xmle);
}


char *
xmlgetAttributeValue (const char** attlist, char* elName)
{
  size_t n = 0;
  if (!attlist) return NULL;
  while ((n < 2*MAX_ATTRIBUTES) && (*(attlist + n) != NULL)) {
    if (strcmp(*(attlist + n), elName) == 0) return (char*)*(attlist + n + 1);
    n = n + 2;
  }
  return NULL;
}

char **
copyCharStarList (char** list)
{
  size_t length = 0;
  while (*(list + length++)){}
  if (length == 0) {
    return NULL;
  } else {
    char** ans = getMem(length * sizeof(char*));
    size_t n = 0;
    while (*(list + n++)) {
      *(ans + n - 1) = copyString(*(list + n - 1));
    }
    return ans;
  }
}



char *
makeAbsoluteURL (char* p1, char* p2)
{
#ifdef XP_MAC
  return p2;
#else 
  return NET_MakeAbsoluteURL(p1, p2);
#endif
}


void
addStyleSheet(XMLFile f, StyleSheet ss)
{
  if (f->ss == NULL) {
    f->ss = ss;
  } else {
    StyleSheet nss = f->ss;
    while (nss->next != NULL) nss = nss->next;
    nss->next = ss;
  }
}


void
addChild (XMLElement parent, XMLElement child)
{
  if (parent->child == NULL) {
    parent->child = child;
  } else {
    XMLElement nc = parent->child;
    while (nc->next) {nc = nc->next;}
    nc->next = child;
  }
}

void XMLDOM_EndHandler (XMLFile f, const char* elementName) {
  f->current = f->current->parent;
}

void XMLDOM_StartHandler (XMLFile f, const char* elementName, const char** attlist) {
  XMLElement xmle = (XMLElement)getMem(sizeof(XMLElementStruct));
  xmle->parent = f->current;
  if (f->top == NULL) f->top = xmle;  
  xmle->tag = copyString(elementName);
  xmle->attributes = copyCharStarList((char**)attlist);
  if (f->current)  addChild(f->current, xmle);
  f->current = xmle;
  if (xmlgetAttributeValue(xmle->attributes, XLL) != NULL) {
    char* linkTag = xmlgetAttributeValue(xmle->attributes, XLL);
    char* hrefVal = xmlgetAttributeValue(xmle->attributes, href);
    char* type = xmlgetAttributeValue(xmle->attributes, "Role");
    char* show = xmlgetAttributeValue(xmle->attributes, "Show");

    if (linkTag && (stringEquals(linkTag, "LINK")) &&
        hrefVal && type && stringEquals(type, "HTML") &&
        show && (stringEquals(show, "Embed"))) {
      XMLHTMLInclusion incl = (XMLHTMLInclusion)getMem(sizeof(XMLHTMLInclusionStruct));
      incl->xml = f;
      incl->content = (char**)getMem(400);
      xmle->content = (char*) incl;
      f->numOpenStreams++;
      readHTML(makeAbsoluteURL(f->address, hrefVal), incl);
    }
  }
}

void XMLDOM_PIHandler (XMLFile f, const char *elementName, const char *data) {  
  if (startsWith("xml:stylesheet", elementName))   {
    char* url ; 
	char* attlist[2*MAX_ATTRIBUTES+1];
	char* attv;
    char* xdata = ((data == NULL) ? NULL : copyString(data));
	StyleSheet ss;
    tokenizeXMLElement ((char*) xdata, (char**)attlist);
    attv = xmlgetAttributeValue(attlist, "type");
	if (!(attv && (startsWith("text/css", attv)))) {
		freeMem(xdata);
		return;
	}
	url = xmlgetAttributeValue(attlist, href);
    ss = (StyleSheet) getMem(sizeof(StyleSheetStruct));
    ss->address = makeAbsoluteURL(f->address, url);
    ss->line = (char*)getMem(XML_BUF_SIZE);
    ss->holdOver = (char*)getMem(XML_BUF_SIZE);
    ss->lineSize = XML_BUF_SIZE;
    ss->xmlFile = f;
    f->numOpenStreams++;
    addStyleSheet(f, ss);
    readCSS(ss);
	freeMem(xdata); 
    return;
  }
  
}
  

void
tokenizeXMLElement (char* attr, char** attlist)
{
  size_t n = 0;
  size_t s = strlen(attr); 
  char c ;
  size_t m = 0;
  size_t atc = 0;
  PRBool inAttrNamep = 1; 
  while (atc < 2*MAX_ATTRIBUTES+1) {*(attlist + atc++) = NULL;}
  atc = 0;
  while (n < s) {
    PRBool attributeOpenStringSeenp = 0;
    m = 0;
    c = attr[n++];
    while ((n <= s) && (atc < 2*MAX_ATTRIBUTES)) {
      if (inAttrNamep && (m > 0) && (wsc(c) || (c == '='))) {
        attr[n-1] = '\0';
        *(attlist + atc++) = &attr[n-m-1];
        break;
      }
      if  (!inAttrNamep && attributeOpenStringSeenp && (c == '"')) {
        attr[n-1] = '\0';
        *(attlist + atc++) = &attr[n-m-1];
        break;
      }
      if (inAttrNamep) {
        if ((m > 0) || (!wsc(c))) m++;
      } else {
        if (c == '"') {
          attributeOpenStringSeenp = 1;
        } else {
          if ((m > 0) || (!(wsc(c)))) m++;
        }
      }
      c = attr[n++];
    }
    inAttrNamep = (inAttrNamep ? 0 : 1);
  }
}
