/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


// 39204897
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "rdf-int.h"
#include "gs.h"



typedef struct _TrieNodeStruct {
    char c;
    struct _TrieNodeStruct* child;
    struct _TrieNodeStruct* next;
    struct _TrieTargetStruct* targets;
} TrieNodeStruct;

typedef TrieNodeStruct* TNS;

typedef struct _TrieTargetStruct {
    RDF_Resource label;
    RDF_Resource target;
    struct _TrieTargetStruct* next;
    RDFT db;
} TrieTargetStruct;

typedef TrieTargetStruct* TTS;

static TNS gRootNode = 0;

int 
addTarget (RDFT db, TNS node, RDF_Resource label, RDF_Resource targetNode) {
  TTS target ;
  int n = 0;
  /*  for (target = node->targets; target != null; target = target->next) {
    if (target->target == targetNode) return 0;
    n++;
  } */
  target = (TTS) fgetMem(sizeof(TrieTargetStruct));
  target->next = node->targets;
  node->targets = target;
  target->label = label;
  target->target = targetNode;
  target->db = db;
  return n;
}

TNS
findChildOfChar (TNS node, char c) {
  TNS ch = node->child;
  char c1 = tolower(c);
  int n = 0;
  while (ch) {
    if (c1 == ch->c) return ch;
    ch = ch->next;
	n++;
  }
  return 0;
}

TNS 
findChildOfString (TNS node, char* str) {
  size_t size = strlen(str);
  size_t n = 0;
  while (n < size) {
    char c = str[n++];
    node = findChildOfChar(node, c);
    if (!node) return 0;
  }
  return node;
}

void RDFGS_AddSearchIndex (RDFT db, char* string, RDF_Resource label, RDF_Resource target) {
    size_t size = strlen(string);
    size_t n    = 0;
    char* stk = 0;
    TNS    prev, next;
    if (!gRootNode) gRootNode = (TNS) getMem(sizeof(TrieNodeStruct));
    prev = gRootNode;
    next = 0;
    while (n < size) {
	char c = string[n++]; 
	if (!wsCharp(c) && (c != '/')) {
          if (!stk) stk = &string[n-1];
	    next = (TNS) findChildOfChar(prev, c);
            if (!next) {
              next = (TNS)fgetMem(sizeof(TrieNodeStruct));
              next->next = prev->child;
              prev->child = next;
              next->c    = tolower(c);
            }
            prev = next;            
	} else if (next) {
	    int n = addTarget(db, next, label, target);
            stk = 0;
	    prev = gRootNode;
        next = 0;
	}
    }
    if (next)  {
		addTarget(db, next, label, target);
		prev = gRootNode;
        next = 0;
	}
}    

void
countChildren (TNS node, size_t *n, size_t *m) {
  TNS ch;
  TTS tg ;
  if (node->targets) (*n)++;
  for (tg = node->targets; tg; tg = tg->next)  (*m)++;
  ch = node->child;
  while (ch) {
    countChildren(ch, n, m);
    ch = ch->next;
  }
}

void
fillUpChildren (RDF_Cursor c, TNS node) {
  TNS ch;
  if (node->targets) *((TTS*)c->pdata1 + c->count++) = node->targets;
  ch = node->child;
  while (ch) {
    fillUpChildren(c, ch);
    ch = ch->next;
  }
} 
  
	    
RDF_Cursor RDFGS_Search (RDFT db, char* searchString, RDF_Resource label) {
    RDF_Cursor c = (RDF_Cursor) getMem(sizeof(RDF_CursorStruct));
    size_t n = 0;
    size_t m = 0;
    c->searchString = searchString;
    c->s = label;
    c->db = db;
    c->pdata = findChildOfString(gRootNode, searchString);
    if (!c->pdata) return c;
    countChildren((TNS)c->pdata, &n, &m);
    c->pdata2 = (RDF_Resource*) getMem(sizeof(RDF_Resource) * (m+1)); 
    c->off1 = m;
    if (n > 0) {
      c->count = 0;
      c->pdata1 = getMem(sizeof(TTS) * (n+1));
      fillUpChildren(c, (TNS)c->pdata);
      c->count = 1;
    }
    if (c->pdata) c->pdata = ((TNS)c->pdata)->targets;

    return c;
}

void RDFGS_DisposeCursor (RDF_Cursor c) {
  if (c->pdata1) freeMem(c->pdata1);
  if (c->pdata2) freeMem(c->pdata2);
  freeMem(c);
}

int
alreadyAdded(RDF_Resource node, RDF_Cursor c) {
  int n =0;
  while (c->pdata2[n] && (n < c->off)) {
    if (c->pdata2[n] == node) return 1;
    n++;
  }
  return 0;
}
    
RDF_Resource RDFGS_NextValue (RDF_Cursor c) { 
  if (!c->pdata) {
    return 0;
  } else  {
    TTS currentTTS = (TTS) c->pdata;
    while (currentTTS) {
      if (((!c->s) || (c->s == currentTTS->label)) && 
          (!alreadyAdded(currentTTS->target, c))) {
        RDF_Resource ans = currentTTS->target;
        c->pdata = currentTTS = currentTTS->next;
        if (!currentTTS && (c->pdata1)) { 
          c->pdata =  ((TTS*)c->pdata1)[c->count++];  
        }
        if (c->off < c->off1) c->pdata2[c->off++] = ans; 
        return ans;
      }       
      c->pdata = currentTTS =  currentTTS->next;
      if (!currentTTS  && (c->pdata1)) {
        c->pdata = currentTTS = ((TTS*)c->pdata1)[c->count++];  
      }
    }
  }
  return 0;
}
	
    
    













