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
      memset(ss->holdOver, '\0', RDF_BUF_SIZE);
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
	} else if (size > last) {
	  memcpy(ss->holdOver, ss->line, m);
	}
      } else if (c == '{') ss->holdOver[0] = '{';
    }
  }
  return(size);
}



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
