/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file was shamelessly copied from mozilla/xpinstall/wizard/unix/src2

#ifndef nsINIParser_h__
#define nsINIParser_h__

#ifdef MOZILLA_INTERNAL_API
#define nsINIParser nsINIParser_internal
#endif

#include "nscore.h"
#include "nsClassHashtable.h"
#include "nsAutoPtr.h"

#include <stdio.h>

class nsILocalFile;

class NS_COM_GLUE nsINIParser
{
public:
    nsINIParser() { }
    ~nsINIParser() { }

    /**
     * Initialize the INIParser with a nsILocalFile. If this method fails, no
     * other methods should be called. This method reads and parses the file,
     * the class does not hold a file handle open. An instance must only be
     * initialized once.
     */
    nsresult Init(nsILocalFile* aFile);

    /**
     * Initialize the INIParser with a file path. If this method fails, no
     * other methods should be called. This method reads and parses the file,
     * the class does not hold a file handle open. An instance must only
     * be initialized once.
     */
    nsresult Init(const char *aPath);

    /**
     * Callback for GetSections
     * @return false to stop enumeration, or true to continue.
     */
    typedef bool
    (* INISectionCallback)(const char *aSection, void *aClosure);

    /**
     * Enumerate the sections within the INI file.
     */
    nsresult GetSections(INISectionCallback aCB, void *aClosure);

    /**
     * Callback for GetStrings
     * @return false to stop enumeration, or true to continue
     */
    typedef bool
    (* INIStringCallback)(const char *aString, const char *aValue,
                          void *aClosure);

    /**
     * Enumerate the strings within a section. If the section does
     * not exist, this function will silently return.
     */
    nsresult GetStrings(const char *aSection,
                        INIStringCallback aCB, void *aClosure);

    /**
     * Get the value of the specified key in the specified section
     * of the INI file represented by this instance.
     *
     * @param aSection      section name
     * @param aKey          key name
     * @param aResult       the value found
     * @throws NS_ERROR_FAILURE if the specified section/key could not be
     *                          found.
     */
    nsresult GetString(const char *aSection, const char *aKey, 
                       nsACString &aResult);

    /**
     * Alternate signature of GetString that uses a pre-allocated buffer
     * instead of a nsACString (for use in the standalone glue before
     * the glue is initialized).
     *
     * @throws NS_ERROR_LOSS_OF_SIGNIFICANT_DATA if the aResult buffer is not
     *         large enough for the data. aResult will be filled with as
     *         much data as possible.
     *         
     * @see GetString [1]
     */
    nsresult GetString(const char *aSection, const char* aKey,
                       char *aResult, PRUint32 aResultLen);

private:
    struct INIValue
    {
        INIValue(const char *aKey, const char *aValue)
            : key(aKey), value(aValue) { }

        const char *key;
        const char *value;
        nsAutoPtr<INIValue> next;
    };

    struct GSClosureStruct
    {
        INISectionCallback  usercb;
        void               *userclosure;
    };

    nsClassHashtable<nsDepCharHashKey, INIValue> mSections;
    nsAutoArrayPtr<char> mFileContents;    

    nsresult InitFromFILE(FILE *fd);

    static PLDHashOperator GetSectionsCB(const char *aKey,
                                         INIValue *aData, void *aClosure);
};

#endif /* nsINIParser_h__ */
