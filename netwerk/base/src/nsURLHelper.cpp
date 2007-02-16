/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cindent: */
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
 * Andreas Otte.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#include "nsURLHelper.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIURLParser.h"
#include "nsIURI.h"
#include "nsMemory.h"
#include "nsEscape.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsNetCID.h"
#include "netCore.h"
#include "prprf.h"

//----------------------------------------------------------------------------
// Init/Shutdown
//----------------------------------------------------------------------------

static PRBool gInitialized = PR_FALSE;
static nsIURLParser *gNoAuthURLParser = nsnull;
static nsIURLParser *gAuthURLParser = nsnull;
static nsIURLParser *gStdURLParser = nsnull;

static void
InitGlobals()
{
    nsCOMPtr<nsIURLParser> parser;

    parser = do_GetService(NS_NOAUTHURLPARSER_CONTRACTID);
    NS_ASSERTION(parser, "failed getting 'noauth' url parser");
    if (parser) {
        gNoAuthURLParser = parser.get();
        NS_ADDREF(gNoAuthURLParser);
    }

    parser = do_GetService(NS_AUTHURLPARSER_CONTRACTID);
    NS_ASSERTION(parser, "failed getting 'auth' url parser");
    if (parser) {
        gAuthURLParser = parser.get();
        NS_ADDREF(gAuthURLParser);
    }

    parser = do_GetService(NS_STDURLPARSER_CONTRACTID);
    NS_ASSERTION(parser, "failed getting 'std' url parser");
    if (parser) {
        gStdURLParser = parser.get();
        NS_ADDREF(gStdURLParser);
    }

    gInitialized = PR_TRUE;
}

void
net_ShutdownURLHelper()
{
    if (gInitialized) {
        NS_IF_RELEASE(gNoAuthURLParser);
        NS_IF_RELEASE(gAuthURLParser);
        NS_IF_RELEASE(gStdURLParser);
        gInitialized = PR_FALSE;
    }
}

//----------------------------------------------------------------------------
// nsIURLParser getters
//----------------------------------------------------------------------------

nsIURLParser *
net_GetAuthURLParser()
{
    if (!gInitialized)
        InitGlobals();
    return gAuthURLParser;
}

nsIURLParser *
net_GetNoAuthURLParser()
{
    if (!gInitialized)
        InitGlobals();
    return gNoAuthURLParser;
}

nsIURLParser *
net_GetStdURLParser()
{
    if (!gInitialized)
        InitGlobals();
    return gStdURLParser;
}

//----------------------------------------------------------------------------
// file:// URL parsing
//----------------------------------------------------------------------------

