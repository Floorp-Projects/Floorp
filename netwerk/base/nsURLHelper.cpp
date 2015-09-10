/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RangedPtr.h"

#include "nsURLHelper.h"
#include "nsIFile.h"
#include "nsIURLParser.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsNetCID.h"
#include "mozilla/Preferences.h"
#include "prnetdb.h"

using namespace mozilla;

//----------------------------------------------------------------------------
// Init/Shutdown
//----------------------------------------------------------------------------

static bool gInitialized = false;
static nsIURLParser *gNoAuthURLParser = nullptr;
static nsIURLParser *gAuthURLParser = nullptr;
static nsIURLParser *gStdURLParser = nullptr;
static int32_t gMaxLength = 1048576; // Default: 1MB

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

    gInitialized = true;
#if !defined(MOZILLA_XPCOMRT_API)
    Preferences::AddIntVarCache(&gMaxLength,
                                "network.standard-url.max-length", 1048576);
#endif
}

void
net_ShutdownURLHelper()
{
    if (gInitialized) {
        NS_IF_RELEASE(gNoAuthURLParser);
        NS_IF_RELEASE(gAuthURLParser);
        NS_IF_RELEASE(gStdURLParser);
        gInitialized = false;
    }
}

int32_t net_GetURLMaxLength()
{
    return gMaxLength;
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

//---------------------------------------------------------------------------
// GetFileFromURLSpec implementations
//---------------------------------------------------------------------------
nsresult
net_GetURLSpecFromDir(nsIFile *aFile, nsACString &result)
{
#if defined(MOZILLA_XPCOMRT_API)
    NS_WARNING("net_GetURLSpecFromDir not implemented");
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    nsAutoCString escPath;
    nsresult rv = net_GetURLSpecFromActualFile(aFile, escPath);
    if (NS_FAILED(rv))
        return rv;

    if (escPath.Last() != '/') {
        escPath += '/';
    }
    
    result = escPath;
    return NS_OK;
#endif // defined(MOZILLA_XPCOMRT_API)
}

nsresult
net_GetURLSpecFromFile(nsIFile *aFile, nsACString &result)
{
#if defined(MOZILLA_XPCOMRT_API)
    NS_WARNING("net_GetURLSpecFromFile not implemented");
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    nsAutoCString escPath;
    nsresult rv = net_GetURLSpecFromActualFile(aFile, escPath);
    if (NS_FAILED(rv))
        return rv;

    // if this file references a directory, then we need to ensure that the
    // URL ends with a slash.  this is important since it affects the rules
    // for relative URL resolution when this URL is used as a base URL.
    // if the file does not exist, then we make no assumption about its type,
    // and simply leave the URL unmodified.
    if (escPath.Last() != '/') {
        bool dir;
        rv = aFile->IsDirectory(&dir);
        if (NS_SUCCEEDED(rv) && dir)
            escPath += '/';
    }
    
    result = escPath;
    return NS_OK;
#endif // defined(MOZILLA_XPCOMRT_API)
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

    if (inURL.Length() > (uint32_t) gMaxLength) {
        return NS_ERROR_MALFORMED_URI;
    }

    outDirectory.Truncate();
    outFileBaseName.Truncate();
    outFileExtension.Truncate();

    const nsPromiseFlatCString &flatURL = PromiseFlatCString(inURL);
    const char *url = flatURL.get();
    
    uint32_t schemeBeg, schemeEnd;
    rv = net_ExtractURLScheme(flatURL, &schemeBeg, &schemeEnd, nullptr);
    if (NS_FAILED(rv)) return rv;

    if (strncmp(url + schemeBeg, "file", schemeEnd - schemeBeg) != 0) {
        NS_ERROR("must be a file:// url");
        return NS_ERROR_UNEXPECTED;
    }

    nsIURLParser *parser = net_GetNoAuthURLParser();
    NS_ENSURE_TRUE(parser, NS_ERROR_UNEXPECTED);

    uint32_t pathPos, filepathPos, directoryPos, basenamePos, extensionPos;
    int32_t pathLen, filepathLen, directoryLen, basenameLen, extensionLen;

    // invoke the parser to extract the URL path
    rv = parser->ParseURL(url, flatURL.Length(),
                          nullptr, nullptr, // don't care about scheme
                          nullptr, nullptr, // don't care about authority
                          &pathPos, &pathLen);
    if (NS_FAILED(rv)) return rv;

    // invoke the parser to extract filepath from the path
    rv = parser->ParsePath(url + pathPos, pathLen,
                           &filepathPos, &filepathLen,
                           nullptr, nullptr,  // don't care about query
                           nullptr, nullptr); // don't care about ref
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
    uint32_t traversal = 0;
    uint32_t special_ftp_len = 0;

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

    /* 
     *  Now lets remove trailing . case
     *     /foo/foo1/.   ->  /foo/foo1/
     */

    if ((urlPtr > (path+1)) && (*(urlPtr-1) == '.') && (*(urlPtr-2) == '/'))
        urlPtr--;

    // Copy remaining stuff past the #?;
    for (; *fwdPtr != '\0'; ++fwdPtr)
    {
        *urlPtr++ = *fwdPtr;
    }
    *urlPtr = '\0';  // terminate the url 
}

nsresult
net_ResolveRelativePath(const nsACString &relativePath,
                        const nsACString &basePath,
                        nsACString &result)
{
    nsAutoCString name;
    nsAutoCString path(basePath);
    bool needsDelim = false;

    if ( !path.IsEmpty() ) {
        char16_t last = path.Last();
        needsDelim = !(last == '/');
    }

    nsACString::const_iterator beg, end;
    relativePath.BeginReading(beg);
    relativePath.EndReading(end);

    bool stop = false;
    char c;
    for (; !stop; ++beg) {
        c = (beg == end) ? '\0' : *beg;
        //printf("%c [name=%s] [path=%s]\n", c, name.get(), path.get());
        switch (c) {
          case '\0':
          case '#':
          case '?':
            stop = true;
            // fall through...
          case '/':
            // delimiter found
            if (name.EqualsLiteral("..")) {
                // pop path
                // If we already have the delim at end, then
                //  skip over that when searching for next one to the left
                int32_t offset = path.Length() - (needsDelim ? 1 : 2);
                // First check for errors
                if (offset < 0 ) 
                    return NS_ERROR_MALFORMED_URI;
                int32_t pos = path.RFind("/", false, offset);
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
                needsDelim = true;
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
                     uint32_t *startPos, 
                     uint32_t *endPos,
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

    uint32_t start = uri - uri_start;
    if (startPos) {
        *startPos = start;
    }

    uint32_t length = 0;
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

bool
net_IsValidScheme(const char *scheme, uint32_t schemeLen)
{
    // first char must be alpha
    if (!nsCRT::IsAsciiAlpha(*scheme))
        return false;

    // nsCStrings may have embedded nulls -- reject those too
    for (; schemeLen; ++scheme, --schemeLen) {
        if (!(nsCRT::IsAsciiAlpha(*scheme) ||
              nsCRT::IsAsciiDigit(*scheme) ||
              *scheme == '+' ||
              *scheme == '.' ||
              *scheme == '-'))
            return false;
    }

    return true;
}

bool
net_FilterURIString(const char *str, nsACString& result)
{
    NS_PRECONDITION(str, "Must have a non-null string!");
    bool writing = false;
    result.Truncate();
    const char *p = str;

    // Remove leading spaces, tabs, CR, LF if any.
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        writing = true;
        str = p + 1;
        p++;
    }

    // Don't strip from the scheme, because other code assumes everything
    // up to the ':' is the scheme, and it's bad not to have it match.
    // If there's no ':', strip.
    bool found_colon = false;
    const char *first = nullptr;
    while (*p) {
        switch (*p) {
            case '\t': 
            case '\r': 
            case '\n':
                if (found_colon) {
                    writing = true;
                    // append chars up to but not including *p
                    if (p > str)
                        result.Append(str, p - str);
                    str = p + 1;
                } else {
                    // remember where the first \t\r\n was in case we find no scheme
                    if (!first)
                        first = p;
                }
                break;

            case ':':
                found_colon = true;
                break;

            case '/':
            case '@':
                if (!found_colon) {
                    // colon also has to precede / or @ to be a scheme
                    found_colon = true; // not really, but means ok to strip
                    if (first) {
                        // go back and replace
                        p = first;
                        continue; // process *p again
                    }
                }
                break;

            default:
                break;
        }
        p++;

        // At end, if there was no scheme, and we hit a control char, fix
        // it up now.
        if (!*p && first != nullptr && !found_colon) {
            // TRICKY - to avoid duplicating code, we reset the loop back
            // to the point we found something to do
            p = first;
            // This also stops us from looping after we finish
            found_colon = true; // so we'll replace \t\r\n
        }
    }

    // Remove trailing spaces if any
    while (((p-1) >= str) && (*(p-1) == ' ')) {
        writing = true;
        p--;
    }

    if (writing && p > str)
        result.Append(str, p - str);

    return writing;
}

#if defined(XP_WIN)
bool
net_NormalizeFileURL(const nsACString &aURL, nsCString &aResultBuf)
{
    bool writing = false;

    nsACString::const_iterator beginIter, endIter;
    aURL.BeginReading(beginIter);
    aURL.EndReading(endIter);

    const char *s, *begin = beginIter.get();

    for (s = begin; s != endIter.get(); ++s)
    {
        if (*s == '\\')
        {
            writing = true;
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
net_ToLowerCase(char *str, uint32_t length)
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
static uint32_t
net_FindStringEnd(const nsCString& flatStr,
                  uint32_t stringStart,
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
        uint32_t stringEnd = flatStr.FindCharInSet(set, stringStart + 1);
        if (stringEnd == uint32_t(kNotFound))
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

    } while (true);

    NS_NOTREACHED("How did we get here?");
    return flatStr.Length();
}
                  

static uint32_t
net_FindMediaDelimiter(const nsCString& flatStr,
                       uint32_t searchStart,
                       char delimiter)
{
    do {
        // searchStart points to the spot from which we should start looking
        // for the delimiter.
        const char delimStr[] = { delimiter, '"', '\0' };
        uint32_t curDelimPos = flatStr.FindCharInSet(delimStr, searchStart);
        if (curDelimPos == uint32_t(kNotFound))
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
    } while (true);

    NS_NOTREACHED("How did we get here?");
    return flatStr.Length();
}

// aOffset should be added to aCharsetStart and aCharsetEnd if this
// function sets them.
static void
net_ParseMediaType(const nsACString &aMediaTypeStr,
                   nsACString       &aContentType,
                   nsACString       &aContentCharset,
                   int32_t          aOffset,
                   bool             *aHadCharset,
                   int32_t          *aCharsetStart,
                   int32_t          *aCharsetEnd)
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
    int32_t charsetParamStart = 0;
    int32_t charsetParamEnd = 0;

    // Iterate over parameters
    bool typeHasCharset = false;
    uint32_t paramStart = flatStr.FindChar(';', typeEnd - start);
    if (paramStart != uint32_t(kNotFound)) {
        // We have parameters.  Iterate over them.
        uint32_t curParamStart = paramStart + 1;
        do {
            uint32_t curParamEnd =
                net_FindMediaDelimiter(flatStr, curParamStart, ';');

            const char* paramName = net_FindCharNotInSet(start + curParamStart,
                                                         start + curParamEnd,
                                                         HTTP_LWS);
            static const char charsetStr[] = "charset=";
            if (PL_strncasecmp(paramName, charsetStr,
                               sizeof(charsetStr) - 1) == 0) {
                charset = paramName + sizeof(charsetStr) - 1;
                charsetEnd = start + curParamEnd;
                typeHasCharset = true;
                charsetParamStart = curParamStart - 1;
                charsetParamEnd = curParamEnd;
            }

            curParamStart = curParamEnd + 1;
        } while (curParamStart < flatStr.Length());
    }

    bool charsetNeedsQuotedStringUnescaping = false;
    if (typeHasCharset) {
        // Trim LWS leading and trailing whitespace from charset.  We include
        // '(' in the trailing trim set to catch media-type comments, which are
        // not at all standard, but may occur in rare cases.
        charset = net_FindCharNotInSet(charset, charsetEnd, HTTP_LWS);
        if (*charset == '"') {
            charsetNeedsQuotedStringUnescaping = true;
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
        memchr(type, '/', typeEnd - type) != nullptr) {
        // Common case here is that aContentType is empty
        bool eq = !aContentType.IsEmpty() &&
            aContentType.Equals(Substring(type, typeEnd),
                                nsCaseInsensitiveCStringComparator());
        if (!eq) {
            aContentType.Assign(type, typeEnd - type);
            ToLowerCase(aContentType);
        }

        if ((!eq && *aHadCharset) || typeHasCharset) {
            *aHadCharset = true;
            if (charsetNeedsQuotedStringUnescaping) {
                // parameters using the "quoted-string" syntax need
                // backslash-escapes to be unescaped (see RFC 2616 Section 2.2)
                aContentCharset.Truncate();
                for (const char *c = charset; c != charsetEnd; c++) {
                    if (*c == '\\' && c + 1 != charsetEnd) {
                        // eat escape
                        c++;  
                    }
                    aContentCharset.Append(*c);
                }
            }
            else {
                aContentCharset.Assign(charset, charsetEnd - charset);
            }
            if (typeHasCharset) {
                *aCharsetStart = charsetParamStart + aOffset;
                *aCharsetEnd = charsetParamEnd + aOffset;
            }
        }
        // Only set a new charset position if this is a different type
        // from the last one we had and it doesn't already have a
        // charset param.  If this is the same type, we probably want
        // to leave the charset position on its first occurrence.
        if (!eq && !typeHasCharset) {
            int32_t charsetStart = int32_t(paramStart);
            if (charsetStart == kNotFound)
                charsetStart =  flatStr.Length();

            *aCharsetEnd = *aCharsetStart = charsetStart + aOffset;
        }
    }
}

#undef HTTP_LWS

void
net_ParseContentType(const nsACString &aHeaderStr,
                     nsACString       &aContentType,
                     nsACString       &aContentCharset,
                     bool             *aHadCharset)
{
    int32_t dummy1, dummy2;
    net_ParseContentType(aHeaderStr, aContentType, aContentCharset,
                         aHadCharset, &dummy1, &dummy2);
}

void
net_ParseContentType(const nsACString &aHeaderStr,
                     nsACString       &aContentType,
                     nsACString       &aContentCharset,
                     bool             *aHadCharset,
                     int32_t          *aCharsetStart,
                     int32_t          *aCharsetEnd)
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

    *aHadCharset = false;
    const nsCString& flatStr = PromiseFlatCString(aHeaderStr);
    
    // iterate over media-types.  Note that ',' characters can happen
    // inside quoted strings, so we need to watch out for that.
    uint32_t curTypeStart = 0;
    do {
        // curTypeStart points to the start of the current media-type.  We want
        // to look for its end.
        uint32_t curTypeEnd =
            net_FindMediaDelimiter(flatStr, curTypeStart, ',');
        
        // At this point curTypeEnd points to the spot where the media-type
        // starting at curTypeEnd ends.  Time to parse that!
        net_ParseMediaType(Substring(flatStr, curTypeStart,
                                     curTypeEnd - curTypeStart),
                           aContentType, aContentCharset, curTypeStart,
                           aHadCharset, aCharsetStart, aCharsetEnd);

        // And let's move on to the next media-type
        curTypeStart = curTypeEnd + 1;
    } while (curTypeStart < flatStr.Length());
}

bool
net_IsValidHostName(const nsCSubstring &host)
{
    const char *end = host.EndReading();
    // Use explicit whitelists to select which characters we are
    // willing to send to lower-level DNS logic. This is more
    // self-documenting, and can also be slightly faster than the
    // blacklist approach, since DNS names are the common case, and
    // the commonest characters will tend to be near the start of
    // the list.

    // Whitelist for DNS names (RFC 1035) with extra characters added 
    // for pragmatic reasons "$+_"
    // see https://bugzilla.mozilla.org/show_bug.cgi?id=355181#c2
    if (net_FindCharNotInSet(host.BeginReading(), end,
                             "abcdefghijklmnopqrstuvwxyz"
                             ".-0123456789"
                             "ABCDEFGHIJKLMNOPQRSTUVWXYZ$+_") == end)
        return true;

    // Might be a valid IPv6 link-local address containing a percent sign
    nsAutoCString strhost(host);
    PRNetAddr addr;
    return PR_StringToNetAddr(strhost.get(), &addr) == PR_SUCCESS;
}

bool
net_IsValidIPv4Addr(const char *addr, int32_t addrLen)
{
    RangedPtr<const char> p(addr, addrLen);

    int32_t octet = -1;   // means no digit yet
    int32_t dotCount = 0; // number of dots in the address

    for (; addrLen; ++p, --addrLen) {
        if (*p == '.') {
            dotCount++;
            if (octet == -1) {
                // invalid octet
                return false;
            }
            octet = -1;
        } else if (*p >= '0' && *p <='9') {
            if (octet == 0) {
                // leading 0 is not allowed
                return false;
            } else if (octet == -1) {
                octet = *p - '0';
            } else {
                octet *= 10;
                octet += *p - '0';
                if (octet > 255)
                    return false;
            }
        } else {
            // invalid character
            return false;
        }
    }

    return (dotCount == 3 && octet != -1);
}

bool
net_IsValidIPv6Addr(const char *addr, int32_t addrLen)
{
    RangedPtr<const char> p(addr, addrLen);

    int32_t digits = 0; // number of digits in current block
    int32_t colons = 0; // number of colons in a row during parsing
    int32_t blocks = 0; // number of hexadecimal blocks
    bool haveZeros = false; // true if double colon is present in the address

    for (; addrLen; ++p, --addrLen) {
        if (*p == ':') {
            if (colons == 0) {
                if (digits != 0) {
                    digits = 0;
                    blocks++;
                }
            } else if (colons == 1) {
                if (haveZeros)
                    return false; // only one occurrence is allowed
                haveZeros = true;
            } else {
                // too many colons in a row
                return false;
            }
            colons++;
        } else if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
                   (*p >= 'A' && *p <= 'F')) {
            if (colons == 1 && blocks == 0) // starts with a single colon
                return false;
            if (digits == 4) // too many digits
                return false;
            colons = 0;
            digits++;
        } else if (*p == '.') {
            // check valid IPv4 from the beginning of the last block
            if (!net_IsValidIPv4Addr(p.get() - digits, addrLen + digits))
                return false;
            return (haveZeros && blocks < 6) || (!haveZeros && blocks == 6);
        } else {
            // invalid character
            return false;
        }
    }

    if (colons == 1) // ends with a single colon
        return false;

    if (digits) // there is a block at the end
        blocks++;

    return (haveZeros && blocks < 8) || (!haveZeros && blocks == 8);
}
