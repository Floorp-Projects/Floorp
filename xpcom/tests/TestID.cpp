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
#define NUM_IDS (sizeof(ids) / sizeof(ids[0]))

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