nsresult
net_ParseFileURL(const nsACString &inURL,
                 nsACString &outDirectory,
                 nsACString &outFileBaseName,
                 nsACString &outFileExtension)
{
    nsresult rv;

    outDirectory.Truncate();
    outFileBaseName.Truncate();
    outFileExtension.Truncate();

    const nsPromiseFlatCString &flatURL = PromiseFlatCString(inURL);
    const char *url = flatURL.get();
    
    PRUint32 schemeBeg, schemeEnd;
    rv = net_ExtractURLScheme(flatURL, &schemeBeg, &schemeEnd, nsnull);
    if (NS_FAILED(rv)) return rv;

    if (strncmp(url + schemeBeg, "file", schemeEnd - schemeBeg) != 0) {
        NS_ERROR("must be a file:// url");
        return NS_ERROR_UNEXPECTED;
    }

    nsIURLParser *parser = net_GetNoAuthURLParser();
    NS_ENSURE_TRUE(parser, NS_ERROR_UNEXPECTED);

    PRUint32 pathPos, filepathPos, directoryPos, basenamePos, extensionPos;
    PRInt32 pathLen, filepathLen, directoryLen, basenameLen, extensionLen;

    // invoke the parser to extract the URL path
    rv = parser->ParseURL(url, flatURL.Length(),
                          nsnull, nsnull, // don't care about scheme
                          nsnull, nsnull, // don't care about authority
                          &pathPos, &pathLen);
    if (NS_FAILED(rv)) return rv;

    // invoke the parser to extract filepath from the path
    rv = parser->ParsePath(url + pathPos, pathLen,
                           &filepathPos, &filepathLen,
                           nsnull, nsnull,  // don't care about param
                           nsnull, nsnull,  // don't care about query
                           nsnull, nsnull); // don't care about ref
    if (NS_FAILED(rv)) return rv;

    filepathPos += pathPos;

    // invoke the parser to extract the directory and filename from filepath
    rv = parser->ParseFilePath(url + filepathPos, filepathLen,
                               &directoryPos, &directoryLen,
                               &basenamePos, &basenameLen,
                               &extensionPos, &extensionLen);
    if (NS_FAILED(rv)) return rv;

    if (directoryLen > 0)
        outDirectory = Substring(inURL, filepathPos + directoryPos, directoryLen);
    if (basenameLen > 0)
        outFileBaseName = Substring(inURL, filepathPos + basenamePos, basenameLen);
    if (extensionLen > 0)
        outFileExtension = Substring(inURL, filepathPos + extensionPos, extensionLen);
    // since we are using a no-auth url parser, there will never be a host
    // XXX not strictly true... file://localhost/foo/bar.html is a valid URL

    return NS_OK;
}

//----------------------------------------------------------------------------
// path manipulation functions
//----------------------------------------------------------------------------

