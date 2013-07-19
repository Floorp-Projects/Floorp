/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __nsmimeinfoimpl_h___
#define __nsmimeinfoimpl_h___

#include "nsIMIMEInfo.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIMutableArray.h"
#include "nsIFile.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsIProcess.h"

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
    NS_DECL_THREADSAFE_ISUPPORTS

    // I'd use NS_DECL_NSIMIMEINFO, but I don't want GetHasDefaultHandler
    NS_IMETHOD GetFileExtensions(nsIUTF8StringEnumerator **_retval);
    NS_IMETHOD SetFileExtensions(const nsACString & aExtensions);
    NS_IMETHOD ExtensionExists(const nsACString & aExtension, bool *_retval);
    NS_IMETHOD AppendExtension(const nsACString & aExtension);
    NS_IMETHOD GetPrimaryExtension(nsACString & aPrimaryExtension);
    NS_IMETHOD SetPrimaryExtension(const nsACString & aPrimaryExtension);
    NS_IMETHOD GetType(nsACString & aType);
    NS_IMETHOD GetMIMEType(nsACString & aMIMEType);
    NS_IMETHOD GetDescription(nsAString & aDescription);
    NS_IMETHOD SetDescription(const nsAString & aDescription);
    NS_IMETHOD Equals(nsIMIMEInfo *aMIMEInfo, bool *_retval);
    NS_IMETHOD GetPreferredApplicationHandler(nsIHandlerApp * *aPreferredAppHandler);
    NS_IMETHOD SetPreferredApplicationHandler(nsIHandlerApp * aPreferredAppHandler);
    NS_IMETHOD GetPossibleApplicationHandlers(nsIMutableArray * *aPossibleAppHandlers);
    NS_IMETHOD GetDefaultDescription(nsAString & aDefaultDescription);
    NS_IMETHOD LaunchWithFile(nsIFile *aFile);
    NS_IMETHOD LaunchWithURI(nsIURI *aURI,
                             nsIInterfaceRequestor *aWindowContext);
    NS_IMETHOD GetPreferredAction(nsHandlerInfoAction *aPreferredAction);
    NS_IMETHOD SetPreferredAction(nsHandlerInfoAction aPreferredAction);
    NS_IMETHOD GetAlwaysAskBeforeHandling(bool *aAlwaysAskBeforeHandling);
    NS_IMETHOD SetAlwaysAskBeforeHandling(bool aAlwaysAskBeforeHandling); 
    NS_IMETHOD GetPossibleLocalHandlers(nsIArray **_retval); 

    enum HandlerClass {
      eMIMEInfo,
      eProtocolInfo
    };

    // nsMIMEInfoBase methods
    nsMIMEInfoBase(const char *aMIMEType = "") NS_HIDDEN;
    nsMIMEInfoBase(const nsACString& aMIMEType) NS_HIDDEN;
    nsMIMEInfoBase(const nsACString& aType, HandlerClass aClass) NS_HIDDEN;
    virtual ~nsMIMEInfoBase();        // must be virtual, as the the base class's Release should call the subclass's destructor

    void SetMIMEType(const nsACString & aMIMEType) { mSchemeOrType = aMIMEType; }

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
    bool HasExtensions() const { return mExtensions.Length() != 0; }

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
     * Loads the URI with the OS default app.
     *
     * @param aURI The URI to pass off to the OS.
     */
    virtual NS_HIDDEN_(nsresult) LoadUriInternal(nsIURI *aURI) = 0;

    static already_AddRefed<nsIProcess> InitProcess(nsIFile* aApp,
                                                    nsresult* aResult);

    /**
     * This method can be used to launch the file or URI with a single 
     * argument (typically either a file path or a URI spec).  This is 
     * meant as a helper method for implementations of
     * LaunchWithURI/LaunchDefaultWithFile.
     *
     * @param aApp The application to launch (may not be null)
     * @param aArg The argument to pass on the command line
     */
    static NS_HIDDEN_(nsresult) LaunchWithIProcess(nsIFile* aApp,
                                                   const nsCString &aArg);
    static NS_HIDDEN_(nsresult) LaunchWithIProcess(nsIFile* aApp,
                                                   const nsString &aArg);

    /**
     * Given a file: nsIURI, return the associated nsIFile
     *
     * @param  aURI      the file: URI in question
     * @param  aFile     the associated nsIFile (out param)
     */
    static NS_HIDDEN_(nsresult) GetLocalFileFromURI(nsIURI *aURI,
                                                    nsIFile **aFile);

    // member variables
    nsTArray<nsCString>    mExtensions; ///< array of file extensions associated w/ this MIME obj
    nsString               mDescription; ///< human readable description
    nsCString              mSchemeOrType;
    HandlerClass           mClass;
    nsCOMPtr<nsIHandlerApp> mPreferredApplication;
    nsCOMPtr<nsIMutableArray> mPossibleApplications;
    nsHandlerInfoAction    mPreferredAction; ///< preferred action to associate with this type
    nsString               mPreferredAppDescription;
    nsString               mDefaultAppDescription;
    bool                   mAlwaysAskBeforeHandling;
};


/**
 * This is a complete implementation of nsIMIMEInfo, and contains all necessary
 * methods. However, depending on your platform you may want to use a different
 * way of launching applications. This class stores the default application in a
 * member variable and provides a function for setting it. For local
 * applications, launching is done using nsIProcess, native path of the file to
 * open as first argument.
 */
class nsMIMEInfoImpl : public nsMIMEInfoBase {
  public:
    nsMIMEInfoImpl(const char *aMIMEType = "") : nsMIMEInfoBase(aMIMEType) {}
    nsMIMEInfoImpl(const nsACString& aMIMEType) : nsMIMEInfoBase(aMIMEType) {}
    nsMIMEInfoImpl(const nsACString& aType, HandlerClass aClass) :
      nsMIMEInfoBase(aType, aClass) {}
    virtual ~nsMIMEInfoImpl() {}

    // nsIMIMEInfo methods
    NS_IMETHOD GetHasDefaultHandler(bool *_retval);
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

    /**
     * Loads the URI with the OS default app.  This should be overridden by each
     * OS's implementation.
     */
    virtual NS_HIDDEN_(nsresult) LoadUriInternal(nsIURI *aURI) = 0;

    nsCOMPtr<nsIFile>      mDefaultApplication; ///< default application associated with this type.
};

#endif //__nsmimeinfoimpl_h___
