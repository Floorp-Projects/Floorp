/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#ifndef __nsmimeinfoimpl_h___
#define __nsmimeinfoimpl_h___

#include "nsIMIMEInfo.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIFile.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"

/** 
 * UTF8 moz-icon URI string for the default handler application's icon, if 
 * available.
 */
#define PROPERTY_DEFAULT_APP_ICON_URL "defaultApplicationIconURL"
/** 
 * UTF8 moz-icon URI string for the user's preferred handler application's 
 * icon, if available.
 */
#define PROPERTY_CUSTOM_APP_ICON_URL "customApplicationIconURL"

/**
 * Basic implementation of nsIMIMEInfo. Incomplete - it is meant to be
 * subclassed, and GetHasDefaultHandler as well as LaunchDefaultWithFile need to
 * be implemented.
 */
class nsMIMEInfoBase : public nsIMIMEInfo {
  public:
    NS_DECL_ISUPPORTS

    // I'd use NS_DECL_NSIMIMEINFO, but I don't want GetHasDefaultHandler
    NS_IMETHOD GetFileExtensions(nsIUTF8StringEnumerator **_retval);
    NS_IMETHOD SetFileExtensions(const nsACString & aExtensions);
    NS_IMETHOD ExtensionExists(const nsACString & aExtension, PRBool *_retval);
    NS_IMETHOD AppendExtension(const nsACString & aExtension);
    NS_IMETHOD GetPrimaryExtension(nsACString & aPrimaryExtension);
    NS_IMETHOD SetPrimaryExtension(const nsACString & aPrimaryExtension);
    NS_IMETHOD GetMIMEType(nsACString & aMIMEType);
    NS_IMETHOD GetDescription(nsAString & aDescription);
    NS_IMETHOD SetDescription(const nsAString & aDescription);
    NS_IMETHOD GetMacType(PRUint32 *aMacType);
    NS_IMETHOD SetMacType(PRUint32 aMacType);
    NS_IMETHOD GetMacCreator(PRUint32 *aMacCreator);
    NS_IMETHOD SetMacCreator(PRUint32 aMacCreator);
    NS_IMETHOD Equals(nsIMIMEInfo *aMIMEInfo, PRBool *_retval);
    NS_IMETHOD GetPreferredApplicationHandler(nsIFile * *aPreferredApplicationHandler);
    NS_IMETHOD SetPreferredApplicationHandler(nsIFile * aPreferredApplicationHandler);
    NS_IMETHOD GetApplicationDescription(nsAString & aApplicationDescription);
    NS_IMETHOD SetApplicationDescription(const nsAString & aApplicationDescription);
    NS_IMETHOD GetDefaultDescription(nsAString & aDefaultDescription);
    NS_IMETHOD LaunchWithFile(nsIFile *aFile);
    NS_IMETHOD GetPreferredAction(nsMIMEInfoHandleAction *aPreferredAction);
    NS_IMETHOD SetPreferredAction(nsMIMEInfoHandleAction aPreferredAction);
    NS_IMETHOD GetAlwaysAskBeforeHandling(PRBool *aAlwaysAskBeforeHandling);
    NS_IMETHOD SetAlwaysAskBeforeHandling(PRBool aAlwaysAskBeforeHandling); 

    // nsMIMEInfoBase methods
    nsMIMEInfoBase(const char *aMIMEType = "") NS_HIDDEN;
    nsMIMEInfoBase(const nsACString& aMIMEType) NS_HIDDEN;
    virtual ~nsMIMEInfoBase();        // must be virtual, as the the base class's Release should call the subclass's destructor

    void SetMIMEType(const nsACString & aMIMEType) { mMIMEType = aMIMEType; }

    void SetDefaultDescription(const nsString& aDesc) { mDefaultAppDescription = aDesc; }

    /**
     * Copies basic data of this MIME Info Implementation to the given other
     * MIME Info. The data consists of the MIME Type, the (default) description,
     * the MacOS type and creator, and the extension list (this object's
     * extension list will replace aOther's list, not append to it). This
     * function also ensures that aOther's primary extension will be the same as
     * the one of this object.
     */
    void CopyBasicDataTo(nsMIMEInfoBase* aOther);

    /**
     * Return whether this MIMEInfo has any extensions
     */
    PRBool HasExtensions() const { return mExtensions.Count() != 0; }

  protected:
    /**
     * Launch the default application for the given file.
     * For even more control over the launching, override launchWithFile.
     * Also see the comment about nsIMIMEInfo in general, above.
     *
     * @param aFile The file that should be opened
     */
    virtual NS_HIDDEN_(nsresult) LaunchDefaultWithFile(nsIFile* aFile) = 0;

    /**
     * This method can be used to launch the file using nsIProcess, with the
     * path of the file being the first parameter to the executable. This is
     * meant as a helper method for implementations of
     * LaunchWithFile/LaunchDefaultWithFile.
     * Neither aApp nor aFile may be null.
     *
     * @param aApp The application to launch
     * @param aFile The file to open in the application
     */
    static NS_HIDDEN_(nsresult) LaunchWithIProcess(nsIFile* aApp, nsIFile* aFile);

    // member variables
    nsCStringArray         mExtensions; ///< array of file extensions associated w/ this MIME obj
    nsString               mDescription; ///< human readable description
    PRUint32               mMacType, mMacCreator; ///< Mac file type and creator
    nsCString              mMIMEType;
    nsCOMPtr<nsIFile>      mPreferredApplication; ///< preferred application associated with this type.
    nsMIMEInfoHandleAction mPreferredAction; ///< preferred action to associate with this type
    nsString               mPreferredAppDescription;
    nsString               mDefaultAppDescription;
    PRBool                 mAlwaysAskBeforeHandling;
};


/**
 * This is a complete implementation of nsIMIMEInfo, and contains all necessary
 * methods. However, depending on your platform you may want to use a different
 * way of launching applications. This class stores the default application in a
 * member variable and provides a function for setting it. Launching is done
 * using nsIProcess, native path of the file to open as first argument.
 */
class nsMIMEInfoImpl : public nsMIMEInfoBase {
  public:
    nsMIMEInfoImpl(const char *aMIMEType = "") : nsMIMEInfoBase(aMIMEType) {}
    nsMIMEInfoImpl(const nsACString& aMIMEType) : nsMIMEInfoBase(aMIMEType) {}
    virtual ~nsMIMEInfoImpl() {}

    // nsIMIMEInfo methods
    NS_IMETHOD GetHasDefaultHandler(PRBool *_retval);
    NS_IMETHOD GetDefaultDescription(nsAString& aDefaultDescription);

    // additional methods
    /**
     * Sets the default application. Supposed to be only called by the OS Helper
     * App Services; the default application is immutable after it is first set.
     */
    void SetDefaultApplication(nsIFile* aApp) { if (!mDefaultApplication) mDefaultApplication = aApp; }
  protected:
    // nsMIMEInfoBase methods
    /**
     * The base class implementation is to use LaunchWithIProcess in combination
     * with mDefaultApplication. Subclasses can override that behaviour.
     */
    virtual NS_HIDDEN_(nsresult) LaunchDefaultWithFile(nsIFile* aFile);


    nsCOMPtr<nsIFile>      mDefaultApplication; ///< default application associated with this type.
};

#endif //__nsmimeinfoimpl_h___