// Replace all /./ with a / while resolving URLs
// But only till #? 
void 
net_CoalesceDirs(netCoalesceFlags flags, char* path)
{
    /* Stolen from the old netlib's mkparse.c.
     *
     * modifies a url of the form   /foo/../foo1  ->  /foo1
     *                       and    /foo/./foo1   ->  /foo/foo1
     *                       and    /foo/foo1/..  ->  /foo/
     */
    char *fwdPtr = path;
    char *urlPtr = path;
    char *lastslash = path;
    PRUint32 traversal = 0;
    PRUint32 special_ftp_len = 0;

    /* Remember if this url is a special ftp one: */
    if (flags & NET_COALESCE_DOUBLE_SLASH_IS_ROOT) 
    {
       /* some schemes (for example ftp) have the speciality that 
          the path can begin // or /%2F to mark the root of the 
          servers filesystem, a simple / only marks the root relative 
          to the user loging in. We remember the length of the marker */
        if (nsCRT::strncasecmp(path,"/%2F",4) == 0)
            special_ftp_len = 4;
        else if (nsCRT::strncmp(path,"//",2) == 0 )
            special_ftp_len = 2; 
    }

    /* find the last slash before # or ? */
    for(; (*fwdPtr != '\0') && 
            (*fwdPtr != '?') && 
            (*fwdPtr != '#'); ++fwdPtr)
    {
    }

    /* found nothing, but go back one only */
    /* if there is something to go back to */
    if (fwdPtr != path && *fwdPtr == '\0')
    {
        --fwdPtr;
    }

    /* search the slash */
    for(; (fwdPtr != path) && 
            (*fwdPtr != '/'); --fwdPtr)
    {
    }
    lastslash = fwdPtr;
    fwdPtr = path;

    /* replace all %2E or %2e with . in the path */
    /* but stop at lastchar if non null */
    for(; (*fwdPtr != '\0') && 
            (*fwdPtr != '?') && 
            (*fwdPtr != '#') &&
            (*lastslash == '\0' || fwdPtr != lastslash); ++fwdPtr)
    {
        if (*fwdPtr == '%' && *(fwdPtr+1) == '2' && 
            (*(fwdPtr+2) == 'E' || *(fwdPtr+2) == 'e'))
        {
            *urlPtr++ = '.';
            ++fwdPtr;
            ++fwdPtr;
        } 
        else 
        {
            *urlPtr++ = *fwdPtr;
        }
    }
    // Copy remaining stuff past the #?;
    for (; *fwdPtr != '\0'; ++fwdPtr)
    {
        *urlPtr++ = *fwdPtr;
    }
    *urlPtr = '\0';  // terminate the url 

    // start again, this time for real 
    fwdPtr = path;
    urlPtr = path;

    for(; (*fwdPtr != '\0') && 
            (*fwdPtr != '?') && 
            (*fwdPtr != '#'); ++fwdPtr)
    {
        if (*fwdPtr == '/' && *(fwdPtr+1) == '.' && *(fwdPtr+2) == '/' )
        {
            // remove . followed by slash
            ++fwdPtr;
        }
        else if(*fwdPtr == '/' && *(fwdPtr+1) == '.' && *(fwdPtr+2) == '.' && 
                (*(fwdPtr+3) == '/' || 
                    *(fwdPtr+3) == '\0' || // This will take care of 
                    *(fwdPtr+3) == '?' ||  // something like foo/bar/..#sometag
                    *(fwdPtr+3) == '#'))
        {
            // remove foo/.. 
            // reverse the urlPtr to the previous slash if possible
            // if url does not allow relative root then drop .. above root 
            // otherwise retain them in the path 
            if(traversal > 0 || !(flags & 
                                  NET_COALESCE_ALLOW_RELATIVE_ROOT))
            { 
                if (urlPtr != path)
                    urlPtr--; // we must be going back at least by one 
                for(;*urlPtr != '/' && urlPtr != path; urlPtr--)
                    ;  // null body 
                --traversal; // count back
                // forward the fwdPtr past the ../
                fwdPtr += 2;
                // if we have reached the beginning of the path
                // while searching for the previous / and we remember
                // that it is an url that begins with /%2F then
                // advance urlPtr again by 3 chars because /%2F already 
                // marks the root of the path
                if (urlPtr == path && special_ftp_len > 3) 
                {
                    ++urlPtr;
                    ++urlPtr;
                    ++urlPtr;
                }
                // special case if we have reached the end 
                // to preserve the last /
                if (*fwdPtr == '.' && *(fwdPtr+1) == '\0')
                    ++urlPtr;
            } 
            else 
            {
                // there are to much /.. in this path, just copy them instead.
                // forward the urlPtr past the /.. and copying it

                // However if we remember it is an url that starts with
                // /%2F and urlPtr just points at the "F" of "/%2F" then do 
                // not overwrite it with the /, just copy .. and move forward
                // urlPtr. 
                if (special_ftp_len > 3 && urlPtr == path+special_ftp_len-1)
                    ++urlPtr;
                else 
                    *urlPtr++ = *fwdPtr;
                ++fwdPtr;
                *urlPtr++ = *fwdPtr;
                ++fwdPtr;
                *urlPtr++ = *fwdPtr;
            }
        }
        else
        {
            // count the hierachie, but only if we do not have reached
            // the root of some special urls with a special root marker 
            if (*fwdPtr == '/' &&  *(fwdPtr+1) != '.' &&
               (special_ftp_len != 2 || *(fwdPtr+1) != '/'))
                traversal++;
            // copy the url incrementaly 
            *urlPtr++ = *fwdPtr;
        }
    }
    // Copy remaining stuff past the #?;
    for (; *fwdPtr != '\0'; ++fwdPtr)
    {
        *urlPtr++ = *fwdPtr;
    }
    *urlPtr = '\0';  // terminate the url 

    /* 
     *  Now lets remove trailing . case
     *     /foo/foo1/.   ->  /foo/foo1/
     */

    if ((urlPtr > (path+1)) && (*(urlPtr-1) == '.') && (*(urlPtr-2) == '/'))
        *(urlPtr-1) = '\0';
}

