/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
    bool IsEmpty(void) {
        return (next == this);
    }
};    

#endif // CLIST_H

