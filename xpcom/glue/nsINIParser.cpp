/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Moz headers (alphabetical)
#include "nsCRTGlue.h"
#include "nsError.h"
#include "nsIFile.h"
#include "nsINIParser.h"
#include "mozilla/FileUtils.h" // AutoFILE

// System headers (alphabetical)
#include <stdio.h>
#include <stdlib.h>
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
  AutoFILE(FILE *fp = nullptr) : fp_(fp) {}
  ~AutoFILE() { if (fp_) fclose(fp_); }
  operator FILE *() { return fp_; }
  FILE** operator &() { return &fp_; }
  void operator=(FILE *fp) { fp_ = fp; }
private:
  FILE *fp_;
};

nsresult
nsINIParser::Init(nsIFile* aFile)
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
    mSections.Init();

    /* get file size */
    if (fseek(fd, 0, SEEK_END) != 0)
        return NS_ERROR_FAILURE;

    long flen = ftell(fd);
    if (flen == 0)
        return NS_ERROR_FAILURE;

    /* malloc an internal buf the size of the file */
    mFileContents = new char[flen + 2];
    if (!mFileContents)
        return NS_ERROR_OUT_OF_MEMORY;

    /* read the file in one swoop */
    if (fseek(fd, 0, SEEK_SET) != 0)
        return NS_BASE_STREAM_OSERROR;

    int rd = fread(mFileContents, sizeof(char), flen, fd);
    if (rd != flen)
        return NS_BASE_STREAM_OSERROR;

    // We write a UTF16 null so that the file is easier to convert to UTF8
    mFileContents[flen] = mFileContents[flen + 1] = '\0';

    char *buffer = &mFileContents[0];

    if (flen >= 3
    && mFileContents[0] == static_cast<char>(0xEF)
    && mFileContents[1] == static_cast<char>(0xBB)
    && mFileContents[2] == static_cast<char>(0xBF)) {
      // Someone set us up the Utf-8 BOM
      // This case is easy, since we assume that BOM-less
      // files are Utf-8 anyway.  Just skip the BOM and process as usual.
      buffer = &mFileContents[3];
    }

#ifdef XP_WIN
    if (flen >= 2
     && mFileContents[0] == static_cast<char>(0xFF)
     && mFileContents[1] == static_cast<char>(0xFE)) {
        // Someone set us up the Utf-16LE BOM
        buffer = &mFileContents[2];
        // Get the size required for our Utf8 buffer
        flen = WideCharToMultiByte(CP_UTF8,
                                   0,
                                   reinterpret_cast<LPWSTR>(buffer),
                                   -1,
                                   NULL,
                                   0,
                                   NULL,
                                   NULL);
        if (0 == flen) {
            return NS_ERROR_FAILURE;
        }

        nsAutoArrayPtr<char> utf8Buffer(new char[flen]);
        if (0 == WideCharToMultiByte(CP_UTF8,
                                     0,
                                     reinterpret_cast<LPWSTR>(buffer),
                                     -1,
                                     utf8Buffer,
                                     flen,
                                     NULL,
                                     NULL)) {
            return NS_ERROR_FAILURE;
        }
        mFileContents = utf8Buffer.forget();
        buffer = mFileContents;
    }
#endif

    char *currSection = nullptr;

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
                currSection = nullptr;
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
