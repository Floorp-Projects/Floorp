/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 et cin: */
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
 * The Original Code is the OS/2 MIME Info Implementation.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rich Walsh <dragtext@e-vertise.com>
 *   Peter Weilbacher <mozilla@Weilbacher.org>
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

#ifndef nsMIMEInfoOS2_h_
#define nsMIMEInfoOS2_h_

#include "nsMIMEInfoImpl.h"
#include "nsIPropertyBag.h"

#include "nsNetCID.h"
#include "nsEscape.h"

#define INCL_DOS
#define INCL_DOSMISC
#define INCL_DOSERRORS
#define INCL_WINSHELLDATA
#include <os2.h>

class nsMIMEInfoOS2 : public nsMIMEInfoBase, public nsIPropertyBag
{
  public:
    nsMIMEInfoOS2(const char *aType = "") :
      nsMIMEInfoBase(aType), mDefaultAppHandle(0) {}
    nsMIMEInfoOS2(const nsACString& aMIMEType) :
      nsMIMEInfoBase(aMIMEType), mDefaultAppHandle(0) {}
    nsMIMEInfoOS2(const nsACString& aType, HandlerClass aClass) :
      nsMIMEInfoBase(aType, aClass), mDefaultAppHandle(0) {}
    virtual ~nsMIMEInfoOS2();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIPROPERTYBAG

    NS_IMETHOD LaunchWithFile(nsIFile *aFile);

    NS_IMETHOD GetHasDefaultHandler(bool *_retval);
    NS_IMETHOD GetDefaultDescription(nsAString& aDefaultDescription);

    void GetDefaultApplication(nsIFile **aDefaultAppHandler);
    void SetDefaultApplication(nsIFile *aDefaultApplication);

    void GetDefaultAppHandle(PRUint32 *aHandle);
    void SetDefaultAppHandle(PRUint32 aHandle);

  protected:
    virtual NS_HIDDEN_(nsresult) LoadUriInternal(nsIURI *aURI);
    // XXX should we do most of the work here and let LaunchWithFile() call this?
    virtual NS_HIDDEN_(nsresult) LaunchDefaultWithFile(nsIFile *aFile) {
      NS_NOTREACHED("Do not call this, use LaunchWithFile");
      return NS_ERROR_UNEXPECTED;
    }

    NS_IMETHOD GetIconURLVariant(nsIFile *aApplication, nsIVariant **_retval);

    nsCOMPtr<nsIFile> mDefaultApplication;
    PRUint32 mDefaultAppHandle;
};

#endif
