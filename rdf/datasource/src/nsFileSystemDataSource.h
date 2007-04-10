/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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

#ifndef nsFileSystemDataSource_h__
#define nsFileSystemDataSource_h__

#include "nsIRDFDataSource.h"
#include "nsIRDFLiteral.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#if defined(XP_UNIX) || defined(XP_OS2) || defined(XP_WIN) || defined(XP_BEOS)
#define USE_NC_EXTENSION
#endif

class FileSystemDataSource : public nsIRDFDataSource
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRDFDATASOURCE

    static NS_METHOD Create(nsISupports* aOuter,
                            const nsIID& aIID, void **aResult);

    ~FileSystemDataSource() { }
    nsresult Init();

private:
    FileSystemDataSource() { }

    // helper methods
    PRBool   isFileURI(nsIRDFResource* aResource);
    PRBool   isDirURI(nsIRDFResource* aSource);
    nsresult GetVolumeList(nsISimpleEnumerator **aResult);
    nsresult GetFolderList(nsIRDFResource *source, PRBool allowHidden, PRBool onlyFirst, nsISimpleEnumerator **aResult);
    nsresult GetName(nsIRDFResource *source, nsIRDFLiteral** aResult);
    nsresult GetURL(nsIRDFResource *source, PRBool *isFavorite, nsIRDFLiteral** aResult);
    nsresult GetFileSize(nsIRDFResource *source, nsIRDFInt** aResult);
    nsresult GetLastMod(nsIRDFResource *source, nsIRDFDate** aResult);

    nsCOMPtr<nsIRDFService>    mRDFService;

    // pseudo-constants
    nsCOMPtr<nsIRDFResource>       mNC_FileSystemRoot;
    nsCOMPtr<nsIRDFResource>       mNC_Child;
    nsCOMPtr<nsIRDFResource>       mNC_Name;
    nsCOMPtr<nsIRDFResource>       mNC_URL;
    nsCOMPtr<nsIRDFResource>       mNC_Icon;
    nsCOMPtr<nsIRDFResource>       mNC_Length;
    nsCOMPtr<nsIRDFResource>       mNC_IsDirectory;
    nsCOMPtr<nsIRDFResource>       mWEB_LastMod;
    nsCOMPtr<nsIRDFResource>       mNC_FileSystemObject;
    nsCOMPtr<nsIRDFResource>       mNC_pulse;
    nsCOMPtr<nsIRDFResource>       mRDF_InstanceOf;
    nsCOMPtr<nsIRDFResource>       mRDF_type;

    nsCOMPtr<nsIRDFLiteral>        mLiteralTrue;
    nsCOMPtr<nsIRDFLiteral>        mLiteralFalse;

#ifdef USE_NC_EXTENSION
    nsresult GetExtension(nsIRDFResource *source, nsIRDFLiteral** aResult);
    nsCOMPtr<nsIRDFResource>       mNC_extension;
#endif

#ifdef  XP_WIN
    PRBool   isValidFolder(nsIRDFResource *source);
    nsresult getIEFavoriteURL(nsIRDFResource *source, nsString aFileURL, nsIRDFLiteral **urlLiteral);
    nsCOMPtr<nsIRDFResource>       mNC_IEFavoriteObject;
    nsCOMPtr<nsIRDFResource>       mNC_IEFavoriteFolder;
    nsCString                      ieFavoritesDir;
#endif

#ifdef  XP_BEOS
    nsresult getNetPositiveURL(nsIRDFResource *source, nsString aFileURL, nsIRDFLiteral **urlLiteral);
    nsCString netPositiveDir;
#endif
};

#endif // nsFileSystemDataSource_h__
