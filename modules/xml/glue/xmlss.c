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
   This is some glue code for a semi-parsing CSS. Its used to
   output html from XML.
   This will be changed when we incorporate the real xml parser 
   from James Clark will replace this  within the next few weeks.

   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
   For more info on XML in navigator, look at the XML section
   of www.mozilla.org.
*/

#include "xmlss.h"
#define href "href"
#define XLL   "XML-Link"

#define BUF_SIZE 4096

int
parseNextXMLCSSBlob (NET_StreamClass *stream, char* blob, int32 size)
{
  StyleSheet ss;
  int32 n, last, m;
  PRBool somethingseenp = false;
  n = last = 0;
  
  ss = (StyleSheet)(stream->data_object);
  if ((ss == NULL) || (size < 0)) {
    return MK_INTERRUPTED;
  }
  
  while (n < size) {
    char c = blob[n];
    m = 0;
    memset(ss->line, '\0', ss->lineSize);
    if (ss->holdOver[0] != '\0') {
      memcpy(ss->line, ss->holdOver, strlen(ss->holdOver));
      m = strlen(ss->holdOver);
      somethingseenp = 1;
      memset(ss->holdOver, '\0', BUF_SIZE);
    }
    while ((m < 300) && (c != '{') && (c != '}')) {
      ss->line[m] = c;
      m++;
      somethingseenp = (somethingseenp || ((c != ' ') && 
					   (c != '\r') && (c != '\n')));
      n++;
      if (n < size) {
	c = blob[n]; 
      }  else break;
    }
    if (c == '}') ss->line[m] = c;
    n++;
    if (m > 0) {
      if ((c == '}') || (c == '{')) {
        last = n;
        if (c == '{') ss->holdOver[0] = '{'; 
        if (somethingseenp) {
          parseNextXMLCSSElement(ss, ss->line);
        }
      }  else if (size > last) {
          memcpy(ss->holdOver, ss->line, m);
      }
    } else if (c == '{') ss->holdOver[0] = '{';
  }
  return(size);
}

static int32 style_id_counter = 0;

void
parseNextXMLCSSElement (StyleSheet ss, char* ele)
{
  if (ele[0] == '{') {
    char* sty = getMem(strlen(ele)-1);
    memcpy(sty, &ele[1], strlen(ele)-2);
    ss->el->style = sty;
  } else {
     StyleElement se = (StyleElement) getMem(sizeof(StyleElementStruct));
     size_t numTags,  size, n, count;
     se->id = ++style_id_counter;
     se->next = ss->el;
     ss->el = se;
     size = strlen(ele);
     count = n = 0;
     numTags = countTagStackSize(ele);
     while (count < numTags) {
       size_t st = 0; 
       char** tgs =  (char**) getMem(numTags * sizeof(char**));
       se->tagStack = tgs;
       while (st < numTags) {	 
	 *(tgs + st++) = getNextSSTag(ele, &n);
       }
       count++;
     }
  }
}



size_t
countSSNumTags (char* ele)
{
  size_t n = 0;
  size_t m = 1;
  size_t sz = strlen(ele);
  while (n < sz) {
    if (ele[n++] == ',') m++;
  }
  return m;
}



size_t
countTagStackSize (char* ele)
{
  size_t n = 0;
  size_t m = 0;
  PRBool inSpace = 1;
  size_t sz = strlen(ele);
  while (n < sz) {
    if (ele[n] == ',') break;
    if ((ele[n] != ' ') && (ele[n] != '\r') && (ele[n] != '\n')) {
      if (inSpace) {
		  m++;
		  inSpace = 0;
	  }
    } else inSpace = 1;
    n++;
  }
  return m;
}



char*
getNextSSTag (char* ele, size_t* n)
{
  char tag[100];
  size_t m = 0;
  char c = ele[*n];
  size_t s = strlen(ele);
  memset(tag, '\0', 100);
  while ((c == '\r') || (c == ' ') || (c == '\n')) {
	*n = *n + 1;
	c = ele[*n];
  }
  while ((c != ' ') && (c != ',') && (*n < s)) {
    tag[m++] = c;
    *n = *n + 1;
    c = ele[*n];
  }
  return copyString(tag);
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
    char* htmlEquiv = getAttributeValue(el->attributes, "html");
    char* linkTag   = getAttributeValue(el->attributes, XLL);
    char*  htmlBuff = getMem(1024);
    XMLElement child = el->child;
    PRBool suppressTag = !startsWith("html:", el->tag) ; 

    if (linkTag && (stringEquals(linkTag, "LINK"))) {
      char* hrefVal = getAttributeValue(el->attributes, href);
      if (hrefVal) {
        char* type = getAttributeValue(el->attributes, "Role");
        char* show = getAttributeValue(el->attributes, "Show");
        if (show &&(strcmp(show, "EMBED") == 0)) {
          if (type && (strcmp(type, "HTML") == 0)) {
            int32 n = 0;
            XMLHTMLInclusion incl = (XMLHTMLInclusion) el->content;
            while (incl && *(incl->content + n)) {              
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
    } else if (!suppressTag){
      sprintf(htmlBuff, "<%s ", &((el->tag)[5]));
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
    } else  if (!suppressTag) {
      sprintf(htmlBuff, "</%s>\n", &((el->tag)[5]));
      outputToStream(f, htmlBuff);
    }
    outputStyleSpan(f, el, 1);
    if (linkTag) outputToStream(f, "</a>");
    freeMem(htmlBuff);
  }
}

void
outputAllStyles(XMLFile xf)
{
  char *ssline;
  StyleSheet ss ;
  StyleElement se;
  outputToStream(xf, "<style>\n");
  for (ss = xf->ss; (ss != NULL) ; ss = ss->next) {    
    for (se = ss->el; (se != NULL) ; se = se->next) {
      if (se->style) { 
      ssline = PR_smprintf(".xml%d {%s}\n", se->id, se->style);
      PR_ASSERT(ssline);
      if (!ssline)
          return;
      outputToStream(xf, ssline);
      free(ssline);
	  }
    }
  }
  outputToStream(xf, "</style>\n");
}

void
convertToHTML (XMLFile xf) {
  XMLElement el = xf->top;
  xf->numOpenStreams--;
  if (xf->numOpenStreams < 1) {
    outputToStream(xf, "<html><body>");
    outputAllStyles(xf);
    outputAsHTML(xf, el); 
    outputToStream(xf, "</body></html>");

  }
}

void
outputStyleSpan (XMLFile f, XMLElement el, PRBool endp)
{
  char *ssid;
  StyleSheet ss ;
  StyleElement se;
  for (ss = f->ss; (ss != NULL) ; ss = ss->next) {    
    for (se = ss->el; (se != NULL) ; se = se->next) {
	 
      if (se->style && stringEquals((se->tagStack)[0],  el->tag)) {
        /* check for se->style from Steve Wingard <swingard@spyglass.com> */
        PRBool divp = startsWith("Display:Block;", se->style);
        PRBool listp = startsWith("Display:List-item;", se->style);
        if (!endp) {
          if (divp) {
            outputToStream(f, "<div ");
          } else  if (listp) {
            outputToStream(f, "<UL><LI><span ");
          } else   {
            outputToStream(f, "<span ");
          }
          ssid=PR_smprintf("class=xml%d>\n", se->id);
          PR_ASSERT(ssid);
          if (!ssid)
              return;
          outputToStream(f, ssid);
          free(ssid);
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
