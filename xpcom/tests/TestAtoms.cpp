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
#include "nsIAtom.h"
#include "nsString.h"
#include "prprf.h"
#include "prtime.h"
#include <stdio.h>

extern "C" int _CrtSetDbgFlag(int);

int main(int argc, char** argv)
{
  FILE* fp = fopen("words.txt", "r");
  if (nsnull == fp) {
    printf("can't open words.txt\n");
    return -1;
  }

  PRInt32 count = 0;
  PRUnichar** strings = new PRUnichar*[60000];
  nsIAtom** ids = new nsIAtom*[60000];
  nsAutoString s1, s2;
  PRTime start = PR_Now();
  PRInt32 i;
  for (i = 0; i < 60000; i++) {
    char buf[1000];
    char* s = fgets(buf, sizeof(buf), fp);
    if (nsnull == s) {
      break;
    }
    nsAutoString sb(buf);
    strings[count++] = sb.ToNewUnicode();
    sb.ToUpperCase();
    strings[count++] = sb.ToNewUnicode();
  }
  PRTime end0 = PR_Now();

  // Find and create idents
  for (i = 0; i < count; i++) {
    ids[i] = NS_NewAtom(strings[i]);
  }
  PRUnichar qqs[1]; qqs[0] = 0;
  nsIAtom* qq = NS_NewAtom(qqs);
  PRTime end1 = PR_Now();

  // Now make sure we can find all the idents we just made
  for (i = 0; i < count; i++) {
    PRUnichar *unicodeString;
    ids[i]->GetUnicode(&unicodeString);
    nsIAtom* id = NS_NewAtom(unicodeString);
    if (id != ids[i]) {
      id->ToString(s1);
      ids[i]->ToString(s2);
      printf("find failed: id='%s' ids[%d]='%s'\n",
             s1.ToNewCString(), i, s2.ToNewCString());
      return -1;
    }
    NS_RELEASE(id);
  }
  PRTime end2 = PR_Now();

  // Destroy all the atoms we just made
  NS_RELEASE(qq);
  for (i = 0; i < count; i++) {
    NS_RELEASE(ids[i]);
  }

  // Print out timings
  PRTime end3 = PR_Now();
  PRTime creates, finds, lookups, dtor, ustoms;
  LL_I2L(ustoms, 1000);
  LL_SUB(creates, end0, start);
  LL_DIV(creates, creates, ustoms);
  LL_SUB(finds, end1, end0);
  LL_DIV(finds, finds, ustoms);
  LL_SUB(lookups, end2, end1);
  LL_DIV(lookups, lookups, ustoms);
  LL_SUB(dtor, end3, end2);
  char buf[500];
  PR_snprintf(buf, sizeof(buf), "making %d ident strings took %lldms",
              count, creates);
  puts(buf);
  PR_snprintf(buf, sizeof(buf), "%d new idents took %lldms",
              count, finds);
  puts(buf);
  PR_snprintf(buf, sizeof(buf), "%d ident lookups took %lldms",
              count, lookups);
  puts(buf);
  PR_snprintf(buf, sizeof(buf), "dtor took %lldusec", dtor);
  puts(buf);

  printf("%d live atoms\n", NS_GetNumberOfAtoms());
  NS_POSTCONDITION(0 == NS_GetNumberOfAtoms(), "dangling atoms");

  return 0;
}