nsresult
net_ResolveRelativePath(const nsACString &relativePath,
                        const nsACString &basePath,
                        nsACString &result)
{
    nsCAutoString name;
    nsCAutoString path(basePath);
    PRBool needsDelim = PR_FALSE;

    if ( !path.IsEmpty() ) {
        PRUnichar last = path.Last();
        needsDelim = !(last == '/');
    }

    nsACString::const_iterator beg, end;
    relativePath.BeginReading(beg);
    relativePath.EndReading(end);

    PRBool stop = PR_FALSE;
    char c;
    for (; !stop; ++beg) {
        c = (beg == end) ? '\0' : *beg;
        //printf("%c [name=%s] [path=%s]\n", c, name.get(), path.get());
        switch (c) {
          case '\0':
          case '#':
          case ';':
          case '?':
            stop = PR_TRUE;
            // fall through...
          case '/':
            // delimiter found
            if (name.EqualsLiteral("..")) {
                // pop path
                // If we already have the delim at end, then
                //  skip over that when searching for next one to the left
                PRInt32 offset = path.Length() - (needsDelim ? 1 : 2);
                // First check for errors
                if (offset < 0 ) 
                    return NS_ERROR_MALFORMED_URI;
                PRInt32 pos = path.RFind("/", PR_FALSE, offset);
                if (pos >= 0)
                    path.Truncate(pos + 1);
                else
                    path.Truncate();
            }
            else if (name.IsEmpty() || name.EqualsLiteral(".")) {
                // do nothing
            }
            else {
                // append name to path
                if (needsDelim)
                    path += '/';
                path += name;
                needsDelim = PR_TRUE;
            }
            name.Truncate();
            break;

          default:
            // append char to name
            name += c;
        }
    }
    // append anything left on relativePath (e.g. #..., ;..., ?...)
    if (c != '\0')
        path += Substring(--beg, end);

    result = path;
    return NS_OK;
}

//----------------------------------------------------------------------------
// scheme fu
//----------------------------------------------------------------------------

/* Extract URI-Scheme if possible */
nsresult
net_ExtractURLScheme(const nsACString &inURI,
                     PRUint32 *startPos, 
                     PRUint32 *endPos,
                     nsACString *scheme)
{
    // search for something up to a colon, and call it the scheme
    const nsPromiseFlatCString &flatURI = PromiseFlatCString(inURI);
    const char* uri_start = flatURI.get();
    const char* uri = uri_start;

    if (!uri)
        return NS_ERROR_MALFORMED_URI;

    // skip leading white space
    while (nsCRT::IsAsciiSpace(*uri))
        uri++;

    PRUint32 start = uri - uri_start;
    if (startPos) {
        *startPos = start;
    }

    PRUint32 length = 0;
    char c;
    while ((c = *uri++) != '\0') {
        // First char must be Alpha
        if (length == 0 && nsCRT::IsAsciiAlpha(c)) {
            length++;
        } 
        // Next chars can be alpha + digit + some special chars
        else if (length > 0 && (nsCRT::IsAsciiAlpha(c) || 
                 nsCRT::IsAsciiDigit(c) || c == '+' || 
                 c == '.' || c == '-')) {
            length++;
        }
        // stop if colon reached but not as first char
        else if (c == ':' && length > 0) {
            if (endPos) {
                *endPos = start + length;
            }

            if (scheme)
                scheme->Assign(Substring(inURI, start, length));
            return NS_OK;
        }
        else 
            break;
    }
    return NS_ERROR_MALFORMED_URI;
}

PRBool
net_IsValidScheme(const char *scheme, PRUint32 schemeLen)
{
    // first char must be alpha
    if (!nsCRT::IsAsciiAlpha(*scheme))
        return PR_FALSE;

    // nsCStrings may have embedded nulls -- reject those too
    for (; schemeLen; ++scheme, --schemeLen) {
        if (!(nsCRT::IsAsciiAlpha(*scheme) ||
              nsCRT::IsAsciiDigit(*scheme) ||
              *scheme == '+' ||
              *scheme == '.' ||
              *scheme == '-'))
            return PR_FALSE;
    }

    return PR_TRUE;
}

