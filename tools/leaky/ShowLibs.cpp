// The contents of this file are subject to the Mozilla Public License
// Version 1.0 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License
// at http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS"
// basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
// the License for the specific language governing rights and
// limitations under the License.
//
// The Initial Developer of the Original Code is Kipp E.B. Hickman.

#include <stdio.h>
#include <dlfcn.h>

#ifdef linux
#include <link.h>
#endif

// A simple test program that dumps out the loaded shared
// libraries. This is essential for leaky to work properly when shared
// libraries are used.

int main(int argc, char** argv)
{
#ifdef linux
  link_map* map = _r_debug.r_map;
  while (NULL != map) {
    printf("addr=%08x name=%s\n", map->l_addr, map->l_name);
    map = map->l_next;
  }
#endif
  return 0;
}
