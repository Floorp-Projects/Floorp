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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsISupportsArray.h"
#include "nsICipherInfo.h"
#include "nsIPref.h"
#include "nsString.h"
#include "sslt.h"

class nsCipherInfo;

class nsCiphers
{
public:
  nsCiphers();
  ~nsCiphers();

  static void InitSingleton();
  static void DestroySingleton();
  
  static void SetAllCiphersFromPrefs(nsIPref *ipref);
  static void SetCipherFromPref(nsIPref *ipref, const char *prefname);

  static PRBool IsImplementedCipherWanted(PRUint16 implemented_cipher_index);

private:
  static nsCiphers *singleton;

  struct CipherData {
    CipherData() 
    :id(0), isWanted(PR_FALSE), isGood(PR_FALSE), heapString(nsnull), dataSegmentString(nsnull) {}
    
    ~CipherData() {
      if (heapString) nsMemory::Free(heapString);
    }
  
    PRUint16 id;
    void setDataSegmentPrefString(const char *dss) {
      dataSegmentString = dss;
    }
    void setHeapString(char *hs) {
      if (heapString) nsMemory::Free(heapString);
      heapString = hs;
    }
    const char *GetPrefString() {
      return heapString ? heapString : dataSegmentString;
    }
    PRPackedBool isWanted;
    PRPackedBool isGood;
    SSLCipherSuiteInfo info;
  private:
    char *heapString;
    const char *dataSegmentString;
  };

  struct CipherData *mCiphers;

  friend class nsCipherInfo;
};

class nsCipherInfoService : public nsICipherInfoService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICIPHERINFOSERVICE

  nsCipherInfoService();
  virtual ~nsCipherInfoService();

private:
  nsCOMPtr<nsISupportsArray> mArray;
};

class nsCipherInfo : public nsICipherInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICIPHERINFO

  nsCipherInfo();
  virtual ~nsCipherInfo();

  void setCipherByImplementedCipherIndex(PRUint16 i);

private:
  PRBool mIsInitialized;
  PRUint16 mCipherIndex;
};
