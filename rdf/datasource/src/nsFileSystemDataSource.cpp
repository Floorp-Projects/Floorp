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

/*
  Implementation for a file system RDF data store.
 */

#include <ctype.h> // for toupper()
#include <stdio.h>
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIEnumerator.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
#include "nsXPIDLString.h"
#include "nsRDFCID.h"
#include "rdfutil.h"
#include "nsIRDFService.h"
#include "plhash.h"
#include "plstr.h"
#include "prlong.h"
#include "prlog.h"
#include "prmem.h"
#include "prprf.h"
#include "prio.h"
#include "rdf.h"
#include "nsIRDFFileSystem.h"
#include "nsEnumeratorUtils.h"
#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIFile.h"
#include "nsEscape.h"
#include "nsCRT.h"

#ifdef  XP_WIN
#include "windef.h"
#include "winbase.h"
#include "nsILineInputStream.h"
#endif

#ifdef  XP_BEOS
#include <File.h>
#include <NodeInfo.h>
#endif

#if defined(XP_WIN) || defined(XP_BEOS)
#include "nsDirectoryServiceDefs.h"
#endif

#ifdef XP_OS2
#define INCL_DOSFILEMGR
#include <os2.h>
#endif

#if defined(XP_UNIX) || defined(XP_OS2) || defined(XP_WIN)
#define USE_NC_EXTENSION
#endif

static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);

#define NS_MOZICON_SCHEME           "moz-icon:"

static const char kFileProtocol[]         = "file://";



class FileSystemDataSource : public nsIRDFFileSystemDataSource
{
private:
    nsCOMPtr<nsISupportsArray> mObservers;

    static PRInt32 gRefCnt;

    // pseudo-constants
    static nsIRDFResource       *kNC_FileSystemRoot;
    static nsIRDFResource       *kNC_Child;
    static nsIRDFResource       *kNC_Name;
    static nsIRDFResource       *kNC_URL;
    static nsIRDFResource       *kNC_Icon;
    static nsIRDFResource       *kNC_Length;
    static nsIRDFResource       *kNC_IsDirectory;
    static nsIRDFResource       *kWEB_LastMod;
    static nsIRDFResource       *kNC_FileSystemObject;
    static nsIRDFResource       *kNC_pulse;
    static nsIRDFResource       *kRDF_InstanceOf;
    static nsIRDFResource       *kRDF_type;

#ifdef USE_NC_EXTENSION
    static nsIRDFResource       *kNC_extension;
#endif

#ifdef  XP_WIN
    static nsIRDFResource       *kNC_IEFavoriteObject;
    static nsIRDFResource       *kNC_IEFavoriteFolder;
    static char                 *ieFavoritesDir;
#endif

#ifdef  XP_BEOS
    static nsIRDFResource       *kNC_NetPositiveObject;
    static char                 *netPositiveDir;
#endif

    static nsIRDFLiteral        *kLiteralTrue;
    static nsIRDFLiteral        *kLiteralFalse;

public:

    NS_DECL_ISUPPORTS

    FileSystemDataSource(void);
    virtual     ~FileSystemDataSource(void);

    // nsIRDFDataSource methods
    NS_DECL_NSIRDFDATASOURCE

    // helper methods
    static PRBool   isFileURI(nsIRDFResource* aResource);
    static PRBool   isDirURI(nsIRDFResource* aSource);
    static nsresult GetVolumeList(nsISimpleEnumerator **aResult);
    static nsresult GetFolderList(nsIRDFResource *source, PRBool allowHidden, PRBool onlyFirst, nsISimpleEnumerator **aResult);
    static nsresult GetName(nsIRDFResource *source, nsIRDFLiteral** aResult);
    static nsresult GetURL(nsIRDFResource *source, PRBool *isFavorite, nsIRDFLiteral** aResult);
    static nsresult GetFileSize(nsIRDFResource *source, nsIRDFInt** aResult);
    static nsresult GetLastMod(nsIRDFResource *source, nsIRDFDate** aResult);

#ifdef USE_NC_EXTENSION
    static nsresult GetExtension(nsIRDFResource *source, nsIRDFLiteral** aResult);
#endif

#ifdef  XP_WIN
    static PRBool   isValidFolder(nsIRDFResource *source);
    static nsresult getIEFavoriteURL(nsIRDFResource *source, nsString aFileURL, nsIRDFLiteral **urlLiteral);
#endif

#ifdef  XP_BEOS
    static nsresult getNetPositiveURL(nsIRDFResource *source, nsString aFileURL, nsIRDFLiteral **urlLiteral);
#endif

};



static  nsIRDFService         *gRDFService = nsnull;
static  FileSystemDataSource  *gFileSystemDataSource = nsnull;

PRInt32 FileSystemDataSource::gRefCnt;

nsIRDFResource      *FileSystemDataSource::kNC_FileSystemRoot;
nsIRDFResource      *FileSystemDataSource::kNC_Child;
nsIRDFResource      *FileSystemDataSource::kNC_Name;
nsIRDFResource      *FileSystemDataSource::kNC_URL;
nsIRDFResource      *FileSystemDataSource::kNC_Icon;
nsIRDFResource      *FileSystemDataSource::kNC_Length;
nsIRDFResource      *FileSystemDataSource::kNC_IsDirectory;
nsIRDFResource      *FileSystemDataSource::kWEB_LastMod;
nsIRDFResource      *FileSystemDataSource::kNC_FileSystemObject;
nsIRDFResource      *FileSystemDataSource::kNC_pulse;
nsIRDFResource      *FileSystemDataSource::kRDF_InstanceOf;
nsIRDFResource      *FileSystemDataSource::kRDF_type;
nsIRDFLiteral       *FileSystemDataSource::kLiteralTrue;
nsIRDFLiteral       *FileSystemDataSource::kLiteralFalse;

#ifdef USE_NC_EXTENSION
nsIRDFResource      *FileSystemDataSource::kNC_extension;
#endif

#ifdef  XP_WIN
nsIRDFResource      *FileSystemDataSource::kNC_IEFavoriteObject;
nsIRDFResource      *FileSystemDataSource::kNC_IEFavoriteFolder;
char                *FileSystemDataSource::ieFavoritesDir;
#endif

#ifdef  XP_BEOS
nsIRDFResource      *FileSystemDataSource::kNC_NetPositiveObject;
char                *FileSystemDataSource::netPositiveDir;
#endif



