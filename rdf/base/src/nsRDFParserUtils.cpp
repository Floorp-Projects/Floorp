/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*

  Some useful parsing routines.

  This isn't the best place for them: I wish that they'd go into some
  shared area (like mozilla/base).

 */

#include <stdlib.h> // XXX for atoi(), maybe this should go into nsCRT?
#include "nsCRT.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsRDFParserUtils.h"

// XXX This totally sucks. I wish that mozilla/base had this code.
PRUnichar
nsRDFParserUtils::EntityToUnicode(const char* buf)
{
    if ((buf[0] == 'g') &&
        (buf[1] == 't') &&
        (buf[2] == '\0'))
        return PRUnichar('>');

    if ((buf[0] == 'l') &&
        (buf[1] == 't') &&
        (buf[2] == '\0'))
        return PRUnichar('<');

    if ((buf[0] == 'a') &&
        (buf[1] == 'm') &&
        (buf[2] == 'p') &&
        (buf[3] == '\0'))
        return PRUnichar('&');

    if ((buf[0] == 'a') &&
        (buf[1] == 'p') &&
        (buf[2] == 'o') &&
        (buf[3] == 's') &&
        (buf[4] == '\0'))
        return PRUnichar('\'');

    if ((buf[0] == 'q') &&
        (buf[1] == 'u') &&
        (buf[2] == 'o') &&
        (buf[3] == 't') &&
        (buf[4] == '\0'))
        return PRUnichar('"');

    NS_NOTYETIMPLEMENTED("look this up in the declared-entity table");
    return PRUnichar('?');
}

// XXX Code copied from nsHTMLContentSink. It should be shared.
void
nsRDFParserUtils::StripAndConvert(nsString& aResult)
{
    // Strip quotes if present
    PRUnichar first = aResult.First();
    if ((first == '"') || (first == '\'')) {
        if (aResult.Last() == first) {
            aResult.Cut(0, 1);
            PRInt32 pos = aResult.Length() - 1;
            if (pos >= 0) {
                aResult.Cut(pos, 1);
            }
        } else {
            // Mismatched quotes - leave them in
        }
    }

    // Reduce any entities
    // XXX Note: as coded today, this will only convert well formed
    // entities.  This may not be compatible enough.
    // XXX there is a table in navigator that translates some numeric entities
    // should we be doing that? If so then it needs to live in two places (bad)
    // so we should add a translate numeric entity method from the parser...
    char cbuf[100];
    PRInt32 i = 0;
    while (i < aResult.Length()) {
        // If we have the start of an entity (and it's not at the end of
        // our string) then translate the entity into it's unicode value.
        if ((aResult.CharAt(i++) == '&') && (i < aResult.Length())) {
            PRInt32 start = i - 1;
            PRUnichar e = aResult.CharAt(i);
            if (e == '#') {
                // Convert a numeric character reference
                i++;
                char* cp = cbuf;
                char* limit = cp + sizeof(cbuf) - 1;
                PRBool ok = PR_FALSE;
                PRInt32 slen = aResult.Length();
                while ((i < slen) && (cp < limit)) {
                    PRUnichar f = aResult.CharAt(i);
                    if (f == ';') {
                        i++;
                        ok = PR_TRUE;
                        break;
                    }
                    if ((f >= '0') && (f <= '9')) {
                        *cp++ = char(f);
                        i++;
                        continue;
                    }
                    break;
                }
                if (!ok || (cp == cbuf)) {
                    continue;
                }
                *cp = '\0';
                if (cp - cbuf > 5) {
                    continue;
                }
                PRInt32 ch = PRInt32( ::atoi(cbuf) );
                if (ch > 65535) {
                    continue;
                }

                // Remove entity from string and replace it with the integer
                // value.
                aResult.Cut(start, i - start);
                aResult.Insert(PRUnichar(ch), start);
                i = start + 1;
            }
            else if (((e >= 'A') && (e <= 'Z')) ||
                     ((e >= 'a') && (e <= 'z'))) {
                // Convert a named entity
                i++;
                char* cp = cbuf;
                char* limit = cp + sizeof(cbuf) - 1;
                *cp++ = char(e);
                PRBool ok = PR_FALSE;
                PRInt32 slen = aResult.Length();
                while ((i < slen) && (cp < limit)) {
                    PRUnichar f = aResult.CharAt(i);
                    if (f == ';') {
                        i++;
                        ok = PR_TRUE;
                        break;
                    }
                    if (((f >= '0') && (f <= '9')) ||
                        ((f >= 'A') && (f <= 'Z')) ||
                        ((f >= 'a') && (f <= 'z'))) {
                        *cp++ = char(f);
                        i++;
                        continue;
                    }
                    break;
                }
                if (!ok || (cp == cbuf)) {
                    continue;
                }
                *cp = '\0';
                PRInt32 ch;

                // XXX Um, here's where we should be converting a
                // named entity. I removed this to avoid a link-time
                // dependency on core raptor.
                ch = EntityToUnicode(cbuf);

                if (ch < 0) {
                    continue;
                }

                // Remove entity from string and replace it with the integer
                // value.
                aResult.Cut(start, i - start);
                aResult.Insert(PRUnichar(ch), start);
                i = start + 1;
            }
            else if (e == '{') {
                // Convert a script entity
                // XXX write me!
                NS_NOTYETIMPLEMENTED("convert a script entity");
            }
        }
    }
}

nsresult
nsRDFParserUtils::GetQuotedAttributeValue(const nsString& aSource, 
                                          const nsString& aAttribute,
                                          nsString& aValue)
{
static const char kQuote = '\"';
static const char kApostrophe = '\'';

    PRInt32 offset;
    PRInt32 endOffset = -1;
    nsresult result = NS_OK;

    offset = aSource.Find(aAttribute);
    if (-1 != offset) {
        offset = aSource.FindChar('=', PR_FALSE,offset);

        PRUnichar next = aSource.CharAt(++offset);
        if (kQuote == next) {
            endOffset = aSource.FindChar(kQuote, PR_FALSE,++offset);
        }
        else if (kApostrophe == next) {
            endOffset = aSource.FindChar(kApostrophe, PR_FALSE,++offset);	  
        }
  
        if (-1 != endOffset) {
            aSource.Mid(aValue, offset, endOffset-offset);
        }
        else {
            // Mismatched quotes - return an error
            result = NS_ERROR_FAILURE;
        }
    }
    else {
        aValue.Truncate();
    }

    return result;
}


PRBool
nsRDFParserUtils::IsJavaScriptLanguage(const nsString& aName)
{
  if (aName.EqualsIgnoreCase("JavaScript") || 
      aName.EqualsIgnoreCase("LiveScript") || 
      aName.EqualsIgnoreCase("Mocha")) { 
    return PR_TRUE;
  } 
  else if (aName.EqualsIgnoreCase("JavaScript1.1")) { 
    return PR_TRUE;
  } 
  else if (aName.EqualsIgnoreCase("JavaScript1.2")) { 
    return PR_TRUE;
  } 
  else if (aName.EqualsIgnoreCase("JavaScript1.3")) { 
    return PR_TRUE;
  } 
  else if (aName.EqualsIgnoreCase("JavaScript1.4")) { 
    return PR_TRUE;
  } 
  else { 
    return PR_FALSE;
  } 
}
