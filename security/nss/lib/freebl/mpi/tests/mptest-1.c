/*
 * Simple test driver for MPI library
 *
 * Test 1: Simple input test (drives single-digit multiply and add,
 *         as well as I/O routines)
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the MPI Arbitrary Precision Integer Arithmetic
 * library.
 *
 * The Initial Developer of the Original Code is Michael J. Fromberger.
 * Portions created by Michael J. Fromberger are 
 * Copyright (C) 1998, 1999, 2000 Michael J. Fromberger. 
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the GPL.
 *
 *  $Id: mptest-1.c,v 1.1 2000/07/14 00:44:41 nelsonb%netscape.com Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#ifdef MAC_CW_SIOUX
#include <console.h>
#endif

#include "mpi.h"

int main(int argc, char *argv[])
{
  int    ix;
  mp_int mp;

#ifdef MAC_CW_SIOUX
  argc = ccommand(&argv);
#endif

  mp_init(&mp);
  
  for(ix = 1; ix < argc; ix++) {
    mp_read_radix(&mp, argv[ix], 10);
    mp_print(&mp, stdout);
    fputc('\n', stdout);
  }

  mp_clear(&mp);
  return 0;
}
