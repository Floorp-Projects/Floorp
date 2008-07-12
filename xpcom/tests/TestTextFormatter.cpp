/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Prasad <prasad@medhas.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include <nsTextFormatter.h>
#include <nsStringGlue.h>
#include <stdio.h>

int main() 
{  
  int test_ok = true;

  nsAutoString fmt(NS_LITERAL_STRING("%3$s %4$S %1$d %2$d")); 
  char utf8[] = "Hello"; 
  PRUnichar ucs2[]={'W', 'o', 'r', 'l', 'd', 0x4e00, 0xAc00, 0xFF45, 0x0103, 0x00}; 
  int d=3; 

  PRUnichar buf[256]; 
  nsTextFormatter::snprintf(buf, 256, fmt.get(), d, 333, utf8, ucs2); 
  nsAutoString out(buf); 
  if(strcmp("Hello World", NS_LossyConvertUTF16toASCII(out).get()))
    test_ok = false;

  const PRUnichar *uout = out.get(); 
  const PRUnichar expected[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20,
                                0x57, 0x6F, 0x72, 0x6C, 0x64, 0x4E00,
                                0xAC00, 0xFF45, 0x0103, 0x20, 0x33,
                                0x20, 0x33, 0x33, 0x33};
  for(PRUint32 i=0;i<out.Length();i++) 
    if(uout[i] != expected[i]) 
      test_ok = false;
  printf(test_ok? "nsTextFormatter: OK\n": "nsTextFormatter: FAIL\n");
}

