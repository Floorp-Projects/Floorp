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
   This is a "stop-gap" xml parser that is being shipped with the free
   source. A real xml parser from James Clark will replace this 
   within the next few weeks.

   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
   For more info on XML in navigator, look at the XML section
   of www.mozilla.org.

*/

#include "xmlparse.h"
#include "xmlglue.h"


#define href "HRef"
#define XLL   "XML-Link"



int
parseNextXMLBlob (NET_StreamClass *stream, char* blob, int32 size)
{
  XMLFile f;
  int32 n, last, m;
  PRBool somethingseenp = false;
  n = last = 0;
  
  f = (XMLFile)(stream->data_object);
  if ((f == NULL) || (f->urls == NULL) || (size < 0))
  {
    return MK_INTERRUPTED;
  }
  
  while (n < size) {
    char c = blob[n];
    m = 0;
    somethingseenp = false;
    memset(f->line, '\0', f->lineSize);
    if (f->holdOver[0] != '\0') {
      memcpy(f->line, f->holdOver, strlen(f->holdOver));
      m = strlen(f->holdOver);
      somethingseenp = true;
      memset(f->holdOver, '\0', XML_BUF_SIZE);
    }
    while ((m < 300) && (c != '<') && (c != '>')) {
      f->line[m] = c;
      m++;
      somethingseenp = (somethingseenp || (!wsc(c)));
      n++;
      if (n < size) {
	c = blob[n]; 
      }  else break;
    }
    if (c == '>') f->line[m] = c;
    n++;
    if (m > 0) {
      if ((c == '<') || (c == '>')) {
	last = n;
	if (c == '<') f->holdOver[0] = '<'; 
	if (somethingseenp == true) {
	  if (f->line[0] == '<') {
	    if (f->line[1] == '/') {
	      f->current = f->current->parent;
	    } else {
	      processNextXMLElement(f, f->line);
	    }
	  } else {
	    processNextXMLContent(f, f->line);
	  }
	}
      } else if (size > last) {
	memcpy(f->holdOver, f->line, m);
      }
    } else if (c == '<') f->holdOver[0] = '<';
  }
  return(size);
}



