/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef nsID_h__
#define nsID_h__

#include "prtypes.h"
#include "string.h"
#include "nsCom.h"

/**
 * A "unique identifier". This is modeled after OSF DCE UUIDs.
 */

struct nsID {
  /**
   * @name Indentifier values
   */

  //@{
  PRUint32 m0;
  PRUint16 m1;
  PRUint16 m2;
  PRUint8 m3[8];
  //@}

  /**
   * @name Methods
   */

  //@{
  /**
   * Equivalency method. Compares this nsID with another.
   * @return <b>PR_TRUE</b> if they are the same, <b>PR_FALSE</b> if not.
   */

  inline PRBool Equals(const nsID& other) const {
    return (PRBool)
      ((((PRUint32*) &m0)[0] == ((PRUint32*) &other.m0)[0]) &&
       (((PRUint32*) &m0)[1] == ((PRUint32*) &other.m0)[1]) &&
       (((PRUint32*) &m0)[2] == ((PRUint32*) &other.m0)[2]) &&
       (((PRUint32*) &m0)[3] == ((PRUint32*) &other.m0)[3]));
  }

  /**
   * nsID Parsing method. Turns a {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
   * string into an nsID
   */
  NS_COM PRBool Parse(char *aIDStr);

  /**
   * nsID string encoder. Returns an allocated string in 
   * {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} format. Caller should free string.
   */
  NS_COM char* ToString() const;
  //@}
};

/**
 * Declare an ID. If NS_IMPL_IDS is set, a variable <i>_name</i> is declared
 * with the given values, otherwise <i>_name</i> is declared as an 
 * <tt>extern</tt> variable.
 */

#ifdef NS_IMPL_IDS
#define NS_DECLARE_ID(_name,m0,m1,m2,m30,m31,m32,m33,m34,m35,m36,m37) \
  extern "C" const nsID _name = {m0,m1,m2,{m30,m31,m32,m33,m34,m35,m36,m37}}
#else
#define NS_DECLARE_ID(_name,m0,m1,m2,m30,m31,m32,m33,m34,m35,m36,m37) \
  extern "C" const nsID _name
#endif

#endif

