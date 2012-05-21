/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