PRBool
FileSystemDataSource::isFileURI(nsIRDFResource *r)
{
    PRBool      isFileURIFlag = PR_FALSE;
    const char  *uri = nsnull;
    
    r->GetValueConst(&uri);
    if ((uri) && (!strncmp(uri, kFileProtocol, sizeof(kFileProtocol) - 1)))
    {
        // XXX HACK HACK HACK
        if (!strchr(uri, '#'))
        {
            isFileURIFlag = PR_TRUE;
        }
    }
    return(isFileURIFlag);
}



PRBool
FileSystemDataSource::isDirURI(nsIRDFResource* source)
{
    nsresult    rv;
    const char  *uri = nsnull;

    rv = source->GetValueConst(&uri);
    if (NS_FAILED(rv)) return(PR_FALSE);

    nsCOMPtr<nsIFile> aDir;

    rv = NS_GetFileFromURLSpec(nsDependentCString(uri), getter_AddRefs(aDir));
    if (NS_FAILED(rv)) return(PR_FALSE);

    PRBool isDirFlag = PR_FALSE;

    rv = aDir->IsDirectory(&isDirFlag);
    if (NS_FAILED(rv)) return(PR_FALSE);

#ifdef  XP_MAC
    // Hide directory structure of packages under Mac OS 9/X
    nsCOMPtr<nsILocalFileMac>   aMacFile = do_QueryInterface(aDir);
    if (aMacFile)
    {
        PRBool isPackageFlag = PR_FALSE;
        rv = aMacFile->IsPackage(&isPackageFlag);
        if (NS_SUCCEEDED(rv) && (isPackageFlag == PR_TRUE))
        {
            isDirFlag = PR_FALSE;
        }
    }
#endif

    return(isDirFlag);
}



FileSystemDataSource::FileSystemDataSource(void)
{
    if (gRefCnt++ == 0)
    {
#ifdef DEBUG
        nsresult rv =
#endif
        nsServiceManager::GetService(kRDFServiceCID,
                                     NS_GET_IID(nsIRDFService),
                                     (nsISupports**) &gRDFService);

        PR_ASSERT(NS_SUCCEEDED(rv));

#ifdef  XP_WIN
        nsCOMPtr<nsIFile> file;
        NS_GetSpecialDirectory(NS_WIN_FAVORITES_DIR, getter_AddRefs(file));
        if (file)
        {
            nsCOMPtr<nsIURI> furi;
            NS_NewFileURI(getter_AddRefs(furi), file); 
            nsCAutoString favoritesDir;
            file->GetNativePath(favoritesDir);
            ieFavoritesDir = ToNewCString(favoritesDir);

            gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "IEFavorite"),
                                     &kNC_IEFavoriteObject);
            gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "IEFavoriteFolder"),
                                     &kNC_IEFavoriteFolder);
        }

#endif

#ifdef XP_BEOS

        nsCOMPtr<nsIFile> file;
        NS_GetSpecialDirectory(NS_BEOS_SETTINGS_DIR, getter_AddRefs(file));

        file->AppendNative(NS_LITERAL_CSTRING("NetPositive"));
        file->AppendNative(NS_LITERAL_CSTRING("Bookmarks"));

        nsCOMPtr<nsIURI> furi;
        NS_NewFileURI(getter_AddRefs(furi), file); 
        nsCAutoString favoritesDir;
        file->GetNativePath(favoritesDir);
        netPositiveDir = ToNewCString(favoritesDir);

#endif

        gRDFService->GetResource(NS_LITERAL_CSTRING("NC:FilesRoot"),
                                 &kNC_FileSystemRoot);
        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "child"),
                                 &kNC_Child);
        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "Name"),
                                 &kNC_Name);
        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "URL"),
                                 &kNC_URL);
        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "Icon"),
                                 &kNC_Icon);
        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "Content-Length"),
                                 &kNC_Length);
        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "IsDirectory"),
                                 &kNC_IsDirectory);
        gRDFService->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastModifiedDate"),
                                 &kWEB_LastMod);
        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "FileSystemObject"),
                                 &kNC_FileSystemObject);
        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "pulse"),
                                 &kNC_pulse);

        gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "instanceOf"),
                                 &kRDF_InstanceOf);
        gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "type"),
                                 &kRDF_type);

#ifdef USE_NC_EXTENSION
        gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "extension"),
                                 &kNC_extension);
#endif
        gRDFService->GetLiteral(NS_LITERAL_STRING("true").get(),       &kLiteralTrue);
        gRDFService->GetLiteral(NS_LITERAL_STRING("false").get(),      &kLiteralFalse);
        gFileSystemDataSource = this;
    }
}



FileSystemDataSource::~FileSystemDataSource (void)
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: FileSystemDataSource\n", gInstanceCount);
#endif

    if (--gRefCnt == 0) {
        NS_RELEASE(kNC_FileSystemRoot);
        NS_RELEASE(kNC_Child);
        NS_RELEASE(kNC_Name);
        NS_RELEASE(kNC_URL);
        NS_RELEASE(kNC_Icon);
        NS_RELEASE(kNC_Length);
        NS_RELEASE(kNC_IsDirectory);
        NS_RELEASE(kWEB_LastMod);
        NS_RELEASE(kNC_FileSystemObject);
        NS_RELEASE(kNC_pulse);
        NS_RELEASE(kRDF_InstanceOf);
        NS_RELEASE(kRDF_type);

#ifdef  XP_WIN
        NS_IF_RELEASE(kNC_IEFavoriteObject);
        NS_IF_RELEASE(kNC_IEFavoriteFolder);

        if (ieFavoritesDir)
        {
            nsMemory::Free(ieFavoritesDir);
            ieFavoritesDir = nsnull;
        }
#endif

#ifdef BEOS

        if (netPositiveDir)
        {
            nsMemory::Free(netPositiveDir);
            netPositiveDir = nsnull;
        }
#endif
#ifdef USE_NC_EXTENSION
        NS_RELEASE(kNC_extension);
#endif

        NS_RELEASE(kLiteralTrue);
        NS_RELEASE(kLiteralFalse);

        gFileSystemDataSource = nsnull;
        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
        gRDFService = nsnull;
    }
}



NS_IMPL_ISUPPORTS1(FileSystemDataSource, nsIRDFDataSource)



