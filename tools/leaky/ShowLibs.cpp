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

#include <stdio.h>
#include <dlfcn.h>

#ifdef linux
#include <link.h>
#endif

// A simple test program that dumps out the loaded shared
// libraries. This is essential for leaky to work properly when shared
// libraries are used.
static void ShowLibs(struct r_debug* rd)
{
  link_map* map = rd->r_map;
  while (NULL != map) {
    printf("addr=%08x name=%s prev=%p next=%p\n", map->l_addr, map->l_name,
	   map->l_prev, map->l_next);
    map = map->l_next;
  }
}

int main(int argc, char** argv)
{
  void* h = dlopen("/usr/X11R6/lib/libX11.so", RTLD_LAZY);
#ifdef linux
  printf("Direct r_debug libs:\n");
  ShowLibs(&_r_debug);

  printf("_DYNAMICE r_debug libs:\n");
  ElfW(Dyn)* dp;
  for (dp = _DYNAMIC; dp->d_tag != DT_NULL; dp++) {
    if (dp->d_tag == DT_DEBUG) {
      struct r_debug* rd = (struct r_debug*) dp->d_un.d_ptr;
      ShowLibs(rd);
    }
  }
#endif
  return 0;
}
