// The contents of this file are subject to the Mozilla Public License
// Version 1.1 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License
// at http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS"
// basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
// the License for the specific language governing rights and
// limitations under the License.
//
// The Initial Developer of the Original Code is Kipp E.B. Hickman.

#include "TestPreload.h"
#include <malloc.h>
#include <stdio.h>

// This is a simple test program that verifies that you can use
// LD_PRELOAD to override the implementation of malloc for your
// system. It depends on PreloadLib.cpp providing an alternate
// implementation of malloc that has a special hack to return a
// constant address given a particular size request.

int main(int argc, char** argv)
{
  char* p = (char*) malloc(LD_PRELOAD_TEST_MALLOC_SIZE);
  if (p == (char*)LD_PRELOAD_TEST_VALUE) {
    printf("LD_PRELOAD worked - we are using our malloc\n");
  }
  else {
    printf("LD_PRELOAD failed\n");
  }
  return 0;
}
