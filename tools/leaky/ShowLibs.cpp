/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Kipp E.B. Hickman.
 * Portions created by the Initial Developer are Copyright (C) 1999
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