void
processNextXMLContent (XMLFile f, char* content)
{
  XMLElement xmle = (XMLElement)getMem(sizeof(XMLElementStruct));
  xmle->content = copyString(content);
  xmle->tag = "xml:content";
  addChild(f->current, xmle);
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
processNextXMLElement (XMLFile f, char* attr)
{
  size_t n = 1;
  size_t s = strlen(attr); 
  size_t m = 0;
  PRBool linkp = 0;
  char* attv;
  PRBool emptyTagp = ((attr[s-2] == '/') || ((attr[s-2] == '?') && (!startsWith("<?xml", attr))));
  XMLElement xmle = (XMLElement)getMem(sizeof(XMLElementStruct));
  char* attlist[2*MAX_ATTRIBUTES+1];
  char* elementName;
  if (startsWith("<!--", attr)) return;
  tokenizeXMLElement(attr, attlist, &elementName);
  if (stringEquals(elementName, "?StyleSheet")) linkp = 1;

  xmle->parent = f->current;
  if (f->top == NULL) f->top = xmle;  
  xmle->tag = copyString(elementName);
  xmle->attributes = copyCharStarList(attlist);
  if (f->current)  addChild(f->current, xmle);
  if (!emptyTagp || (!f->current)) f->current = xmle;
  attv = getAttributeValue(xmle->attributes, "Type");
  if (linkp && attv && (startsWith("css", attv))) {
    char* url = getAttributeValue(xmle->attributes, href);
    StyleSheet ss = (StyleSheet) getMem(sizeof(StyleSheetStruct));
    ss->address = makeAbsoluteURL(f->address, url);
    ss->line = (char*)getMem(XML_BUF_SIZE);
    ss->holdOver = (char*)getMem(XML_BUF_SIZE);
    ss->lineSize = XML_BUF_SIZE;
    ss->xmlFile = f;
    f->numOpenStreams++;
    addStyleSheet(f, ss);
    readCSS(ss);
  }
  if (emptyTagp && (getAttributeValue(xmle->attributes, XLL) != NULL)) {
    char* linkTag = getAttributeValue(xmle->attributes, XLL);
    char* hrefVal = getAttributeValue(xmle->attributes, href);
    char* type = getAttributeValue(xmle->attributes, "Role");
    char* show = getAttributeValue(xmle->attributes, "Show");

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


void
outputAttributes(XMLFile f, char** attlist)
{
  uint32 n = 0;
  PRBool xmlinkp = (getAttributeValue(attlist, XLL) != NULL);
  if (!attlist) return ;
  while ((n < 2*MAX_ATTRIBUTES) && (*(attlist + n) != NULL)) {
    if (!(xmlinkp && stringEquals(*(attlist + n), href))) {
      outputToStream(f, *(attlist + n));
      outputToStream(f, "=\"");
      outputToStream(f, *(attlist + n + 1));
      outputToStream(f, "\" ");
    }

    n = n + 2;
  }
}



void
outputAsHTML (XMLFile f, XMLElement el)
{
  if (strcmp(el->tag, "xml:content") == 0) {
    outputToStream(f, el->content);
  } else {
    char* htmlEquiv = getAttributeValue(el->attributes, "html-equiv");
    char* linkTag   = getAttributeValue(el->attributes, XLL);
    char*  htmlBuff = getMem(1024);
    XMLElement child = el->child;
    PRBool suppressTag = stringEquals(el->tag, "Title") ; /*hack to suppress html docbook interaction */

  

    if (linkTag && (stringEquals(linkTag, "LINK"))) {
      char* hrefVal = getAttributeValue(el->attributes, "HRef");
      if (hrefVal) {
        char* type = getAttributeValue(el->attributes, "Role");
        char* show = getAttributeValue(el->attributes, "Show");
        if (show &&(strcmp(show, "EMBED") == 0)) {
          if (type && (strcmp(type, "HTML") == 0)) {
            int32 n = 0;
            XMLHTMLInclusion incl = (XMLHTMLInclusion) el->content;
            while (*(incl->content + n)) {              
              outputToStream(f, (*(incl->content + n)));
              n++;
            }           
          } else if (type && (strcmp(type, "IMAGE") == 0)) {
            sprintf(htmlBuff, "<img src=%s>\n", hrefVal);
            outputToStream(f, htmlBuff);
          }
          freeMem(htmlBuff);
          return;
        } else {
          sprintf(htmlBuff, "<a href=\"%s\">", hrefVal);
          outputToStream(f, htmlBuff);
       }
      } else {
        linkTag = 0;
      }
    }

    outputStyleSpan(f, el, 0);

    if (htmlEquiv) {
      sprintf(htmlBuff, "<%s ", htmlEquiv);
      outputToStream(f, htmlBuff);
      outputAttributes(f, el->attributes);
      outputToStream(f, ">\n");
    } else if (!(el->tag[0] == '?') && !suppressTag){
      sprintf(htmlBuff, "<%s ", el->tag);
      outputToStream(f, htmlBuff);
      outputAttributes(f, el->attributes);
      outputToStream(f, ">\n");
    }
    
    while (child) {
      outputAsHTML(f, child);
      child = child->next;
    }
    if (htmlEquiv) {
      sprintf(htmlBuff, "</%s>\n", htmlEquiv);
      outputToStream(f, htmlBuff);
    } else  if (!(el->tag[0] == '?') && !suppressTag) {
      sprintf(htmlBuff, "</%s>\n", el->tag);
      outputToStream(f, htmlBuff);
    }
    outputStyleSpan(f, el, 1);
    if (linkTag) outputToStream(f, "</a>");
    freeMem(htmlBuff);
  }
}



void
convertToHTML (XMLFile xf) {
  XMLElement el = xf->top;
  xf->numOpenStreams--;
  if (xf->numOpenStreams < 1) {
    outputToStream(xf, "<html><body>");
    outputAsHTML(xf, el); 
    outputToStream(xf, "</body></html>");
  }
}




void
outputStyleSpan (XMLFile f, XMLElement el, PRBool endp)
{
  StyleSheet ss ;
  StyleElement se;
  for (ss = f->ss; (ss != NULL) ; ss = ss->next) {    
    for (se = ss->el; (se != NULL) ; se = se->next) {
	 
      if (stringEquals((se->tagStack)[0],  el->tag)) {
        PRBool divp = startsWith("Display:Block;", se->style);
        PRBool listp = startsWith("Display:List-item;", se->style);
        if (!endp) {
          if (divp) {
            outputToStream(f, "<div style=\"");
            outputToStream(f, &(se->style)[14]);
          } else  if (listp) {
            outputToStream(f, "<UL><LI><span style=\"");
            outputToStream(f, &(se->style)[20]);
          } else   {
            outputToStream(f, "<span style=\"");
            outputToStream(f, se->style);
          }
          outputToStream(f, "\">\n");
        } else {
          if (divp) {
            outputToStream(f, "</div>"); 
          } else if (listp) {
            outputToStream(f, "</UL></span>");
          } else {
            outputToStream(f, "</span>");
          }
        }
      }
    }
  }
}




void
tokenizeXMLElement (char* attr, char** attlist, char** elementName)
{
  size_t n = 1;
  size_t s = strlen(attr); 
  char c ;
  size_t m = 0;
  size_t atc = 0;
  PRBool emptyTagp =  (attr[s-2] == '/');
  PRBool inAttrNamep = 1;
  c = attr[n++]; 
  while (wsc(c)) {
    c = attr[n++];
  }
  *elementName = &attr[n-1];
  while (n < s) {
    if (wsc(c)) break;
    c = attr[n++];
  }
  attr[n-1] = '\0';
  while (atc < 2*MAX_ATTRIBUTES+1) {*(attlist + atc++) = NULL;}
  atc = 0;
  s = (emptyTagp ? s-2 : s-1);
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

/* Kludge Alert */
