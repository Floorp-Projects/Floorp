/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef CLIST_H
#define CLIST_H

#include <stddef.h>

// -----------------------------------------------------------------------
//
// Simple circular linked-list implementation...
//
// -----------------------------------------------------------------------

// Foreward declarations...
struct CList;

#define OBJECT_PTR_FROM_CLIST(className, listElement) \
            ((char*)listElement - offsetof(className, m_link))
                

struct CList {
    CList *next;
    CList *prev;

    CList() {
        next = prev = this;
    }

    ~CList() {
        Remove();
    }
        
    //
    // Append an element to the end of this list
    //
    void Append(CList &element) {
        element.next = this;
        element.prev = prev;
        prev->next = &element;
        prev = &element;
    }

    //
    // Add an element to the beginning of this list
    //
    void Add(CList &element) {
        element.next = next;
        element.prev = this;
        next->prev = &element;
        next = &element;
    }

    //
    // Append this element to the end of a list
    //
    void AppendToList(CList &list) {
        list.Append(*this);
    }

    //
    // Add this element to the beginning of a list
    //
    void AddToList(CList &list) {
        list.Add(*this);
    }

    //
    // Remove this element from the list and re-initialize
    //
    void Remove(void) {
        prev->next = next;
        next->prev = prev;
 
        next = prev = this;
    }

    //
    // Is this list empty ?
    //
    BOOL IsEmpty(void) {
        return (next == this);
    }
};    

#endif // CLIST_H

