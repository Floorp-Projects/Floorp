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
#include "rdf-int.h"
#include "gs.h"



typedef struct _TrieNodeStruct {
    char c;
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

void addTarget (RDFT db, TNS node, RDF_Resource label, RDF_Resource targetNode) {
    TTS target = (TTS) fgetMem(sizeof(TrieTargetStruct));
    target->next = node->targets;
    node->targets = target;
    target->label = label;
    target->target = targetNode;
    target->db = db;
}

void RDFGS_AddSearchIndex (RDFT db, char* string, RDF_Resource label, RDF_Resource target) {
    size_t size = strlen(string);
    size_t n    = 0;
    TNS    prev, next;
    if (!gRootNode) gRootNode = (TNS) getMem(sizeof(TrieNodeStruct));
    prev = gRootNode;
    next = 0;
    while (n < size) {
	char c = string[n]; 
	if (!wsCharp(c)) {
	    next = (TNS) fgetMem(sizeof(TrieNodeStruct));
	    next->next = gRootNode->next;
	    gRootNode->next = next;
	    gRootNode->c    = c;
	} else if (next) {
	    addTarget(db, next, label, target);
	    next = 0;
	}
    }
    if (next)  addTarget(db, next, label, target);
}    
	    
RDF_Cursor RDFGS_Search (RDFT db, char* searchString, RDF_Resource label) {
    RDF_Cursor c = (RDF_Cursor) getMem(sizeof(RDF_CursorStruct));
    c->searchString = searchString;
    c->s = label;
    c->db = db;
    c->pdata = gRootNode;
    return c;
}

void RDFGS_DisposeCursor (RDF_Cursor c) {
    freeMem(c);
}
    
RDF_Resource RDFGS_NextValue (RDF_Cursor c) {
    TNS currentTNS = (TNS) c->pdata;
    TTS currentTTS = (TTS) c->pdata1;
    if (!currentTNS && !currentTTS) {
	return 0;
    } else if (currentTTS) {
	while (currentTTS) {
	    if ((!c->s) || (c->s == currentTTS->label)) {
		RDF_Resource ans =currentTTS->target;
		currentTTS = c->pdata1 = currentTTS->next;
		return ans;
	    } 
	    c->pdata1 = currentTTS =  currentTTS->next;
	}
    }
    while (!currentTTS) {
	c->pdata = currentTNS = currentTNS->next;
	if (currentTNS) c->pdata1 = currentTTS = currentTNS->targets;
    }
    return RDFGS_NextValue(c);
}
	
    
    