PRBool
net_FilterURIString(const char *str, nsACString& result)
{
    NS_PRECONDITION(str, "Must have a non-null string!");
    PRBool writing = PR_FALSE;
    result.Truncate();
    const char *p = str;

    // Remove leading spaces, tabs, CR, LF if any.
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        writing = PR_TRUE;
        str = p + 1;
        p++;
    }

    while (*p) {
        if (*p == '\t' || *p == '\r' || *p == '\n') {
            writing = PR_TRUE;
            // append chars up to but not including *p
            if (p > str)
                result.Append(str, p - str);
            str = p + 1;
        }
        p++;
    }

    // Remove trailing spaces if any
    while (((p-1) >= str) && (*(p-1) == ' ')) {
        writing = PR_TRUE;
        p--;
    }

    if (writing && p > str)
        result.Append(str, p - str);

    return writing;
}

#if defined(XP_WIN) || defined(XP_OS2)
PRBool
net_NormalizeFileURL(const nsACString &aURL, nsCString &aResultBuf)
{
    PRBool writing = PR_FALSE;

    nsACString::const_iterator beginIter, endIter;
    aURL.BeginReading(beginIter);
    aURL.EndReading(endIter);

    const char *s, *begin = beginIter.get();

    for (s = begin; s != endIter.get(); ++s)
    {
        if (*s == '\\')
        {
            writing = PR_TRUE;
            if (s > begin)
                aResultBuf.Append(begin, s - begin);
            aResultBuf += '/';
            begin = s + 1;
        }
    }
    if (writing && s > begin)
        aResultBuf.Append(begin, s - begin);

    return writing;
}
#endif

//----------------------------------------------------------------------------
// miscellaneous (i.e., stuff that should really be elsewhere)
//----------------------------------------------------------------------------

static inline
void ToLower(char &c)
{
    if ((unsigned)(c - 'A') <= (unsigned)('Z' - 'A'))
        c += 'a' - 'A';
}

void
net_ToLowerCase(char *str, PRUint32 length)
{
    for (char *end = str + length; str < end; ++str)
        ToLower(*str);
}

void
net_ToLowerCase(char *str)
{
    for (; *str; ++str)
        ToLower(*str);
}

char *
net_FindCharInSet(const char *iter, const char *stop, const char *set)
{
    for (; iter != stop && *iter; ++iter) {
        for (const char *s = set; *s; ++s) {
            if (*iter == *s)
                return (char *) iter;
        }
    }
    return (char *) iter;
}

char *
net_FindCharNotInSet(const char *iter, const char *stop, const char *set)
{
repeat:
    for (const char *s = set; *s; ++s) {
        if (*iter == *s) {
            if (++iter == stop)
                break;
            goto repeat;
        }
    }
    return (char *) iter;
}

char *
net_RFindCharNotInSet(const char *stop, const char *iter, const char *set)
{
    --iter;
    --stop;

    if (iter == stop)
        return (char *) iter;

repeat:
    for (const char *s = set; *s; ++s) {
        if (*iter == *s) {
            if (--iter == stop)
                break;
            goto repeat;
        }
    }
    return (char *) iter;
}

#define HTTP_LWS " \t"

// Return the index of the closing quote of the string, if any
static PRUint32
net_FindStringEnd(const nsCString& flatStr,
                  PRUint32 stringStart,
                  char stringDelim)
{
    NS_ASSERTION(stringStart < flatStr.Length() &&
                 flatStr.CharAt(stringStart) == stringDelim &&
                 (stringDelim == '"' || stringDelim == '\''),
                 "Invalid stringStart");

    const char set[] = { stringDelim, '\\', '\0' };
    do {
        // stringStart points to either the start quote or the last
        // escaped char (the char following a '\\')
                
        // Write to searchStart here, so that when we get back to the
        // top of the loop right outside this one we search from the
        // right place.
        PRUint32 stringEnd = flatStr.FindCharInSet(set, stringStart + 1);
        if (stringEnd == PRUint32(kNotFound))
            return flatStr.Length();

        if (flatStr.CharAt(stringEnd) == '\\') {
            // Hit a backslash-escaped char.  Need to skip over it.
            stringStart = stringEnd + 1;
            if (stringStart == flatStr.Length())
                return stringStart;

            // Go back to looking for the next escape or the string end
            continue;
        }

        return stringEnd;

    } while (PR_TRUE);

    NS_NOTREACHED("How did we get here?");
    return flatStr.Length();
}
                  

