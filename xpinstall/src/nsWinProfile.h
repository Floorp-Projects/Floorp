/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsWinProfile_h__
#define nsWinProfile_h__

#include "prtypes.h"

#include "nsInstall.h"

class nsWinProfile
{
  public:

    /* Public Fields */

    /* Public Methods */

    nsWinProfile( nsInstall* suObj, const nsString& folder, const nsString& file );
    ~nsWinProfile(); 

    /**
     * Schedules a write into a windows "ini" file.  "Value" can be
     * null to delete the value, but we don't support deleting an entire
     * section via a null "key".  The actual write takes place during
     * SoftwareUpdate.FinalizeInstall();
     *
     * @return  false for failure, true for success
     */
    PRInt32 WriteString( nsString section, nsString key, nsString value, PRInt32* aReturn );
    
    /**
     * Reads a value from a windows "ini" file.  We don't support using
     * a null "key" to return a list of keys--you have to know what you want
     *
     * @return  String value from INI, "" if not found, null if error
     */
    PRInt32 GetString( nsString section, nsString key, nsString* aReturn );
    
    nsString* GetFilename();
    nsInstall* InstallObject();
    
    PRInt32 FinalWriteString( nsString section, nsString key, nsString value );

    
  private:
    
    /* Private Fields */
    nsString*  mFilename;
    nsInstall* mInstallObject;
    
    /* Private Methods */
    PRInt32 NativeWriteString( nsString section, nsString key, nsString value );
    PRInt32 NativeGetString( nsString section, nsString key, nsString* aReturn );
};

#endif /* nsWinProfile_h__ */
