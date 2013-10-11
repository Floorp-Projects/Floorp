/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>
#include "plstr.h"
#include "nsID.h"

static const char* const ids[] = {
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
    const char* idstr = ids[i];
    if (!id.Parse(idstr)) {
      fprintf(stderr, "TestID: Parse failed on test #%d\n", i);
      return -1;
    }
    char* cp = id.ToString();
    if (nullptr == cp) {
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
