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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
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
/* $Id: nsPKCS12Blob.h,v 1.16 2006/04/12 15:43:32 benjamin%smedbergs.us Exp $ */

#ifndef _NS_PKCS12BLOB_H_
#define _NS_PKCS12BLOB_H_

#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsIPK11TokenDB.h"
#include "nsNSSHelper.h"
#include "nsIPK11Token.h"
#include "nsIMutableArray.h"

#include "nss.h"

extern "C" {
#include "pkcs12.h"
#include "p12plcy.h"
}

class nsIX509Cert;

//
// nsPKCS12Blob
//
// Class for importing/exporting PKCS#12 blobs
//
class nsPKCS12Blob
{
public:
  nsPKCS12Blob();
  virtual ~nsPKCS12Blob();

  // Set the token to use (default is internal)
  nsresult SetToken(nsIPK11Token *token);

  // PKCS#12 Import
  nsresult ImportFromFile(nsILocalFile *file);

  // PKCS#12 Export
#if 0
  //nsresult LoadCerts(const PRUnichar **certNames, int numCerts);
  nsresult LoadCerts(nsIX509Cert **certs, int numCerts);
#endif
  nsresult ExportToFile(nsILocalFile *file, nsIX509Cert **certs, int numCerts);

private:

  nsCOMPtr<nsIPK11Token>          mToken;
  nsCOMPtr<nsIMutableArray>       mCertArray;
  nsCOMPtr<nsIInterfaceRequestor> mUIContext;

  // local helper functions
  nsresult getPKCS12FilePassword(SECItem *);
  nsresult newPKCS12FilePassword(SECItem *);
  nsresult inputToDecoder(SEC_PKCS12DecoderContext *, nsILocalFile *);
  void unicodeToItem(const PRUnichar *, SECItem *);
  void handleError(int myerr = 0);

  // RetryReason and ImportMode are used when importing a PKCS12 file.
  // There are two reasons that cause us to retry:
  // - When the password entered by the user is incorrect.
  //   The user will be prompted to try again.
  // - When the user entered a zero length password.
  //   An empty password should be represented as an empty
  //   string (a SECItem that contains a single terminating
  //   NULL UTF16 character), but some applications use a
  //   zero length SECItem.
  //   We try both variations, zero length item and empty string,
  //   without giving a user prompt when trying the different empty password flavors.
  
  enum RetryReason { rr_do_not_retry, rr_bad_password, rr_auto_retry_empty_password_flavors };
  enum ImportMode { im_standard_prompt, im_try_zero_length_secitem };
  
  nsresult ImportFromFileHelper(nsILocalFile *file, ImportMode aImportMode, RetryReason &aWantRetry);

  // NSPR file I/O for export file
  PRFileDesc *mTmpFile;
  char       *mTmpFilePath;

  // simulated file I/O for "in memory" temporary digest data
  nsCString                 *mDigest;
  nsCString::const_iterator *mDigestIterator;

  bool        mTokenSet;

  // C-style callback functions for the NSS PKCS#12 library
  static SECStatus PR_CALLBACK digest_open(void *, PRBool);
  static SECStatus PR_CALLBACK digest_close(void *, PRBool);
  static int       PR_CALLBACK digest_read(void *, unsigned char *, unsigned long);
  static int       PR_CALLBACK digest_write(void *, unsigned char *, unsigned long);
  static SECItem * PR_CALLBACK nickname_collision(SECItem *, PRBool *, void *);
  static void PR_CALLBACK write_export_file(void *arg, const char *buf, unsigned long len);

};

#endif /* _NS_PKCS12BLOB_H_ */

