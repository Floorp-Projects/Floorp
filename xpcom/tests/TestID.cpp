/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#include <stdio.h>
#include "plstr.h"
#include "nsID.h"

static char* ids[] = {
  "5C347B10-D55C-11D1-89B7-006008911B81",
  "{5C347B10-D55C-11D1-89B7-006008911B81}",
  "5c347b10-d55c-11d1-89b7-006008911b81",
  "{5c347b10-d55c-11d1-89b7-006008911b81}",

  "FC347B10-D55C-F1D1-F9B7-006008911B81",
  "{FC347B10-D55C-F1D1-F9B7-006008911B81}",
  "fc347b10-d55c-f1d1-f9b7-006008911b81",
  "{fc347b10-d55c-f1d1-f9b7-006008911b81}",
};
#define NUM_IDS ((int) (sizeof(ids) / sizeof(ids[0])))

int main(int argc, char** argv)
{
  nsID id;
  for (int i = 0; i < NUM_IDS; i++) {
    char* idstr = ids[i];
    if (!id.Parse(idstr)) {
      fprintf(stderr, "TestID: Parse failed on test #%d\n", i);
      return -1;
    }
    char* cp = id.ToString();
    if (NULL == cp) {
      fprintf(stderr, "TestID: ToString failed on test #%d\n", i);
      return -1;
    }
    if (0 != PL_strcmp(cp, ids[4*(i/4) + 3])) {
      fprintf(stderr, "TestID: compare of ToString failed on test #%d\n", i);
      return -1;
    }
  }

  return 0;
}