static PRUint32
net_FindMediaDelimiter(const nsCString& flatStr,
                       PRUint32 searchStart,
                       char delimiter)
{
    do {
        // searchStart points to the spot from which we should start looking
        // for the delimiter.
        const char delimStr[] = { delimiter, '"', '\'', '\0' };
        PRUint32 curDelimPos = flatStr.FindCharInSet(delimStr, searchStart);
        if (curDelimPos == PRUint32(kNotFound))
            return flatStr.Length();
            
        char ch = flatStr.CharAt(curDelimPos);
        if (ch == delimiter) {
            // Found delimiter
            return curDelimPos;
        }

        // We hit the start of a quoted string.  Look for its end.
        searchStart = net_FindStringEnd(flatStr, curDelimPos, ch);
        if (searchStart == flatStr.Length())
            return searchStart;

        ++searchStart;

        // searchStart now points to the first char after the end of the
        // string, so just go back to the top of the loop and look for
        // |delimiter| again.
    } while (PR_TRUE);

    NS_NOTREACHED("How did we get here?");
    return flatStr.Length();
}

static void
net_ParseMediaType(const nsACString &aMediaTypeStr,
                   nsACString       &aContentType,
                   nsACString       &aContentCharset,
                   PRBool           *aHadCharset)
{
    const nsCString& flatStr = PromiseFlatCString(aMediaTypeStr);
    const char* start = flatStr.get();
    const char* end = start + flatStr.Length();

    // Trim LWS leading and trailing whitespace from type.  We include '(' in
    // the trailing trim set to catch media-type comments, which are not at all
    // standard, but may occur in rare cases.
    const char* type = net_FindCharNotInSet(start, end, HTTP_LWS);
    const char* typeEnd = net_FindCharInSet(type, end, HTTP_LWS ";(");

    const char* charset = "";
    const char* charsetEnd = charset;

    // Iterate over parameters
    PRBool typeHasCharset = PR_FALSE;
    PRUint32 paramStart = flatStr.FindChar(';', typeEnd - start);
    if (paramStart != PRUint32(kNotFound)) {
        // We have parameters.  Iterate over them.
        PRUint32 curParamStart = paramStart + 1;
        do {
            PRUint32 curParamEnd =
                net_FindMediaDelimiter(flatStr, curParamStart, ';');

            const char* paramName = net_FindCharNotInSet(start + curParamStart,
                                                         start + curParamEnd,
                                                         HTTP_LWS);
            static const char charsetStr[] = "charset=";
            if (PL_strncasecmp(paramName, charsetStr,
                               sizeof(charsetStr) - 1) == 0) {
                charset = paramName + sizeof(charsetStr) - 1;
                charsetEnd = start + curParamEnd;
                typeHasCharset = PR_TRUE;
            }

            curParamStart = curParamEnd + 1;
        } while (curParamStart < flatStr.Length());
    }

    if (typeHasCharset) {
        // Trim LWS leading and trailing whitespace from charset.  We include
        // '(' in the trailing trim set to catch media-type comments, which are
        // not at all standard, but may occur in rare cases.
        charset = net_FindCharNotInSet(charset, charsetEnd, HTTP_LWS);
        if (*charset == '"' || *charset == '\'') {
            charsetEnd =
                start + net_FindStringEnd(flatStr, charset - start, *charset);
            charset++;
            NS_ASSERTION(charsetEnd >= charset, "Bad charset parsing");
        } else {
            charsetEnd = net_FindCharInSet(charset, charsetEnd, HTTP_LWS ";(");
        }
    }

    // if the server sent "*/*", it is meaningless, so do not store it.
    // also, if type is the same as aContentType, then just update the
    // charset.  however, if charset is empty and aContentType hasn't
    // changed, then don't wipe-out an existing aContentCharset.  We
    // also want to reject a mime-type if it does not include a slash.
    // some servers give junk after the charset parameter, which may
    // include a comma, so this check makes us a bit more tolerant.

    if (type != typeEnd && strncmp(type, "*/*", typeEnd - type) != 0 &&
        memchr(type, '/', typeEnd - type) != NULL) {
        // Common case here is that aContentType is empty
        PRBool eq = !aContentType.IsEmpty() &&
            aContentType.Equals(Substring(type, typeEnd),
                                nsCaseInsensitiveCStringComparator());
        if (!eq) {
            aContentType.Assign(type, typeEnd - type);
            ToLowerCase(aContentType);
        }
        if ((!eq && *aHadCharset) || typeHasCharset) {
            *aHadCharset = PR_TRUE;
            aContentCharset.Assign(charset, charsetEnd - charset);
        }
    }
}

