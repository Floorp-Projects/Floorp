/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsAtomTable_h__
#define nsAtomTable_h__

#include "nsIAtom.h"

class AtomImpl : public nsIAtom {
public:
  AtomImpl();
  virtual ~AtomImpl();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIATOM

  void* operator new(size_t size, const nsAReadableString& aString);

  void operator delete(void* ptr) {
    ::operator delete(ptr);
  }

  // Actually more; 0 terminated. This slot is reserved for the
  // terminating zero.
  PRUnichar mString[1];
};

#endif // nsAtomTable_h__
