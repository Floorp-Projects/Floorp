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

/**
 * A "unique identifier". This is modeled after OSF DCE UUIDs.
 */
struct nsID {
  PRUint32 m0;
  PRUint16 m1, m2;
  PRUint8 m3[8];
 
  inline PRBool Equals(const nsID& other) const {
    return (PRBool)
      ((((PRUint32*) &m0)[0] == ((PRUint32*) &other.m0)[0]) &&
       (((PRUint32*) &m0)[1] == ((PRUint32*) &other.m0)[1]) &&
       (((PRUint32*) &m0)[2] == ((PRUint32*) &other.m0)[2]) &&
       (((PRUint32*) &m0)[3] == ((PRUint32*) &other.m0)[3]));
  }

  // Turns a {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} string into
  // an nsID
  PRBool Parse(char *aIDStr);

  // Returns an allocated string in {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
  // format. Caller should free string.
  char* ToString() const;
};

#endif

