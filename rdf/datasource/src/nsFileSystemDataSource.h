/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFileSystemDataSource_h__
#define nsFileSystemDataSource_h__

#include "nsIRDFDataSource.h"
#include "nsIRDFLiteral.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

#if defined(XP_UNIX) || defined(XP_OS2) || defined(XP_WIN)
#define USE_NC_EXTENSION
#endif

class FileSystemDataSource MOZ_FINAL : public nsIRDFDataSource
{
public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(FileSystemDataSource)
    NS_DECL_NSIRDFDATASOURCE

    static nsresult Create(nsISupports* aOuter,
                           const nsIID& aIID, void **aResult);

    ~FileSystemDataSource() { }
    nsresult Init();

private:
    FileSystemDataSource() { }

    // helper methods
    bool     isFileURI(nsIRDFResource* aResource);
    bool     isDirURI(nsIRDFResource* aSource);
    nsresult GetVolumeList(nsISimpleEnumerator **aResult);
    nsresult GetFolderList(nsIRDFResource *source, bool allowHidden, bool onlyFirst, nsISimpleEnumerator **aResult);
    nsresult GetName(nsIRDFResource *source, nsIRDFLiteral** aResult);
    nsresult GetURL(nsIRDFResource *source, bool *isFavorite, nsIRDFLiteral** aResult);
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
    bool     isValidFolder(nsIRDFResource *source);
    nsresult getIEFavoriteURL(nsIRDFResource *source, nsString aFileURL, nsIRDFLiteral **urlLiteral);
    nsCOMPtr<nsIRDFResource>       mNC_IEFavoriteObject;
    nsCOMPtr<nsIRDFResource>       mNC_IEFavoriteFolder;
    nsCString                      ieFavoritesDir;
#endif
};

#endif // nsFileSystemDataSource_h__
