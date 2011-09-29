/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Bratell <bratell@lysator.liu.se>
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
#include "nsID.h"
#include "prprf.h"
#include "nsMemory.h"

static const char gIDFormat[] = 
  "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}";

static const char gIDFormat2[] = 
  "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x";


/**
 * Multiplies the_int_var with 16 (0x10) and adds the value of the
 * hexadecimal digit the_char. If it fails it returns PR_FALSE from
 * the function it's used in.
 */

#define ADD_HEX_CHAR_TO_INT_OR_RETURN_FALSE(the_char, the_int_var) \
    the_int_var = (the_int_var << 4) + the_char; \
    if(the_char >= '0' && the_char <= '9') the_int_var -= '0'; \
    else if(the_char >= 'a' && the_char <= 'f') the_int_var -= 'a'-10; \
    else if(the_char >= 'A' && the_char <= 'F') the_int_var -= 'A'-10; \
    else return PR_FALSE


/**
 * Parses number_of_chars characters from the char_pointer pointer and
 * puts the number in the dest_variable. The pointer is moved to point
 * at the first character after the parsed ones. If it fails it returns
 * PR_FALSE from the function the macro is used in.
 */

#define PARSE_CHARS_TO_NUM(char_pointer, dest_variable, number_of_chars) \
  do { PRInt32 _i=number_of_chars; \
  dest_variable = 0; \
  while(_i) { \
    ADD_HEX_CHAR_TO_INT_OR_RETURN_FALSE(*char_pointer, dest_variable); \
    char_pointer++; \
    _i--; \
  } } while(0)


/**
 * Parses a hyphen from the char_pointer string. If there is no hyphen there
 * the function returns PR_FALSE from the function it's used in. The
 * char_pointer is advanced one step.
 */

 #define PARSE_HYPHEN(char_pointer)   if(*(char_pointer++) != '-') return PR_FALSE
    
/* 
 * Turns a {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} string into
 * an nsID. It can also handle the old format without the { and }.
 */

bool nsID::Parse(const char *aIDStr)
{
  /* Optimized for speed */
  if(!aIDStr) {
    return PR_FALSE;
  }

  bool expectFormat1 = (aIDStr[0] == '{');
  if(expectFormat1) aIDStr++;

  PARSE_CHARS_TO_NUM(aIDStr, m0, 8);
  PARSE_HYPHEN(aIDStr);
  PARSE_CHARS_TO_NUM(aIDStr, m1, 4);
  PARSE_HYPHEN(aIDStr);
  PARSE_CHARS_TO_NUM(aIDStr, m2, 4);
  PARSE_HYPHEN(aIDStr);
  int i;
  for(i=0; i<2; i++)
    PARSE_CHARS_TO_NUM(aIDStr, m3[i], 2);
  PARSE_HYPHEN(aIDStr);
  while(i < 8) {
    PARSE_CHARS_TO_NUM(aIDStr, m3[i], 2);
    i++;
  }
  
  return expectFormat1 ? *aIDStr == '}' : PR_TRUE;
}

#ifndef XPCOM_GLUE_AVOID_NSPR

/*
 * Returns an allocated string in {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
 * format. The string is allocated with NS_Alloc and should be freed by
 * the caller.
 */

char *nsID::ToString() const 
{
  char *res = (char*)NS_Alloc(NSID_LENGTH);

  if (res != NULL) {
    PR_snprintf(res, NSID_LENGTH, gIDFormat,
                m0, (PRUint32) m1, (PRUint32) m2,
                (PRUint32) m3[0], (PRUint32) m3[1], (PRUint32) m3[2],
                (PRUint32) m3[3], (PRUint32) m3[4], (PRUint32) m3[5],
                (PRUint32) m3[6], (PRUint32) m3[7]);
  }
  return res;
}

void nsID::ToProvidedString(char (&dest)[NSID_LENGTH]) const 
{
  PR_snprintf(dest, NSID_LENGTH, gIDFormat,
              m0, (PRUint32) m1, (PRUint32) m2,
              (PRUint32) m3[0], (PRUint32) m3[1], (PRUint32) m3[2],
              (PRUint32) m3[3], (PRUint32) m3[4], (PRUint32) m3[5],
              (PRUint32) m3[6], (PRUint32) m3[7]);
}

#endif // XPCOM_GLUE_AVOID_NSPR
