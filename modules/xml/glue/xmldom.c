/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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


#ifdef MOZILLA_CLIENT
char *
makeAbsoluteURL (char* p1, char* p2)
{
  return NET_MakeAbsoluteURL(p1, p2);

}


#endif

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
#ifdef MOZILLA_CLIENT
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
      if (f->numTransclusions == 0) {
        f->transclusions = (XMLElement*)getMem(sizeof(XMLElement*) * 10);
      } 
      f->transclusions[f->numTransclusions++] = xmle;
#ifdef DOM
      ET_ReflectObject(f->mwcontext, f->transclusions[f->numTransclusions - 1], NULL, LO_DOCUMENT_LAYER_ID, 
		  f->numTransclusions - 1, LM_TRANSCLUSIONS);
#endif
    }
  }
#endif
}


char** 
setAttribute (char** attlist, char* elName, char* elValue)
{
  size_t n = 0;
  char** nattlist;
  if (!attlist) return NULL;
  while ((n < 2*MAX_ATTRIBUTES) && (*(attlist + n) != NULL)) {
    if (strcmp(*(attlist + n), elName) == 0) {
      *(attlist + n + 1) = elValue;
      return attlist;
    }
    n = n + 2;
  }

  nattlist = getMem(n+2);
  memcpy(nattlist, attlist, (n * sizeof(char**)));
  *(nattlist + n) = elName;
  *(nattlist + n + 1) = elValue;
  freeMem(attlist);
  return nattlist;
}

#ifdef MOZILLA_CLIENT
void XMLSetTransclusionProperty( XMLFile f, uint index, char* propName, char* propValue ) {
  XMLElement el = f->transclusions[index];
  if (!el) return;
  el->attributes = setAttribute(el->attributes, propName, propValue);
  if (stringEquals(propName, "href")) {
    XMLHTMLInclusion incl =  (XMLHTMLInclusion) el->content;
    freeMem(incl->content);
    incl->content =  (char**)getMem(400);
    incl->n = 0;
    readHTML (makeAbsoluteURL(f->address, propValue), (XMLHTMLInclusion)el->content);
  } else if (stringEquals(propName, "visibility")
             ||stringEquals(propName, "display")) {
    xmlhtml_complete_int(f);
  }
}

void XMLDeleteMochaObjectReference(XMLFile f, uint index)
{
  XMLElement el = f->transclusions[index];
  if (el) el->mocha_object = NULL;
}

int32
XMLTransclusionCount(MWContext *context)
{
  if (!context) {
    return 0;
  } else {
    XMLFile f = (XMLFile)context->xmlfile;
    if (!f) {
      return 0;
    } else {
      return f->numTransclusions;
    }
  }
}

JSObject* XML_GetMochaObject (void* el) {
  return ((XMLElement)el)->mocha_object;
}

void XML_SetMochaObject (void* el, JSObject* jso) {
  ((XMLElement)el)->mocha_object = jso;
}


void * /* XMLElement */ XMLGetTransclusionByIndex( MWContext *context, uint index )
{
  XMLFile f = context->xmlfile;
  if (!f) return 0;
  return f->transclusions[index];
}

#endif

void XMLDOM_PIHandler (XMLFile f, const char *elementName, const char *data) {  
#ifdef MOZILLA_CLIENT
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
#endif  
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

