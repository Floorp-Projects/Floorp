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
#include "nsID.h"
#include "prprf.h"

static const char gIDFormat[] = 
  "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}";

static const char gIDFormat2[] = 
  "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x";

/* 
 * Turns a {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} string into
 * an nsID
 */

NS_COM PRBool nsID::Parse(char *aIDStr)
{
  PRInt32 count = 0;
  PRInt32 n1, n2, n3[8];
  PRInt32 n0;

  if (NULL != aIDStr) {
    count = PR_sscanf(aIDStr,
                      (aIDStr[0] == '{') ? gIDFormat : gIDFormat2,
                      &n0, &n1, &n2, 
                      &n3[0],&n3[1],&n3[2],&n3[3],
                      &n3[4],&n3[5],&n3[6],&n3[7]);

    m0 = (PRInt32) n0;
    m1 = (PRInt16) n1;
    m2 = (PRInt16) n2;
    for (int i = 0; i < 8; i++) {
      m3[i] = (PRInt8) n3[i];
    }
  }
  return (PRBool) (count == 11);
}

/*
 * Returns an allocated string in {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
 * format. Caller should delete [] the string.
 */

NS_COM char *nsID::ToString() const 
{
  char *res = new char[39];

  if (res != NULL) {
    PR_snprintf(res, 39, gIDFormat,
                m0, (PRUint32) m1, (PRUint32) m2,
                (PRUint32) m3[0], (PRUint32) m3[1], (PRUint32) m3[2],
                (PRUint32) m3[3], (PRUint32) m3[4], (PRUint32) m3[5],
                (PRUint32) m3[6], (PRUint32) m3[7]);
  }
  return res;
}

