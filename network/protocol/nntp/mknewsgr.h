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
/* functions to manage large numbers of USENET newsgroups
 * store them on disk, free them, search them, etc.
 *
 * Lou Montulli
 */

#ifndef MKNEWSGR_H
#define MKNEWSGR_H

/* save a newsgroup name
 *
 * will munge the newsgroup name passed in for efficiency
 *
 */
extern void NET_StoreNewsGroup(char *hostname, PRBool is_secure, char * newsgroup);

/* free up the list of newsgroups
 */
extern void NET_FreeNewsgroups(char *hostname, PRBool is_secure);

/* sort the newsgroups
 */
extern void NET_SortNewsgroups(char *hostname, PRBool is_secure);

/* Search and display newsgroups
 */
extern int NET_DisplayNewsgroups(MWContext *context,
								 char *hostname,
								 Bool is_secure, 
								 char *search_string);

/* Save them out to disk
 */
extern void NET_SaveNewsgroupsToDisk(char *hostname, PRBool is_secure);

/* read them from disk
 */
extern time_t NET_ReadNewsgroupsFromDisk(char *hostname, PRBool is_secure);

/* Get the last access date, load the newsgroups from
 * disk if they are not loaded
 */
extern time_t NET_NewsgroupsLastUpdatedTime(char *hostname, PRBool is_secure);

#endif
