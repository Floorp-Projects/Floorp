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
#ifndef nsIAtom_h___
#define nsIAtom_h___

#include "nscore.h"
#include "nsISupports.h"
class nsString;
class nsISizeOfHandler;

#define NS_IATOM_IID          \
{ 0x3d1b15b0, 0x93b4, 0x11d1, \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

/**
 * A globally unique identfier.  nsIAtom's can be compared for
 * equality by using operator '=='. These objects are reference
 * counted like other nsISupports objects. When you are done with
 * the atom, NS_RELEASE it.
 */
class nsIAtom : public nsISupports {
public:
  /**
   * Translate the unicode string into the stringbuf.
   */
  virtual void ToString(nsString& aString) const = 0;

  /**
   * Return a pointer to a zero terminated unicode string.
   */
  virtual const PRUnichar* GetUnicode() const = 0;

  /**
   * Add the size, in bytes, of the atom to the handler.
   */
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const = 0;
};

/**
 * Find an atom that matches the given iso-latin1 C string. The
 * C string is translated into it's unicode equivalent.
 */
extern NS_BASE nsIAtom* NS_NewAtom(const char* isolatin1);

/**
 * Find an atom that matches the given unicode string. The string is assumed
 * to be zero terminated.
 */
extern NS_BASE nsIAtom* NS_NewAtom(const PRUnichar* unicode);

/**
 * Find an atom that matches the given string.
 */
extern NS_BASE nsIAtom* NS_NewAtom(const nsString& aString);

/**
 * Return a count of the total number of atoms currently
 * alive in the system.
 */
extern NS_BASE nsrefcnt NS_GetNumberOfAtoms(void);

#endif /* nsIAtom_h___ */
