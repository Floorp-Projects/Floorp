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
    if ((buf[0] == 'g' || buf[0] == 'G') &&
        (buf[1] == 't' || buf[1] == 'T'))
        return PRUnichar('>');

    if ((buf[0] == 'l' || buf[0] == 'L') &&
        (buf[1] == 't' || buf[1] == 'T'))
        return PRUnichar('<');

    if ((buf[0] == 'a' || buf[0] == 'A') &&
        (buf[1] == 'm' || buf[1] == 'M') &&
        (buf[2] == 'p' || buf[2] == 'P'))
        return PRUnichar('&');

    NS_NOTYETIMPLEMENTED("this is a named entity that I can't handle...");
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
    PRInt32 index = 0;
    while (index < aResult.Length()) {
        // If we have the start of an entity (and it's not at the end of
        // our string) then translate the entity into it's unicode value.
        if ((aResult.CharAt(index++) == '&') && (index < aResult.Length())) {
            PRInt32 start = index - 1;
            PRUnichar e = aResult.CharAt(index);
            if (e == '#') {
                // Convert a numeric character reference
                index++;
                char* cp = cbuf;
                char* limit = cp + sizeof(cbuf) - 1;
                PRBool ok = PR_FALSE;
                PRInt32 slen = aResult.Length();
                while ((index < slen) && (cp < limit)) {
                    PRUnichar e = aResult.CharAt(index);
                    if (e == ';') {
                        index++;
                        ok = PR_TRUE;
                        break;
                    }
                    if ((e >= '0') && (e <= '9')) {
                        *cp++ = char(e);
                        index++;
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
                aResult.Cut(start, index - start);
                aResult.Insert(PRUnichar(ch), start);
                index = start + 1;
            }
            else if (((e >= 'A') && (e <= 'Z')) ||
                     ((e >= 'a') && (e <= 'z'))) {
                // Convert a named entity
                index++;
                char* cp = cbuf;
                char* limit = cp + sizeof(cbuf) - 1;
                *cp++ = char(e);
                PRBool ok = PR_FALSE;
                PRInt32 slen = aResult.Length();
                while ((index < slen) && (cp < limit)) {
                    PRUnichar e = aResult.CharAt(index);
                    if (e == ';') {
                        index++;
                        ok = PR_TRUE;
                        break;
                    }
                    if (((e >= '0') && (e <= '9')) ||
                        ((e >= 'A') && (e <= 'Z')) ||
                        ((e >= 'a') && (e <= 'z'))) {
                        *cp++ = char(e);
                        index++;
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
                aResult.Cut(start, index - start);
                aResult.Insert(PRUnichar(ch), start);
                index = start + 1;
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
        offset = aSource.Find('=', offset);

        PRUnichar next = aSource.CharAt(++offset);
        if (kQuote == next) {
            endOffset = aSource.Find(kQuote, ++offset);
        }
        else if (kApostrophe == next) {
            endOffset = aSource.Find(kApostrophe, ++offset);	  
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


void
nsRDFParserUtils::FullyQualifyURI(const nsIURL* base, nsString& spec)
{
    // This is a fairly heavy-handed way to do this, but...I don't
    // like typing.
    nsIURL* url;
    if (NS_SUCCEEDED(NS_NewURL(&url, spec, base))) {
        PRUnichar* str;
        url->ToString(&str);
        spec = str;
        delete str;
        url->Release();
    }
}

