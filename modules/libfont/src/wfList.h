/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


// wfList.h
// A doubly linked list manager
//
// Created by dp Suresh <dp@netscape.com>

#ifndef __WF_LIST__
#define __WF_LIST__

struct wfListElement;
struct wfListElement {
  void *item;
  struct wfListElement *next;
  struct wfListElement *prev;
};

class wfList;

typedef void (*WF_FREE_FUNC)(wfList *object, void *item);

class wfList
{
public:
  struct wfListElement *head;
  struct wfListElement *tail;
  WF_FREE_FUNC freeItemFunc;

  typedef enum {
    WF_FALSE = 0,
    WF_TRUE,
    SUCCESS,
    GENERIC_ERROR,
    NOT_FOUND,
    NO_MEMORY
#if defined(XP_OS2)
	/* insure that we know the size of this enum (4 bytes!) */
	, ERROR_CODE_MAX = 70000 
#endif
  } ERROR_CODE;
  typedef ERROR_CODE (*IteratorFunction) (void *);

public:
  // Constructor
  wfList(WF_FREE_FUNC freeFunc);

  // Destructor
  ~wfList();

  // Private methods
private:
  struct wfListElement *find (void *item);
  ERROR_CODE listAdd (struct wfListElement *ele);
  ERROR_CODE listRemove (struct wfListElement *ele);
  void listDeleteItem (void *item);

  // Public Methods
public:
  ERROR_CODE add (void *item);
  ERROR_CODE remove (void *item);
  ERROR_CODE removeAll(void);
  ERROR_CODE isExist(void *item);
  ERROR_CODE isEmpty(void);
  int count(void);
  ERROR_CODE iterate(IteratorFunction fn);
};

#endif /* __WF_LIST__ */
