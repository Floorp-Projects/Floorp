/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "Queue.h"

struct  QueueEntry {
    void * ptr;
    QueueEntry *next;
    QueueEntry *prev;
    QueueEntry(void* _ptr) {
      next = prev = NULL;
      ptr = _ptr;
    }
};

Queue::Queue() {
    m_head = NULL;
    m_tail = NULL;
    m_lock = PR_NewLock();
}

Queue::~Queue() {
    while (Get())
	;
    PR_DestroyLock(m_lock);
}

nsresult Queue::Put(void *ptr) {
    PR_Lock(m_lock);
    QueueEntry* p = new QueueEntry(ptr);
    if (!m_tail) {
	m_tail = new QueueEntry(ptr);
    }
    else {
       m_tail->next = new QueueEntry(ptr);
       m_tail->next->prev = m_tail;
       m_tail = m_tail->next;
    }
    if (!m_head) 
	m_head = m_tail;
   
    PR_Unlock(m_lock);
    return NS_OK;
}

void * Queue::Get() {
    void *res;
    PR_Lock(m_lock);

    if (!m_head) {
      m_tail = NULL;
      PR_Unlock(m_lock);
      return NULL; 
    }

    QueueEntry* excluded;
    excluded = m_head;

    if (m_head->next)
      m_head = m_head->next, m_head->prev = NULL;
    else
     m_head = m_tail = NULL;
    
    
    res = excluded->ptr;
    delete excluded;
    PR_Unlock(m_lock);
    return res;
}



