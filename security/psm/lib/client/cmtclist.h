/* 
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#ifndef cmtclist_h___
#define cmtclist_h___

typedef struct CMTCListStr CMTCList;
/*
** Circular linked list
*/
struct CMTCListStr {
    CMTCList	*next;
    CMTCList	*prev;
};

/*
** Insert element "_e" into the list, before "_l".
*/
#define CMT_INSERT_BEFORE(_e,_l)	 \
	(_e)->next = (_l);	 \
	(_e)->prev = (_l)->prev; \
	(_l)->prev->next = (_e); \
	(_l)->prev = (_e);	 \

/*
** Insert element "_e" into the list, after "_l".
*/
#define CMT_INSERT_AFTER(_e,_l)	 \
	(_e)->next = (_l)->next; \
	(_e)->prev = (_l);	 \
	(_l)->next->prev = (_e); \
	(_l)->next = (_e);	 \

/*
** Append an element "_e" to the end of the list "_l"
*/
#define CMT_APPEND_LINK(_e,_l) CMT_INSERT_BEFORE(_e,_l)

/*
** Insert an element "_e" at the head of the list "_l"
*/
#define CMT_INSERT_LINK(_e,_l) CMT_INSERT_AFTER(_e,_l)

/* Return the head/tail of the list */
#define CMT_LIST_HEAD(_l) (_l)->next
#define CMT_LIST_TAIL(_l) (_l)->prev

/*
** Remove the element "_e" from it's circular list.
*/
#define CMT_REMOVE_LINK(_e)	       \
	(_e)->prev->next = (_e)->next; \
	(_e)->next->prev = (_e)->prev; \

/*
** Remove the element "_e" from it's circular list. Also initializes the
** linkage.
*/
#define CMT_REMOVE_AND_INIT_LINK(_e)    \
	(_e)->prev->next = (_e)->next; \
	(_e)->next->prev = (_e)->prev; \
	(_e)->next = (_e);	       \
	(_e)->prev = (_e);	       \

/*
** Return non-zero if the given circular list "_l" is empty, zero if the
** circular list is not empty
*/
#define CMT_CLIST_IS_EMPTY(_l) \
    ((_l)->next == (_l))

/*
** Initialize a circular list
*/
#define CMT_INIT_CLIST(_l)  \
	(_l)->next = (_l); \
	(_l)->prev = (_l); \

#define CMT_INIT_STATIC_CLIST(_l) \
    {(_l), (_l)}

#endif /* cmtclist_h___ */