#undef HTTP_LWS

void
net_ParseContentType(const nsACString &aHeaderStr,
                     nsACString       &aContentType,
                     nsACString       &aContentCharset,
                     PRBool           *aHadCharset)
{
    //
    // Augmented BNF (from RFC 2616 section 3.7):
    //
    //   header-value = media-type *( LWS "," LWS media-type )
    //   media-type   = type "/" subtype *( LWS ";" LWS parameter )
    //   type         = token
    //   subtype      = token
    //   parameter    = attribute "=" value
    //   attribute    = token
    //   value        = token | quoted-string
    //   
    //
    // Examples:
    //
    //   text/html
    //   text/html, text/html
    //   text/html,text/html; charset=ISO-8859-1
    //   text/html,text/html; charset="ISO-8859-1"
    //   text/html;charset=ISO-8859-1, text/html
    //   text/html;charset='ISO-8859-1', text/html
    //   application/octet-stream
    //

    *aHadCharset = PR_FALSE;
    const nsCString& flatStr = PromiseFlatCString(aHeaderStr);
    
    // iterate over media-types.  Note that ',' characters can happen
    // inside quoted strings, so we need to watch out for that.
    PRUint32 curTypeStart = 0;
    do {
        // curTypeStart points to the start of the current media-type.  We want
        // to look for its end.
        PRUint32 curTypeEnd =
            net_FindMediaDelimiter(flatStr, curTypeStart, ',');
        
        // At this point curTypeEnd points to the spot where the media-type
        // starting at curTypeEnd ends.  Time to parse that!
        net_ParseMediaType(Substring(flatStr, curTypeStart,
                                     curTypeEnd - curTypeStart),
                           aContentType, aContentCharset, aHadCharset);

        // And let's move on to the next media-type
        curTypeStart = curTypeEnd + 1;
    } while (curTypeStart < flatStr.Length());
}

PRBool
net_IsValidHostName(const nsCSubstring &host)
{
    const char *end = host.EndReading();
    // ctrl-chars and  !\"#%&'()*,/;<=>?@\\^{|}\x7f
    // if one of these chars is found return false
    return net_FindCharInSet(host.BeginReading(), end, 
                             "\x01\x02\x03\x04\x05\x06\x07\x08"
                             "\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
                             "\x11\x12\x13\x14\x15\x16\x17\x18"
                             "\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20"
                             "\x21\x22\x23\x25\x26\x27\x28\x29"
                             "\x2a\x2c\x2f\x3b\x3c\x3d\x3e"
                             "\x3f\x40\x5c\x5e\x7b\x7c\x7e\x7f") == end;
}
