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

// This file was shamelessly copied from mozilla/xpinstall/wizard/unix/src2

#ifndef nsINIParser_h__
#define nsINIParser_h__

#include "nscore.h"

// hack alert: in static builds, nsINIParser here conflicts with nsINIParser
// in browser/components/migration. So we use a #define to make the symbols
// unique.
#define nsINIParser nsINITParser

class nsILocalFile;

class nsINIParser
{
public:
    nsINIParser();
    ~nsINIParser();

#if 0 // use nsresult instead
    /**
     * Errors
     */
    enum INIResult
    {
        OK                  = 0,
        E_READ              = -701,
        E_MEM               = -702,
        E_PARAM             = -703,
        E_NO_SEC            = -704,
        E_NO_KEY            = -705,
        E_SEC_CORRUPT       = -706,
        E_SMALL_BUF         = -707
    };
#endif

    /**
     * Initialize the INIParser with a nsILocalFile. If this method fails, no
     * other methods should be called.
     */
    nsresult Init(nsILocalFile* aFile);

    /**
     * GetString
     * 
     * Gets the value of the specified key in the specified section
     * of the INI file represented by this instance. The value is stored
     * in the supplied buffer. The buffer size is provided as input and
     * the actual bytes used by the value is set in the in/out size param.
     *
     * @param aSection      section name
     * @param aKey          key name
     * @param aValBuf       user supplied buffer
     * @param aIOValBufSize buf size on input; actual buf used on output
     *
     * @return mError       operation success code
     */
    nsresult GetString(const char *aSection, const char *aKey, 
                       char *aValBuf, PRUint32 aIOValBufSize );

private:
    nsresult FindSection(const char *aSection, char **aOutSecPtr);
    nsresult FindKey(const char *aSecPtr, const char *aKey,
                     char *aVal, int aIOValSize);

    char    *mFileBuf;
    int     mFileBufSize;
};

#endif /* nsINIParser_h__ */
