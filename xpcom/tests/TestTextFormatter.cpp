/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <nsTextFormatter.h>
#include <nsStringGlue.h>
#include <stdio.h>

int main() 
{  
  int test_ok = true;

  nsAutoString fmt(NS_LITERAL_STRING("%3$s %4$S %1$d %2$d %2$d %3$s"));
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
                                0x20, 0x33, 0x33, 0x33, 0x20, 0x33,
                                0x33, 0x33, 0x20, 0x48, 0x65, 0x6C,
                                0x6C, 0x6F};
  for(uint32_t i=0;i<out.Length();i++) {
    if(uout[i] != expected[i]) {
      test_ok = false;
      break;
    }
  }
  printf(test_ok? "nsTextFormatter: OK\n": "nsTextFormatter: FAIL\n");
}

