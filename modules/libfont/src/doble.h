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
/* 
 * doble.h (DoneObservable.h)
 *
 * C++ implementation of the (doble) DoneObservableObject
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _doble_H_
#define _doble_H_

// Implements: DoneObservable (cdoble)
#include "cdoble.h"

// Uses: DoneObserver (nfdoer)
#include "Mnfdoer.h"

// Subclass of: wfList
#include "wfList.h"

class DoneObservableObject : public wfList {
public:
  DoneObservableObject();
  ~DoneObservableObject();

  void addObserver(struct nfdoer *observer)
  {
	  add(observer);
  }
  int countObservers(void)
  {
	  return(count());
  }
  void deleteObserver(struct nfdoer *observer)
  {
	  remove(observer);
  }
  void deleteObservers(void)
  {
	  removeAll();
  }

  void notifyObservers(cdoble *obj, void *arg);
};
#endif /* _doble_H_ */
