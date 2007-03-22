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
 * The Initial Developer of the Original Code is
 * Kipp E.B. Hickman.
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
#include <malloc.h>

void s1(int, int)
{
    malloc(100);
}

void s2()
{
    s1(1, 2);
    malloc(100);
}

void s3()
{
    s2();
    malloc(100);
    malloc(200);
}

void s4()
{
    s3();
    char* cp = new char[300];
    cp = cp;
}

// Test that mutually recrusive methods don't foul up the graph output
void s6(int recurse);

void s5(int recurse)
{
    malloc(100);
    if (recurse > 0) {
      s6(recurse - 1);
    }
}

void s6(int recurse)
{
    malloc(100);
    if (recurse > 0) {
      s5(recurse - 1);
    }
}

// Test that two pathways through the same node don't cause replicated
// descdendants (A -> B -> C, X -> B -> D shouldn't produce a graph
// that shows A -> B -> D!)

void C()
{
  malloc(10);
}

void D()
{
  malloc(10);
}

void B(int way)
{
  malloc(10);
  if (way) {
    C();
    C();
    C();
  } else {
    D();
  }
}

void A()
{
  malloc(10);
  B(1);
}

void X()
{
  malloc(10);
  B(0);
}

int main()
{
  s1(1, 2);
  s2();
  s3();
  s4();
  s5(10);
  A();
  X();
}
