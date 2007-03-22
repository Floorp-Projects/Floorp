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
 * The Original Code is ``mcount''
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corp.  Portions created by the Initial Developer are
 * Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
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

/*

  A library that can be LD_PRELOAD-ed to dump a function's address on
  function entry.

 */

#include <unistd.h>

/**
 * When compiled with `-pg' (or even just `-p'), gcc inserts a call to
 * |mcount| in each function prologue. This implementation of |mcount|
 * will grab the return address from the stack, and write it to stdout
 * as a binary stream, creating a gross execution trace the
 * instrumented program.
 *
 * For more info on gcc inline assembly, see:
 *
 *   <http://www.ibm.com/developerworks/linux/library/l-ia.html?dwzone=linux>
 *
 */
void
mcount()
{
    register unsigned int caller;
    unsigned int buf[1];

    // Grab the return address. 
    asm("movl 4(%%esp),%0"
        : "=r"(caller));

    buf[0] = caller;
    write(STDOUT_FILENO, buf, sizeof buf[0]);
}