NS_IMETHODIMP
FileSystemDataSource::GetURI(char **uri)
{
    NS_PRECONDITION(uri != nsnull, "null ptr");
    if (! uri)
        return NS_ERROR_NULL_POINTER;

    if ((*uri = nsCRT::strdup("rdf:files")) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::GetSource(nsIRDFResource* property,
                                nsIRDFNode* target,
                                PRBool tv,
                                nsIRDFResource** source /* out */)
{
    NS_PRECONDITION(property != nsnull, "null ptr");
    if (! property)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(target != nsnull, "null ptr");
    if (! target)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(source != nsnull, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    *source = nsnull;
    return NS_RDF_NO_VALUE;
}



NS_IMETHODIMP
FileSystemDataSource::GetSources(nsIRDFResource *property,
                                 nsIRDFNode *target,
                                 PRBool tv,
                                 nsISimpleEnumerator **sources /* out */)
{
//  NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::GetTarget(nsIRDFResource *source,
                                nsIRDFResource *property,
                                PRBool tv,
                                nsIRDFNode **target /* out */)
{
    NS_PRECONDITION(source != nsnull, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(property != nsnull, "null ptr");
    if (! property)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(target != nsnull, "null ptr");
    if (! target)
        return NS_ERROR_NULL_POINTER;

    *target = nsnull;

    nsresult        rv = NS_RDF_NO_VALUE;

    // we only have positive assertions in the file system data source.
    if (! tv)
        return NS_RDF_NO_VALUE;

    if (source == kNC_FileSystemRoot)
    {
        if (property == kNC_pulse)
        {
            nsIRDFLiteral   *pulseLiteral;
            gRDFService->GetLiteral(NS_LITERAL_STRING("12").get(), &pulseLiteral);
            *target = pulseLiteral;
            return NS_OK;
        }
    }
    else if (isFileURI(source))
    {
        if (property == kNC_Name)
        {
            nsCOMPtr<nsIRDFLiteral> name;
            rv = GetName(source, getter_AddRefs(name));
            if (NS_FAILED(rv)) return(rv);
            if (!name)  rv = NS_RDF_NO_VALUE;
            if (rv == NS_RDF_NO_VALUE)  return(rv);
            return name->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
        }
        else if (property == kNC_URL)
        {
            nsCOMPtr<nsIRDFLiteral> url;
            rv = GetURL(source, nsnull, getter_AddRefs(url));
            if (NS_FAILED(rv)) return(rv);
            if (!url)   rv = NS_RDF_NO_VALUE;
            if (rv == NS_RDF_NO_VALUE)  return(rv);

            return url->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
        }
        else if (property == kNC_Icon)
        {
            nsCOMPtr<nsIRDFLiteral> url;
            PRBool isFavorite = PR_FALSE;
            rv = GetURL(source, &isFavorite, getter_AddRefs(url));
            if (NS_FAILED(rv)) return(rv);
            if (isFavorite || !url) rv = NS_RDF_NO_VALUE;
            if (rv == NS_RDF_NO_VALUE)  return(rv);
            
            const PRUnichar *uni = nsnull;
            url->GetValueConst(&uni);
            if (!uni)   return(NS_RDF_NO_VALUE);
            nsAutoString    urlStr;
            urlStr.Assign(NS_LITERAL_STRING(NS_MOZICON_SCHEME).get());
            urlStr.Append(uni);

            rv = gRDFService->GetLiteral(urlStr.get(), getter_AddRefs(url));
            if (NS_FAILED(rv) || !url)    return(NS_RDF_NO_VALUE);
            return url->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
        }
        else if (property == kNC_Length)
        {
            nsCOMPtr<nsIRDFInt> fileSize;
            rv = GetFileSize(source, getter_AddRefs(fileSize));
            if (NS_FAILED(rv)) return(rv);
            if (!fileSize)  rv = NS_RDF_NO_VALUE;
            if (rv == NS_RDF_NO_VALUE)  return(rv);

            return fileSize->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
        }
        else  if (property == kNC_IsDirectory)
        {
            *target = (isDirURI(source)) ? kLiteralTrue : kLiteralFalse;
            NS_ADDREF(*target);
            return NS_OK;
        }
        else if (property == kWEB_LastMod)
        {
            nsCOMPtr<nsIRDFDate> lastMod;
            rv = GetLastMod(source, getter_AddRefs(lastMod));
            if (NS_FAILED(rv)) return(rv);
            if (!lastMod)   rv = NS_RDF_NO_VALUE;
            if (rv == NS_RDF_NO_VALUE)  return(rv);

            return lastMod->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
        }
        else if (property == kRDF_type)
        {
            nsCString type;
            rv = kNC_FileSystemObject->GetValueUTF8(type);
            if (NS_FAILED(rv)) return(rv);

#ifdef  XP_WIN
            // under Windows, if its an IE favorite, return that type
            if (ieFavoritesDir)
            {
                nsCString uri;
                rv = source->GetValueUTF8(uri);
                if (NS_FAILED(rv)) return(rv);

                NS_ConvertUTF8toUTF16 theURI(uri);

                if (theURI.Find(ieFavoritesDir) == 0)
                {
                    if (theURI[theURI.Length() - 1] == '/')
                    {
                        rv = kNC_IEFavoriteFolder->GetValueUTF8(type);
                    }
                    else
                    {
                        rv = kNC_IEFavoriteObject->GetValueUTF8(type);
                    }
                    if (NS_FAILED(rv)) return(rv);
                }
            }
#endif

            NS_ConvertUTF8toUTF16 url(type);
            nsCOMPtr<nsIRDFLiteral> literal;
            gRDFService->GetLiteral(url.get(), getter_AddRefs(literal));
            rv = literal->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
            return(rv);
        }
        else if (property == kNC_pulse)
        {
            nsCOMPtr<nsIRDFLiteral> pulseLiteral;
            gRDFService->GetLiteral(NS_LITERAL_STRING("12").get(), getter_AddRefs(pulseLiteral));
            rv = pulseLiteral->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
            return(rv);
        }
        else if (property == kNC_Child)
        {
            // Oh this is evil. Somebody kill me now.
            nsCOMPtr<nsISimpleEnumerator> children;
            rv = GetFolderList(source, PR_FALSE, PR_TRUE, getter_AddRefs(children));
            if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE)) return(rv);

            PRBool hasMore;
            rv = children->HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return(rv);

            if (hasMore)
            {
                nsCOMPtr<nsISupports> isupports;
                rv = children->GetNext(getter_AddRefs(isupports));
                if (NS_FAILED(rv)) return(rv);

                return isupports->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
            }
        }
#ifdef USE_NC_EXTENSION
        else if (property == kNC_extension)
        {
            nsCOMPtr<nsIRDFLiteral> extension;
            rv = GetExtension(source, getter_AddRefs(extension));
            if (!extension)    rv = NS_RDF_NO_VALUE;
            if (rv == NS_RDF_NO_VALUE) return(rv);
            return extension->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
        }
#endif
    }

    return(NS_RDF_NO_VALUE);
}



NS_IMETHODIMP
FileSystemDataSource::GetTargets(nsIRDFResource *source,
                nsIRDFResource *property,
                PRBool tv,
                nsISimpleEnumerator **targets /* out */)
{
    NS_PRECONDITION(source != nsnull, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(property != nsnull, "null ptr");
    if (! property)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(targets != nsnull, "null ptr");
    if (! targets)
        return NS_ERROR_NULL_POINTER;

    *targets = nsnull;

    // we only have positive assertions in the file system data source.
    if (! tv)
        return NS_RDF_NO_VALUE;

    nsresult rv;

    if (source == kNC_FileSystemRoot)
    {
        if (property == kNC_Child)
        {
            return GetVolumeList(targets);
        }
        else if (property == kNC_pulse)
        {
            nsIRDFLiteral   *pulseLiteral;
            gRDFService->GetLiteral(NS_LITERAL_STRING("12").get(), &pulseLiteral);
            nsISimpleEnumerator* result = new nsSingletonEnumerator(pulseLiteral);
            NS_RELEASE(pulseLiteral);

            if (! result)
                return NS_ERROR_OUT_OF_MEMORY;

            NS_ADDREF(result);
            *targets = result;
            return NS_OK;
        }
    }
    else if (isFileURI(source))
    {
        if (property == kNC_Child)
        {
            return GetFolderList(source, PR_FALSE, PR_FALSE, targets);
        }
        else if (property == kNC_Name)
        {
            nsCOMPtr<nsIRDFLiteral> name;
            rv = GetName(source, getter_AddRefs(name));
            if (NS_FAILED(rv)) return rv;

            nsISimpleEnumerator* result = new nsSingletonEnumerator(name);
            if (! result)
                return NS_ERROR_OUT_OF_MEMORY;

            NS_ADDREF(result);
            *targets = result;
            return NS_OK;
        }
        else if (property == kNC_URL)
        {
            nsCOMPtr<nsIRDFLiteral> url;
            rv = GetURL(source, nsnull, getter_AddRefs(url));
            if (NS_FAILED(rv)) return rv;

            nsISimpleEnumerator* result = new nsSingletonEnumerator(url);
            if (! result)
                return NS_ERROR_OUT_OF_MEMORY;

            NS_ADDREF(result);
            *targets = result;
            return NS_OK;
        }
        else if (property == kRDF_type)
        {
            nsCString uri;
            rv = kNC_FileSystemObject->GetValueUTF8(uri);
            if (NS_FAILED(rv)) return rv;

            NS_ConvertUTF8toUTF16 url(uri);

            nsCOMPtr<nsIRDFLiteral> literal;
            rv = gRDFService->GetLiteral(url.get(), getter_AddRefs(literal));
            if (NS_FAILED(rv)) return rv;

            nsISimpleEnumerator* result = new nsSingletonEnumerator(literal);

            if (! result)
                return NS_ERROR_OUT_OF_MEMORY;

            NS_ADDREF(result);
            *targets = result;
            return NS_OK;
        }
        else if (property == kNC_pulse)
        {
            nsCOMPtr<nsIRDFLiteral> pulseLiteral;
            rv = gRDFService->GetLiteral(NS_LITERAL_STRING("12").get(),
                getter_AddRefs(pulseLiteral));
            if (NS_FAILED(rv)) return rv;

            nsISimpleEnumerator* result = new nsSingletonEnumerator(pulseLiteral);

            if (! result)
                return NS_ERROR_OUT_OF_MEMORY;

            NS_ADDREF(result);
            *targets = result;
            return NS_OK;
        }
    }

    return NS_NewEmptyEnumerator(targets);
}



NS_IMETHODIMP
FileSystemDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
    return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FileSystemDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
    return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FileSystemDataSource::Change(nsIRDFResource* aSource,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aOldTarget,
                             nsIRDFNode* aNewTarget)
{
    return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FileSystemDataSource::Move(nsIRDFResource* aOldSource,
                           nsIRDFResource* aNewSource,
                           nsIRDFResource* aProperty,
                           nsIRDFNode* aTarget)
{
    return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
FileSystemDataSource::HasAssertion(nsIRDFResource *source,
                             nsIRDFResource *property,
                             nsIRDFNode *target,
                             PRBool tv,
                             PRBool *hasAssertion /* out */)
{
    NS_PRECONDITION(source != nsnull, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(property != nsnull, "null ptr");
    if (! property)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(target != nsnull, "null ptr");
    if (! target)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(hasAssertion != nsnull, "null ptr");
    if (! hasAssertion)
        return NS_ERROR_NULL_POINTER;

    // we only have positive assertions in the file system data source.
    *hasAssertion = PR_FALSE;

    if (! tv) {
        return NS_OK;
    }

    if ((source == kNC_FileSystemRoot) || isFileURI(source))
    {
        if (property == kRDF_type)
        {
            nsCOMPtr<nsIRDFResource> resource( do_QueryInterface(target) );
            if (resource.get() == kRDF_type)
            {
                *hasAssertion = PR_TRUE;
            }
        }
#ifdef USE_NC_EXTENSION
        else if (property == kNC_extension)
        {
            // Cheat just a little here by making dirs always match
            if (isDirURI(source))
            {
                *hasAssertion = PR_TRUE;
            }
            else
            {
                nsCOMPtr<nsIRDFLiteral> extension;
                GetExtension(source, getter_AddRefs(extension));
                if (extension.get() == target)
                {
                    *hasAssertion = PR_TRUE;
                }
            }
        }
#endif
        else if (property == kNC_IsDirectory)
        {
            PRBool isDir = isDirURI(source);
            PRBool isEqual = PR_FALSE;
            target->EqualsNode(kLiteralTrue, &isEqual);
            if (isEqual)
            {
                *hasAssertion = isDir;
            }
            else
            {
                target->EqualsNode(kLiteralFalse, &isEqual);
                if (isEqual)
                    *hasAssertion = !isDir;
            }
        }
    }

    return NS_OK;
}



NS_IMETHODIMP 
FileSystemDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP 
FileSystemDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
    *result = PR_FALSE;

    if (aSource == kNC_FileSystemRoot)
    {
        *result = (aArc == kNC_Child || aArc == kNC_pulse);
    }
    else if (isFileURI(aSource))
    {
        if (aArc == kNC_pulse)
        {
            *result = PR_TRUE;
        }
        else if (isDirURI(aSource))
        {
#ifdef  XP_WIN
            *result = isValidFolder(aSource);
#else
            *result = PR_TRUE;
#endif
        }
        else if (aArc == kNC_pulse || aArc == kNC_Name || aArc == kNC_Icon ||
                 aArc == kNC_URL || aArc == kNC_Length || aArc == kWEB_LastMod ||
                 aArc == kNC_FileSystemObject || aArc == kRDF_InstanceOf ||
                 aArc == kRDF_type)
        {
            *result = PR_TRUE;
        }
    }
    return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsISimpleEnumerator ** labels /* out */)
{
//  NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::ArcLabelsOut(nsIRDFResource *source,
                   nsISimpleEnumerator **labels /* out */)
{
    NS_PRECONDITION(source != nsnull, "null ptr");
    if (! source)
    return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(labels != nsnull, "null ptr");
    if (! labels)
    return NS_ERROR_NULL_POINTER;

    nsresult rv;

    if (source == kNC_FileSystemRoot)
    {
        nsCOMPtr<nsISupportsArray> array;
        rv = NS_NewISupportsArray(getter_AddRefs(array));
        if (NS_FAILED(rv)) return rv;

        array->AppendElement(kNC_Child);
        array->AppendElement(kNC_pulse);

        nsISimpleEnumerator* result = new nsArrayEnumerator(array);
        if (! result)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(result);
        *labels = result;
        return NS_OK;
    }
    else if (isFileURI(source))
    {
        nsCOMPtr<nsISupportsArray> array;
        rv = NS_NewISupportsArray(getter_AddRefs(array));
        if (NS_FAILED(rv)) return rv;

        if (isDirURI(source))
        {
#ifdef  XP_WIN
            if (isValidFolder(source) == PR_TRUE)
            {
                array->AppendElement(kNC_Child);
            }
#else
            array->AppendElement(kNC_Child);
#endif
            array->AppendElement(kNC_pulse);
        }

        nsISimpleEnumerator* result = new nsArrayEnumerator(array);
        if (! result)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(result);
        *labels = result;
        return NS_OK;
    }

    return NS_NewEmptyEnumerator(labels);
}



NS_IMETHODIMP
FileSystemDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
    NS_NOTYETIMPLEMENTED("sorry!");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::AddObserver(nsIRDFObserver *n)
{
    NS_PRECONDITION(n != nsnull, "null ptr");
    if (! n)
        return NS_ERROR_NULL_POINTER;

    if (! mObservers)
    {
        nsresult rv;
        rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
        if (NS_FAILED(rv)) return rv;
    }
    mObservers->AppendElement(n);
    return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::RemoveObserver(nsIRDFObserver *n)
{
    NS_PRECONDITION(n != nsnull, "null ptr");
    if (! n)
        return NS_ERROR_NULL_POINTER;

    if (! mObservers)
        return NS_OK;

    mObservers->RemoveElement(n);
    return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::GetAllCmds(nsIRDFResource* source,
                                     nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
    return(NS_NewEmptyEnumerator(commands));
}



NS_IMETHODIMP
FileSystemDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                       nsIRDFResource*   aCommand,
                                       nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                       PRBool* aResult)
{
    return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
FileSystemDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
    return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
FileSystemDataSource::BeginUpdateBatch()
{
    return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::EndUpdateBatch()
{
    return NS_OK;
}



nsresult
NS_NewRDFFileSystemDataSource(nsIRDFDataSource **result)
{
    if (!result)
        return NS_ERROR_NULL_POINTER;

    // only one file system data source
    if (nsnull == gFileSystemDataSource)
    {
        if ((gFileSystemDataSource = new FileSystemDataSource()) == nsnull)
        {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    NS_ADDREF(gFileSystemDataSource);
    *result = gFileSystemDataSource;
    return NS_OK;
}



nsresult
FileSystemDataSource::GetVolumeList(nsISimpleEnumerator** aResult)
{
    nsresult rv;
    nsCOMPtr<nsISupportsArray> volumes;

    rv = NS_NewISupportsArray(getter_AddRefs(volumes));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> vol;

#ifdef  XP_MAC
    StrFileName     fname;
    HParamBlockRec  pb;
    for (int16 volNum = 1; ; volNum++)
    {
        pb.volumeParam.ioCompletion = NULL;
        pb.volumeParam.ioVolIndex = volNum;
        pb.volumeParam.ioNamePtr = (StringPtr)fname;
        if (PBHGetVInfo(&pb,FALSE) != noErr)
            break;
        FSSpec fss(pb.volumeParam.ioVRefNum, fsRtParID, fname);
        nsCOMPtr<nsILocalFileMac> lf;
        NS_NewLocalFileWithFSSpec(fss, true, getter_AddRefs(lf));

        nsCOMPtr<nsIURI> furi;
        NS_NewFileURI(getter_AddRefs(furi), lf); 

        nsXPIDLCString spec;
        furi->GetSpec(getter_Copies(spec);

        rv = gRDFService->GetResource(spec, getter_AddRefs(vol));
        if (NS_FAILED(rv)) return rv;

        volumes->AppendElement(vol);
    }
#endif

#ifdef  XP_WIN
    PRInt32         driveType;
    char            drive[32];
    PRInt32         volNum;
    char            *url;

    for (volNum = 0; volNum < 26; volNum++)
    {
        sprintf(drive, "%c:\\", volNum + 'A');
        driveType = GetDriveType(drive);
        if (driveType != DRIVE_UNKNOWN && driveType != DRIVE_NO_ROOT_DIR)
        {
            if (nsnull != (url = PR_smprintf("file:///%c|/", volNum + 'A')))
            {
                rv = gRDFService->GetResource(nsDependentCString(url),
                                              getter_AddRefs(vol));
                PR_Free(url);

                if (NS_FAILED(rv)) return rv;
                volumes->AppendElement(vol);
            }
        }
    }
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
    gRDFService->GetResource(NS_LITERAL_CSTRING("file:///"), getter_AddRefs(vol));
    volumes->AppendElement(vol);
#endif

#ifdef XP_OS2
    ULONG ulDriveNo = 0;
    ULONG ulDriveMap = 0;
    char *url;

    rv = DosQueryCurrentDisk(&ulDriveNo, &ulDriveMap);
    if (NS_FAILED(rv))
        return rv;

    for (int volNum = 0; volNum < 26; volNum++)
    {
        if (((ulDriveMap << (31 - volNum)) >> 31))
        {
            if (nsnull != (url = PR_smprintf("file:///%c|/", volNum + 'A')))
            {
                rv = gRDFService->GetResource(nsDependentCString(url), getter_AddRefs(vol));
                PR_Free(url);

                if (NS_FAILED(rv)) return rv;
                volumes->AppendElement(vol);
            }
        }

    }
#endif

    nsISimpleEnumerator* result = new nsArrayEnumerator(volumes);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;

    return NS_OK;
}



#ifdef  XP_WIN
PRBool
FileSystemDataSource::isValidFolder(nsIRDFResource *source)
{
    PRBool  isValid = PR_TRUE;
    if (!ieFavoritesDir)    return(isValid);

    nsresult        rv;
    nsCString       uri;
    rv = source->GetValueUTF8(uri);
    if (NS_FAILED(rv)) return(isValid);

    NS_ConvertUTF8toUTF16 theURI(uri);
    if (theURI.Find(ieFavoritesDir) == 0)
    {
        isValid = PR_FALSE;

        nsCOMPtr<nsISimpleEnumerator>   folderEnum;
        if (NS_SUCCEEDED(rv = GetFolderList(source, PR_TRUE, PR_FALSE, getter_AddRefs(folderEnum))))
        {
            PRBool      hasAny = PR_FALSE, hasMore;
            while (NS_SUCCEEDED(folderEnum->HasMoreElements(&hasMore)) &&
                    (hasMore == PR_TRUE))
            {
                hasAny = PR_TRUE;

                nsCOMPtr<nsISupports>       isupports;
                if (NS_FAILED(rv = folderEnum->GetNext(getter_AddRefs(isupports))))
                    break;
                nsCOMPtr<nsIRDFResource>    res = do_QueryInterface(isupports);
                if (!res)   break;

                nsCOMPtr<nsIRDFLiteral>     nameLiteral;
                if (NS_FAILED(rv = GetName(res, getter_AddRefs(nameLiteral))))
                    break;
                
                const PRUnichar         *uniName;
                if (NS_FAILED(rv = nameLiteral->GetValueConst(&uniName)))
                    break;
                nsAutoString            name(uniName);

                // An empty folder, or a folder that contains just "desktop.ini",
                // is considered to be a IE Favorite; otherwise, its a folder
                if (!name.LowerCaseEqualsLiteral("desktop.ini"))
                {
                    isValid = PR_TRUE;
                    break;
                }
            }
            if (hasAny == PR_FALSE) isValid = PR_TRUE;
        }
    }
    return(isValid);
}
#endif



nsresult
FileSystemDataSource::GetFolderList(nsIRDFResource *source, PRBool allowHidden,
                PRBool onlyFirst, nsISimpleEnumerator** aResult)
{
    if (!isDirURI(source))
        return(NS_RDF_NO_VALUE);

    nsresult                    rv;
    nsCOMPtr<nsISupportsArray>  nameArray;

    rv = NS_NewISupportsArray(getter_AddRefs(nameArray));
    if (NS_FAILED(rv))
        return(rv);

    const char      *parentURI = nsnull;
    rv = source->GetValueConst(&parentURI);
    if (NS_FAILED(rv))
        return(rv);
    if (!parentURI)
        return(NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIURI>    aIURI;
    if (NS_FAILED(rv = NS_NewURI(getter_AddRefs(aIURI), nsDependentCString(parentURI))))
        return(rv);

    nsCOMPtr<nsIFileURL>    fileURL = do_QueryInterface(aIURI);
    if (!fileURL)
        return(PR_FALSE);

    nsCOMPtr<nsIFile>   aDir;
    if (NS_FAILED(rv = fileURL->GetFile(getter_AddRefs(aDir))))
        return(rv);

    // ensure that we DO NOT resolve aliases
    nsCOMPtr<nsILocalFile>  aDirLocal = do_QueryInterface(aDir);
    if (aDirLocal)
        aDirLocal->SetFollowLinks(PR_FALSE);

    nsCOMPtr<nsISimpleEnumerator>   dirContents;
    if (NS_FAILED(rv = aDir->GetDirectoryEntries(getter_AddRefs(dirContents))))
        return(rv);
    if (!dirContents)
        return(NS_ERROR_UNEXPECTED);

    PRBool          hasMore;
    while(NS_SUCCEEDED(rv = dirContents->HasMoreElements(&hasMore)) &&
        (hasMore == PR_TRUE))
    {
        nsCOMPtr<nsISupports>   isupports;
        if (NS_FAILED(rv = dirContents->GetNext(getter_AddRefs(isupports))))
            break;

        nsCOMPtr<nsIFile>   aFile = do_QueryInterface(isupports);
        if (!aFile)
            break;

        if (allowHidden == PR_FALSE)
        {
            PRBool          hiddenFlag = PR_FALSE;
            if (NS_FAILED(rv = aFile->IsHidden(&hiddenFlag)))
                break;
            if (hiddenFlag == PR_TRUE)
                continue;
        }

        nsAutoString leafStr;
        if (NS_FAILED(rv = aFile->GetLeafName(leafStr)))
            break;
        if (leafStr.IsEmpty())
            continue;
  
        nsCAutoString           fullURI;
        fullURI.Assign(parentURI);
        if (fullURI.Last() != '/')
        {
            fullURI.Append('/');
        }

        char    *escLeafStr = nsEscape(NS_ConvertUCS2toUTF8(leafStr).get(), url_Path);
        leafStr.Truncate();

        if (!escLeafStr)
            continue;
  
        nsCAutoString           leaf(escLeafStr);
        Recycle(escLeafStr);
        escLeafStr = nsnull;

        // using nsEscape() [above] doesn't escape slashes, so do that by hand
        PRInt32         aOffset;
        while ((aOffset = leaf.FindChar('/')) >= 0)
        {
            leaf.Cut((PRUint32)aOffset, 1);
            leaf.Insert("%2F", (PRUint32)aOffset);
        }

        // append the encoded name
        fullURI.Append(leaf);

        PRBool          dirFlag = PR_FALSE;
        rv = aFile->IsDirectory(&dirFlag);
        if (NS_SUCCEEDED(rv) && (dirFlag == PR_TRUE))
        {
            fullURI.Append('/');
        }

        nsCOMPtr<nsIRDFResource>    fileRes;
        gRDFService->GetResource(fullURI, getter_AddRefs(fileRes));

        nameArray->AppendElement(fileRes);

        if (onlyFirst == PR_TRUE)
            break;
    }

    nsISimpleEnumerator* result = new nsArrayEnumerator(nameArray);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;

    return NS_OK;
}



nsresult
FileSystemDataSource::GetLastMod(nsIRDFResource *source, nsIRDFDate **aResult)
{
    *aResult = nsnull;

    nsresult        rv;
    const char      *uri = nsnull;

    rv = source->GetValueConst(&uri);
    if (NS_FAILED(rv)) return(rv);
    if (!uri)
        return(NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIURI>    aIURI;
    if (NS_FAILED(rv = NS_NewURI(getter_AddRefs(aIURI), nsDependentCString(uri))))
        return(rv);

    nsCOMPtr<nsIFileURL>    fileURL = do_QueryInterface(aIURI);
    if (!fileURL)
        return(PR_FALSE);

    nsCOMPtr<nsIFile>   aFile;
    if (NS_FAILED(rv = fileURL->GetFile(getter_AddRefs(aFile))))
        return(rv);
    if (!aFile)
        return(NS_ERROR_UNEXPECTED);

    // ensure that we DO NOT resolve aliases
    nsCOMPtr<nsILocalFile>  aFileLocal = do_QueryInterface(aFile);
    if (aFileLocal)
        aFileLocal->SetFollowLinks(PR_FALSE);

    PRInt64 lastModDate;
    if (NS_FAILED(rv = aFile->GetLastModifiedTime(&lastModDate)))
        return(rv);

    // convert from milliseconds to seconds
    PRTime      temp64, thousand;
    LL_I2L(thousand, PR_MSEC_PER_SEC);
    LL_MUL(temp64, lastModDate, thousand);

    gRDFService->GetDateLiteral(temp64, aResult);

    return(NS_OK);
}



nsresult
FileSystemDataSource::GetFileSize(nsIRDFResource *source, nsIRDFInt **aResult)
{
    *aResult = nsnull;

    nsresult        rv;
    const char      *uri = nsnull;

    rv = source->GetValueConst(&uri);
    if (NS_FAILED(rv))
        return(rv);
    if (!uri)
        return(NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIURI>    aIURI;
    if (NS_FAILED(rv = NS_NewURI(getter_AddRefs(aIURI), nsDependentCString(uri))))
        return(rv);

    nsCOMPtr<nsIFileURL>    fileURL = do_QueryInterface(aIURI);
    if (!fileURL)
        return(PR_FALSE);

    nsCOMPtr<nsIFile>   aFile;
    if (NS_FAILED(rv = fileURL->GetFile(getter_AddRefs(aFile))))
        return(rv);
    if (!aFile)
        return(NS_ERROR_UNEXPECTED);

    // ensure that we DO NOT resolve aliases
    nsCOMPtr<nsILocalFile>  aFileLocal = do_QueryInterface(aFile);
    if (aFileLocal)
        aFileLocal->SetFollowLinks(PR_FALSE);

    // don't do anything with directories
    PRBool  isDir = PR_FALSE;
    if (NS_FAILED(rv = aFile->IsDirectory(&isDir)))
        return(rv);
    if (isDir == PR_TRUE)
        return(NS_RDF_NO_VALUE);

    PRInt64     aFileSize64;
#ifdef  XP_MAC
    // on Mac, get total file size (data + resource fork)
    nsCOMPtr<nsILocalFileMac>   aMacFile = do_QueryInterface(aFile);
    if (!aMacFile)
        return(NS_ERROR_UNEXPECTED);
    if (NS_FAILED(rv = aMacFile->GetFileSizeWithResFork(&aFileSize64)))
        return(rv);
#else
    if (NS_FAILED(rv = aFile->GetFileSize(&aFileSize64)))
        return(rv);
#endif

    // convert 64bits to 32bits
    PRInt32     aFileSize32 = 0;
    LL_L2I(aFileSize32, aFileSize64);

    gRDFService->GetIntLiteral(aFileSize32, aResult);

    return(NS_OK);
}



nsresult
FileSystemDataSource::GetName(nsIRDFResource *source, nsIRDFLiteral **aResult)
{
    nsresult        rv;
    const char      *uri = nsnull;

    rv = source->GetValueConst(&uri);
    if (NS_FAILED(rv))
        return(rv);
    if (!uri)
        return(NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIURI>    aIURI;
    if (NS_FAILED(rv = NS_NewURI(getter_AddRefs(aIURI), nsDependentCString(uri))))
        return(rv);

    nsCOMPtr<nsIFileURL>    fileURL = do_QueryInterface(aIURI);
    if (!fileURL)
        return(PR_FALSE);

    nsCOMPtr<nsIFile>   aFile;
    if (NS_FAILED(rv = fileURL->GetFile(getter_AddRefs(aFile))))
        return(rv);
    if (!aFile)
        return(NS_ERROR_UNEXPECTED);

    // ensure that we DO NOT resolve aliases
    nsCOMPtr<nsILocalFile>  aFileLocal = do_QueryInterface(aFile);
    if (aFileLocal)
        aFileLocal->SetFollowLinks(PR_FALSE);

    nsAutoString name;
    if (NS_FAILED(rv = aFile->GetLeafName(name)))
        return(rv);
    if (name.IsEmpty())
        return(NS_ERROR_UNEXPECTED);

#ifdef  XP_MAC
    nsCOMPtr<nsILocalFileMac>   aMacFile = do_QueryInterface(aFile);
    if (aMacFile)
    {
        PRBool isPackageFlag = PR_FALSE;
        rv = aMacFile->IsPackage(&isPackageFlag);
        if (NS_SUCCEEDED(rv) && (isPackageFlag == PR_TRUE))
        {
            // mungle package names under Mac OS 9/X
            PRUint32 len = name.Length();
            if (name.RFind(".app", PR_TRUE) == len - 4)
            {
                name.SetLength(len-4);
            }
        }
    }
#endif

#ifdef  XP_WIN
    // special hack for IE favorites under Windows; strip off the
    // trailing ".url" or ".lnk" at the end of IE favorites names
    PRInt32 nameLen = name.Length();
    if ((strncmp(uri, ieFavoritesDir, strlen(ieFavoritesDir)) == 0) && (nameLen > 4))
    {
        nsAutoString extension;
        name.Right(extension, 4);
        if (extension.LowerCaseEqualsLiteral(".url") ||
            extension.LowerCaseEqualsLiteral(".lnk"))
        {
            name.Truncate(nameLen - 4);
        }
    }
#endif

#ifdef  XP_BEOS
    // under BEOS, try and get the "META:title" attribute (if its a file)
    if (strstr(uri, netPositiveDir) != 0)
    {
        PRBool value;
        if ((NS_SUCCEEDED(aFileLocal->IsFile(&value) && value)) ||
            (NS_SUCCEEDED(aFileLocal->IsDirectory(&value) && value)))
        {
            nsXPIDLCString nativePath;
            aFileLocal->GetNativePath(nativePath);

            rv = NS_ERROR_FAILURE;
            if (nativePath) 
            {
                BFile   bf(nativePath.get(), B_READ_ONLY);
                if (bf.InitCheck() == B_OK)
                {
                    char        beNameAttr[4096];
                    ssize_t     len;

                    if ((len = bf.ReadAttr("META:title", B_STRING_TYPE,
                        0, beNameAttr, sizeof(beNameAttr)-1)) > 0)
                    {
                        beNameAttr[len] = '\0';
                        CopyUTF8toUTF16(beNameAttr, name);
                        rv = NS_OK;
                    }
                }
            }
            if (NS_OK != rv)
            {
                nsCAutoString leafName;
                rv = aFileLocal->GetNativeLeafName(leafName);
                if (NS_SUCCEEDED(rv)) {
                    CopyUTF8toUTF16(leafName, name);
                    rv = NS_OK;
                }
            }
        }
    }
#endif

    gRDFService->GetLiteral(name.get(), aResult);

    return NS_OK;
}



#ifdef USE_NC_EXTENSION
nsresult
FileSystemDataSource::GetExtension(nsIRDFResource *source, nsIRDFLiteral **aResult)
{
    nsCOMPtr<nsIRDFLiteral> name;
    nsresult rv = GetName(source, getter_AddRefs(name));
    if (NS_FAILED(rv))
        return rv;

    const PRUnichar* unicodeLeafName;
    rv = name->GetValueConst(&unicodeLeafName);
    if (NS_FAILED(rv))
        return rv;

    nsAutoString filename(unicodeLeafName);
    PRInt32 lastDot = filename.RFindChar('.');
    if (lastDot == -1)
    {
        gRDFService->GetLiteral(EmptyString().get(), aResult);
    }
    else
    {
        nsAutoString extension;
        filename.Right(extension, (filename.Length() - lastDot));
        gRDFService->GetLiteral(extension.get(), aResult);
    }

    return NS_OK;
}
#endif

#ifdef  XP_WIN
nsresult
FileSystemDataSource::getIEFavoriteURL(nsIRDFResource *source, nsString aFileURL, nsIRDFLiteral **urlLiteral)
{
    nsresult        rv = NS_OK;
    
    *urlLiteral = nsnull;

    nsCOMPtr<nsIFile> f;
    NS_GetFileFromURLSpec(NS_ConvertUCS2toUTF8(aFileURL), getter_AddRefs(f)); 

    PRBool value;

    if (NS_SUCCEEDED(f->IsDirectory(&value)) && value)
    {
        if (isValidFolder(source))
            return(NS_RDF_NO_VALUE);
        aFileURL.AppendLiteral("desktop.ini");
    }
    else if (aFileURL.Length() > 4)
    {
        nsAutoString    extension;

        aFileURL.Right(extension, 4);
        if (!extension.LowerCaseEqualsLiteral(".url"))
        {
            return(NS_RDF_NO_VALUE);
        }
    }

    nsCOMPtr<nsIInputStream> strm;
    NS_NewLocalFileInputStream(getter_AddRefs(strm),f);
    nsCOMPtr<nsILineInputStream> linereader = do_QueryInterface(strm, &rv);

    nsAutoString    line;
    nsCAutoString   cLine;
    while(NS_SUCCEEDED(rv))
    {
        PRBool  isEOF;
        rv = linereader->ReadLine(cLine, &isEOF);
        CopyASCIItoUTF16(cLine, line);

        if (isEOF)
        {
            if (line.Find("URL=", PR_TRUE) == 0)
            {
                line.Cut(0, 4);
                rv = gRDFService->GetLiteral(line.get(), urlLiteral);
                break;
            }
            else if (line.Find("CDFURL=", PR_TRUE) == 0)
            {
                line.Cut(0, 7);
                rv = gRDFService->GetLiteral(line.get(), urlLiteral);
                break;
            }
            line.Truncate();
        }
    }

    return(rv);
}
#endif



nsresult
FileSystemDataSource::GetURL(nsIRDFResource *source, PRBool *isFavorite, nsIRDFLiteral** aResult)
{
    if (isFavorite) *isFavorite = PR_FALSE;

    nsresult        rv;
    nsCString       uri;
	
    rv = source->GetValueUTF8(uri);
    if (NS_FAILED(rv))
        return(rv);

    NS_ConvertUTF8toUTF16 url(uri);

#ifdef  XP_WIN
    // under Windows, if its an IE favorite, munge the URL
    if (ieFavoritesDir)
    {
        if (url.Find(ieFavoritesDir) == 0)
        {
            if (isFavorite) *isFavorite = PR_TRUE;
            rv = getIEFavoriteURL(source, url, aResult);
            return(rv);
        }
    }
#endif

#ifdef  XP_BEOS
    // under BEOS, try and get the "META:url" attribute
    if (netPositiveDir)
    {
        if (strstr(uri.get(), netPositiveDir) != 0)
        {
            if (isFavorite) *isFavorite = PR_TRUE;
            rv = getNetPositiveURL(source, url, aResult);
            return(rv);
        }
    }
#endif

    // if we fall through to here, its not any type of bookmark
    // stored in the platform native file system, so just set the URL

    gRDFService->GetLiteral(url.get(), aResult);

    return(NS_OK);
}



#ifdef  XP_BEOS

nsresult
FileSystemDataSource::getNetPositiveURL(nsIRDFResource *source, nsString aFileURL, nsIRDFLiteral **urlLiteral)
{
    nsresult        rv = NS_RDF_NO_VALUE;

    *urlLiteral = nsnull;


    nsCOMPtr<nsIFile> f;
    NS_GetFileFromURLSpec(NS_ConvertUCS2toUTF8(aFileURL), getter_AddRefs(f)); 



    nsXPIDLCString nativePath;
    f->GetNativePath(nativePath);

    PRBool value;
    if (NS_SUCCEEDED(f->IsFile(&value) && value))
    {
        if (nativePath)
        {
            BFile   bf(nativePath.get(), B_READ_ONLY);
            if (bf.InitCheck() == B_OK)
            {
                char        beURLattr[4096];
                ssize_t     len;

                if ((len = bf.ReadAttr("META:url", B_STRING_TYPE,
                    0, beURLattr, sizeof(beURLattr)-1)) > 0)
                {
                    beURLattr[len] = '\0';
                    nsAutoString    bookmarkURL;
                    CopyUTF8toUTF16(beURLattr, bookmarkURL);
                    rv = gRDFService->GetLiteral(bookmarkURL.get(),
                        urlLiteral);
                }
            }
        }
    }
    return(rv);
}

#endif
