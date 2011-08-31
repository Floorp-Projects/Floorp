/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *   Benjamin Smedberg <bsmedberg@covad.net>
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

#include "nsINIParser.h"
#include "nsError.h"
#include "nsILocalFile.h"
#include "nsCRTGlue.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef XP_WIN
#include <windows.h>
#endif

#if defined(XP_WIN)
#define READ_BINARYMODE L"rb"
#elif defined(XP_OS2)
#define READ_BINARYMODE "rb"
#else
#define READ_BINARYMODE "r"
#endif

#ifdef XP_WIN
inline FILE *TS_tfopen (const char *path, const wchar_t *mode)
{
    wchar_t wPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH);
    return _wfopen(wPath, mode);
}
#else
inline FILE *TS_tfopen (const char *path, const char *mode)
{
    return fopen(path, mode);
}
#endif

// Stack based FILE wrapper to ensure that fclose is called, copied from
// toolkit/mozapps/update/updater/readstrings.cpp

class AutoFILE {
public:
  AutoFILE(FILE *fp = nsnull) : fp_(fp) {}
  ~AutoFILE() { if (fp_) fclose(fp_); }
  operator FILE *() { return fp_; }
  FILE** operator &() { return &fp_; }
  void operator=(FILE *fp) { fp_ = fp; }
private:
  FILE *fp_;
};

nsresult
nsINIParser::Init(nsILocalFile* aFile)
{
    /* open the file. Don't use OpenANSIFileDesc, because you mustn't
       pass FILE* across shared library boundaries, which may be using
       different CRTs */

    AutoFILE fd;

#ifdef XP_WIN
    nsAutoString path;
    nsresult rv = aFile->GetPath(path);
    NS_ENSURE_SUCCESS(rv, rv);

    fd = _wfopen(path.get(), READ_BINARYMODE);
#else
    nsCAutoString path;
    aFile->GetNativePath(path);

    fd = fopen(path.get(), READ_BINARYMODE);
#endif

    if (!fd)
      return NS_ERROR_FAILURE;

    return InitFromFILE(fd);
}

nsresult
nsINIParser::Init(const char *aPath)
{
    /* open the file */
    AutoFILE fd = TS_tfopen(aPath, READ_BINARYMODE);

    if (!fd)
        return NS_ERROR_FAILURE;

    return InitFromFILE(fd);
}

static const char kNL[] = "\r\n";
static const char kEquals[] = "=";
static const char kWhitespace[] = " \t";
static const char kRBracket[] = "]";

nsresult
nsINIParser::InitFromFILE(FILE *fd)
{
    if (!mSections.Init())
        return NS_ERROR_OUT_OF_MEMORY;

    /* get file size */
    if (fseek(fd, 0, SEEK_END) != 0)
        return NS_ERROR_FAILURE;

    long flen = ftell(fd);
    if (flen == 0)
        return NS_ERROR_FAILURE;

    /* malloc an internal buf the size of the file */
    mFileContents = new char[flen + 1];
    if (!mFileContents)
        return NS_ERROR_OUT_OF_MEMORY;

    /* read the file in one swoop */
    if (fseek(fd, 0, SEEK_SET) != 0)
        return NS_BASE_STREAM_OSERROR;

    int rd = fread(mFileContents, sizeof(char), flen, fd);
    if (rd != flen)
        return NS_BASE_STREAM_OSERROR;

    mFileContents[flen] = '\0';

    char *buffer = mFileContents;
    char *currSection = nsnull;

    // outer loop tokenizes into lines
    while (char *token = NS_strtok(kNL, &buffer)) {
        if (token[0] == '#' || token[0] == ';') // it's a comment
            continue;

        token = (char*) NS_strspnp(kWhitespace, token);
        if (!*token) // empty line
            continue;

        if (token[0] == '[') { // section header!
            ++token;
            currSection = token;

            char *rb = NS_strtok(kRBracket, &token);
            if (!rb || NS_strtok(kWhitespace, &token)) {
                // there's either an unclosed [Section or a [Section]Moretext!
                // we could frankly decide that this INI file is malformed right
                // here and stop, but we won't... keep going, looking for
                // a well-formed [section] to continue working with
                currSection = nsnull;
            }

            continue;
        }

        if (!currSection) {
            // If we haven't found a section header (or we found a malformed
            // section header), don't bother parsing this line.
            continue;
        }

        char *key = token;
        char *e = NS_strtok(kEquals, &token);
        if (!e || !token)
            continue;

        INIValue *v;
        if (!mSections.Get(currSection, &v)) {
            v = new INIValue(key, token);
            if (!v)
                return NS_ERROR_OUT_OF_MEMORY;

            mSections.Put(currSection, v);
            continue;
        }

        // Check whether this key has already been specified; overwrite
        // if so, or append if not.
        while (v) {
            if (!strcmp(key, v->key)) {
                v->value = token;
                break;
            }
            if (!v->next) {
                v->next = new INIValue(key, token);
                if (!v->next)
                    return NS_ERROR_OUT_OF_MEMORY;
                break;
            }
            v = v->next;
        }
        NS_ASSERTION(v, "v should never be null coming out of this loop");
    }

    return NS_OK;
}

nsresult
nsINIParser::GetString(const char *aSection, const char *aKey, 
                       nsACString &aResult)
{
    INIValue *val;
    mSections.Get(aSection, &val);

    while (val) {
        if (strcmp(val->key, aKey) == 0) {
            aResult.Assign(val->value);
            return NS_OK;
        }

        val = val->next;
    }

    return NS_ERROR_FAILURE;
}

nsresult
nsINIParser::GetString(const char *aSection, const char *aKey, 
                       char *aResult, PRUint32 aResultLen)
{
    INIValue *val;
    mSections.Get(aSection, &val);

    while (val) {
        if (strcmp(val->key, aKey) == 0) {
            strncpy(aResult, val->value, aResultLen);
            aResult[aResultLen - 1] = '\0';
            if (strlen(val->value) >= aResultLen)
                return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;

            return NS_OK;
        }

        val = val->next;
    }

    return NS_ERROR_FAILURE;
}

PLDHashOperator
nsINIParser::GetSectionsCB(const char *aKey, INIValue *aData,
                           void *aClosure)
{
    GSClosureStruct *cs = reinterpret_cast<GSClosureStruct*>(aClosure);

    return cs->usercb(aKey, cs->userclosure) ? PL_DHASH_NEXT : PL_DHASH_STOP;
}

nsresult
nsINIParser::GetSections(INISectionCallback aCB, void *aClosure)
{
    GSClosureStruct gs = {
        aCB,
        aClosure
    };

    mSections.EnumerateRead(GetSectionsCB, &gs);
    return NS_OK;
}

nsresult
nsINIParser::GetStrings(const char *aSection,
                        INIStringCallback aCB, void *aClosure)
{
    INIValue *val;

    for (mSections.Get(aSection, &val);
         val;
         val = val->next) {

        if (!aCB(val->key, val->value, aClosure))
            return NS_OK;
    }

    return NS_OK;
}
