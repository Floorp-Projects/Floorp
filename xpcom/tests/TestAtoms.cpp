/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
    nsAutoString sb;
    sb.AssignWithConversion(buf);
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
    const PRUnichar *unicodeString;
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
