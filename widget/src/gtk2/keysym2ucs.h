/*
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
 * The Original Code is from xterm-122 source
 * XFree86: xc/programs/xterm/keysym2ucs.h,v 1.1 1999/06/12 15:37:18 dawes Exp 
 *
 * The Initial Developer of the Original Code is 
 * Markus G. Kuhn <mkuhn@acm.org>, University of Cambridge, June 1999
 * Richard Verhoeven <river@win.tue.nl> 
 *
 * Contributor(s):
 * Frank Tang <ftang@netscape.com> adopt into mozilla
 */
/*
 * This module converts keysym values into the corresponding ISO 10646-1
 * (UCS, Unicode) values.
 */

#include <X11/X.h>

#ifdef __cplusplus
extern "C" { 
#endif

long keysym2ucs(KeySym keysym); 

#ifdef __cplusplus
} /* extern "C" */
#endif



