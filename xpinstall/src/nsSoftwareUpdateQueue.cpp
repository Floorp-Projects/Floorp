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

#include "nsSoftwareUpdateQueue.h"
#include "prmem.h"
 /*
 * This struct represents one entry in a queue setup to hold download 
 * requests if there are more than one.
 */
typedef struct su_QItem
{
    struct su_QItem   * next;
    struct su_QItem   * prev;
    su_startCallback  * QItem;
} su_QItem;


static su_QItem  * Qhead = NULL;
static su_QItem  * Qtail = NULL;

PRBool QInsert( su_startCallback * Item )
{
    su_QItem *p = (su_QItem*) PR_MALLOC( sizeof (su_QItem));

    if (p == NULL)
        return FALSE;

    p->QItem = Item;
    p->next = Qhead;
    p->prev = NULL;
    if (Qhead != NULL)
        Qhead->prev = p;
    Qhead = p;
    if (Qtail == NULL) /* First Item inserted in Q? */
        Qtail = p;

    return TRUE;
}

su_startCallback *QGetItem(void)
{
    su_QItem *Qtemp = Qtail;

    if (Qtemp != NULL)
    {
        su_startCallback *p = NULL;
    
        Qtail = Qtemp->prev;
    
        if (Qtail == NULL) /* Last Item deleted from Q? */
            Qhead = NULL;
        else
            Qtail->next = NULL;
    
        p = Qtemp->QItem;
        PR_FREEIF(Qtemp);
        return p;
    }
    return NULL;
}
