/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is ShowAlignments.cpp.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
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

/* show the size and alignment requirements of various types */

#include "nsMemory.h"
#include <stdio.h>

struct S {
  double d;
  char c;
  short s;
};

int main()
{
  static const char str[] =
    "Type %s has size %u and alignment requirement %u\n";
  #define SHOW_TYPE(t_) \
    printf(str, #t_, unsigned(sizeof(t_)), unsigned(NS_ALIGNMENT_OF(t_)))

  SHOW_TYPE(char);
  SHOW_TYPE(unsigned short);
  SHOW_TYPE(int);
  SHOW_TYPE(long);
  SHOW_TYPE(PRUint8);
  SHOW_TYPE(PRInt16);
  SHOW_TYPE(PRUint32);
  SHOW_TYPE(PRFloat64);
  SHOW_TYPE(void*);
  SHOW_TYPE(double);
  SHOW_TYPE(short[7]);
  SHOW_TYPE(S);
  SHOW_TYPE(double S::*);

  return 0;
}
