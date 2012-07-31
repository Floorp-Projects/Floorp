/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  Implementation for a file system RDF data store.
 */

#include "nsFileSystemDataSource.h"

#include <ctype.h> // for toupper()
#include <stdio.h>
#include "nsIEnumerator.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFObserver.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsRDFCID.h"
#include "rdfutil.h"
#include "plhash.h"
#include "plstr.h"
#include "prlong.h"
#include "prlog.h"
#include "prmem.h"
#include "prprf.h"
#include "prio.h"
#include "rdf.h"
#include "nsEnumeratorUtils.h"
#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIFile.h"
#include "nsEscape.h"
#include "nsCRTGlue.h"
#include "nsAutoPtr.h"

#ifdef XP_WIN
#include "windef.h"
#include "winbase.h"
#include "nsILineInputStream.h"
#include "nsDirectoryServiceDefs.h"
#endif

#ifdef XP_OS2
#define INCL_DOSFILEMGR
#include <os2.h>
#endif

#define NS_MOZICON_SCHEME           "moz-icon:"

static const char kFileProtocol[]         = "file://";

bool
FileSystemDataSource::isFileURI(nsIRDFResource *r)
{
    bool        isFileURIFlag = false;
    const char  *uri = nullptr;
    
    r->GetValueConst(&uri);
    if ((uri) && (!strncmp(uri, kFileProtocol, sizeof(kFileProtocol) - 1)))
    {
        // XXX HACK HACK HACK
        if (!strchr(uri, '#'))
        {
            isFileURIFlag = true;
        }
    }
    return(isFileURIFlag);
}



bool
FileSystemDataSource::isDirURI(nsIRDFResource* source)
{
    nsresult    rv;
    const char  *uri = nullptr;

    rv = source->GetValueConst(&uri);
    if (NS_FAILED(rv)) return(false);

    nsCOMPtr<nsIFile> aDir;

    rv = NS_GetFileFromURLSpec(nsDependentCString(uri), getter_AddRefs(aDir));
    if (NS_FAILED(rv)) return(false);

    bool isDirFlag = false;

    rv = aDir->IsDirectory(&isDirFlag);
    if (NS_FAILED(rv)) return(false);

    return(isDirFlag);
}


nsresult
FileSystemDataSource::Init()
{
    nsresult rv;

    mRDFService = do_GetService("@mozilla.org/rdf/rdf-service;1");
    NS_ENSURE_TRUE(mRDFService, NS_ERROR_FAILURE);

    rv =  mRDFService->GetResource(NS_LITERAL_CSTRING("NC:FilesRoot"),
                                   getter_AddRefs(mNC_FileSystemRoot));
    nsresult tmp = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "child"),
                                   getter_AddRefs(mNC_Child));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "Name"),
                                   getter_AddRefs(mNC_Name));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "URL"),
                                   getter_AddRefs(mNC_URL));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "Icon"),
                                   getter_AddRefs(mNC_Icon));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "Content-Length"),
                                   getter_AddRefs(mNC_Length));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "IsDirectory"),
                                   getter_AddRefs(mNC_IsDirectory));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = mRDFService->GetResource(NS_LITERAL_CSTRING(WEB_NAMESPACE_URI "LastModifiedDate"),
                                   getter_AddRefs(mWEB_LastMod));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "FileSystemObject"),
                                   getter_AddRefs(mNC_FileSystemObject));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "pulse"),
                                   getter_AddRefs(mNC_pulse));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = mRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "instanceOf"),
                                   getter_AddRefs(mRDF_InstanceOf));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = mRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "type"),
                                   getter_AddRefs(mRDF_type));

    static const PRUnichar kTrue[] = {'t','r','u','e','\0'};
    static const PRUnichar kFalse[] = {'f','a','l','s','e','\0'};

    tmp = mRDFService->GetLiteral(kTrue, getter_AddRefs(mLiteralTrue));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    tmp = mRDFService->GetLiteral(kFalse, getter_AddRefs(mLiteralFalse));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

#ifdef USE_NC_EXTENSION
    rv = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "extension"),
                                  getter_AddRefs(mNC_extension));
    NS_ENSURE_SUCCESS(rv, rv);
#endif

#ifdef XP_WIN
    rv =  mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "IEFavorite"),
                                  getter_AddRefs(mNC_IEFavoriteObject));
    tmp = mRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "IEFavoriteFolder"),
                                   getter_AddRefs(mNC_IEFavoriteFolder));
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    nsCOMPtr<nsIFile> file;
    NS_GetSpecialDirectory(NS_WIN_FAVORITES_DIR, getter_AddRefs(file));
    if (file)
    {
        nsCOMPtr<nsIURI> furi;
        NS_NewFileURI(getter_AddRefs(furi), file);
        NS_ENSURE_TRUE(furi, NS_ERROR_FAILURE);

        file->GetNativePath(ieFavoritesDir);
    }
#endif

    return NS_OK;
}

//static
nsresult
FileSystemDataSource::Create(nsISupports* aOuter, const nsIID& aIID, void **aResult)
{
    NS_ENSURE_NO_AGGREGATION(aOuter);

    nsRefPtr<FileSystemDataSource> self = new FileSystemDataSource();
    if (!self)
        return NS_ERROR_OUT_OF_MEMORY;
     
    nsresult rv = self->Init();
    NS_ENSURE_SUCCESS(rv, rv);

    return self->QueryInterface(aIID, aResult);
}

NS_IMPL_CYCLE_COLLECTION_0(FileSystemDataSource) 
NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemDataSource)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystemDataSource)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemDataSource)
    NS_INTERFACE_MAP_ENTRY(nsIRDFDataSource)
    NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
FileSystemDataSource::GetURI(char **uri)
{
    NS_PRECONDITION(uri != nullptr, "null ptr");
    if (! uri)
        return NS_ERROR_NULL_POINTER;

    if ((*uri = NS_strdup("rdf:files")) == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::GetSource(nsIRDFResource* property,
                                nsIRDFNode* target,
                                bool tv,
                                nsIRDFResource** source /* out */)
{
    NS_PRECONDITION(property != nullptr, "null ptr");
    if (! property)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(target != nullptr, "null ptr");
    if (! target)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(source != nullptr, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    *source = nullptr;
    return NS_RDF_NO_VALUE;
}



NS_IMETHODIMP
FileSystemDataSource::GetSources(nsIRDFResource *property,
                                 nsIRDFNode *target,
                                 bool tv,
                                 nsISimpleEnumerator **sources /* out */)
{
//  NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::GetTarget(nsIRDFResource *source,
                                nsIRDFResource *property,
                                bool tv,
                                nsIRDFNode **target /* out */)
{
    NS_PRECONDITION(source != nullptr, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(property != nullptr, "null ptr");
    if (! property)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(target != nullptr, "null ptr");
    if (! target)
        return NS_ERROR_NULL_POINTER;

    *target = nullptr;

    nsresult        rv = NS_RDF_NO_VALUE;

    // we only have positive assertions in the file system data source.
    if (! tv)
        return NS_RDF_NO_VALUE;

    if (source == mNC_FileSystemRoot)
    {
        if (property == mNC_pulse)
        {
            nsIRDFLiteral   *pulseLiteral;
            mRDFService->GetLiteral(NS_LITERAL_STRING("12").get(), &pulseLiteral);
            *target = pulseLiteral;
            return NS_OK;
        }
    }
    else if (isFileURI(source))
    {
        if (property == mNC_Name)
        {
            nsCOMPtr<nsIRDFLiteral> name;
            rv = GetName(source, getter_AddRefs(name));
            if (NS_FAILED(rv)) return(rv);
            if (!name)  rv = NS_RDF_NO_VALUE;
            if (rv == NS_RDF_NO_VALUE)  return(rv);
            return name->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
        }
        else if (property == mNC_URL)
        {
            nsCOMPtr<nsIRDFLiteral> url;
            rv = GetURL(source, nullptr, getter_AddRefs(url));
            if (NS_FAILED(rv)) return(rv);
            if (!url)   rv = NS_RDF_NO_VALUE;
            if (rv == NS_RDF_NO_VALUE)  return(rv);

            return url->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
        }
        else if (property == mNC_Icon)
        {
            nsCOMPtr<nsIRDFLiteral> url;
            bool isFavorite = false;
            rv = GetURL(source, &isFavorite, getter_AddRefs(url));
            if (NS_FAILED(rv)) return(rv);
            if (isFavorite || !url) rv = NS_RDF_NO_VALUE;
            if (rv == NS_RDF_NO_VALUE)  return(rv);
            
            const PRUnichar *uni = nullptr;
            url->GetValueConst(&uni);
            if (!uni)   return(NS_RDF_NO_VALUE);
            nsAutoString    urlStr;
            urlStr.Assign(NS_LITERAL_STRING(NS_MOZICON_SCHEME).get());
            urlStr.Append(uni);

            rv = mRDFService->GetLiteral(urlStr.get(), getter_AddRefs(url));
            if (NS_FAILED(rv) || !url)    return(NS_RDF_NO_VALUE);
            return url->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
        }
        else if (property == mNC_Length)
        {
            nsCOMPtr<nsIRDFInt> fileSize;
            rv = GetFileSize(source, getter_AddRefs(fileSize));
            if (NS_FAILED(rv)) return(rv);
            if (!fileSize)  rv = NS_RDF_NO_VALUE;
            if (rv == NS_RDF_NO_VALUE)  return(rv);

            return fileSize->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
        }
        else  if (property == mNC_IsDirectory)
        {
            *target = (isDirURI(source)) ? mLiteralTrue : mLiteralFalse;
            NS_ADDREF(*target);
            return NS_OK;
        }
        else if (property == mWEB_LastMod)
        {
            nsCOMPtr<nsIRDFDate> lastMod;
            rv = GetLastMod(source, getter_AddRefs(lastMod));
            if (NS_FAILED(rv)) return(rv);
            if (!lastMod)   rv = NS_RDF_NO_VALUE;
            if (rv == NS_RDF_NO_VALUE)  return(rv);

            return lastMod->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
        }
        else if (property == mRDF_type)
        {
            nsCString type;
            rv = mNC_FileSystemObject->GetValueUTF8(type);
            if (NS_FAILED(rv)) return(rv);

#ifdef  XP_WIN
            // under Windows, if its an IE favorite, return that type
            if (!ieFavoritesDir.IsEmpty())
            {
                nsCString uri;
                rv = source->GetValueUTF8(uri);
                if (NS_FAILED(rv)) return(rv);

                NS_ConvertUTF8toUTF16 theURI(uri);

                if (theURI.Find(ieFavoritesDir) == 0)
                {
                    if (theURI[theURI.Length() - 1] == '/')
                    {
                        rv = mNC_IEFavoriteFolder->GetValueUTF8(type);
                    }
                    else
                    {
                        rv = mNC_IEFavoriteObject->GetValueUTF8(type);
                    }
                    if (NS_FAILED(rv)) return(rv);
                }
            }
#endif

            NS_ConvertUTF8toUTF16 url(type);
            nsCOMPtr<nsIRDFLiteral> literal;
            mRDFService->GetLiteral(url.get(), getter_AddRefs(literal));
            rv = literal->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
            return(rv);
        }
        else if (property == mNC_pulse)
        {
            nsCOMPtr<nsIRDFLiteral> pulseLiteral;
            mRDFService->GetLiteral(NS_LITERAL_STRING("12").get(), getter_AddRefs(pulseLiteral));
            rv = pulseLiteral->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
            return(rv);
        }
        else if (property == mNC_Child)
        {
            // Oh this is evil. Somebody kill me now.
            nsCOMPtr<nsISimpleEnumerator> children;
            rv = GetFolderList(source, false, true, getter_AddRefs(children));
            if (NS_FAILED(rv) || (rv == NS_RDF_NO_VALUE)) return(rv);

            bool hasMore;
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
        else if (property == mNC_extension)
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
                bool tv,
                nsISimpleEnumerator **targets /* out */)
{
    NS_PRECONDITION(source != nullptr, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(property != nullptr, "null ptr");
    if (! property)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(targets != nullptr, "null ptr");
    if (! targets)
        return NS_ERROR_NULL_POINTER;

    *targets = nullptr;

    // we only have positive assertions in the file system data source.
    if (! tv)
        return NS_RDF_NO_VALUE;

    nsresult rv;

    if (source == mNC_FileSystemRoot)
    {
        if (property == mNC_Child)
        {
            return GetVolumeList(targets);
        }
        else if (property == mNC_pulse)
        {
            nsCOMPtr<nsIRDFLiteral> pulseLiteral;
            mRDFService->GetLiteral(NS_LITERAL_STRING("12").get(),
                                    getter_AddRefs(pulseLiteral));
            return NS_NewSingletonEnumerator(targets, pulseLiteral);
        }
    }
    else if (isFileURI(source))
    {
        if (property == mNC_Child)
        {
            return GetFolderList(source, false, false, targets);
        }
        else if (property == mNC_Name)
        {
            nsCOMPtr<nsIRDFLiteral> name;
            rv = GetName(source, getter_AddRefs(name));
            if (NS_FAILED(rv)) return rv;

            return NS_NewSingletonEnumerator(targets, name);
        }
        else if (property == mNC_URL)
        {
            nsCOMPtr<nsIRDFLiteral> url;
            rv = GetURL(source, nullptr, getter_AddRefs(url));
            if (NS_FAILED(rv)) return rv;

            return NS_NewSingletonEnumerator(targets, url);
        }
        else if (property == mRDF_type)
        {
            nsCString uri;
            rv = mNC_FileSystemObject->GetValueUTF8(uri);
            if (NS_FAILED(rv)) return rv;

            NS_ConvertUTF8toUTF16 url(uri);

            nsCOMPtr<nsIRDFLiteral> literal;
            rv = mRDFService->GetLiteral(url.get(), getter_AddRefs(literal));
            if (NS_FAILED(rv)) return rv;

            return NS_NewSingletonEnumerator(targets, literal);
        }
        else if (property == mNC_pulse)
        {
            nsCOMPtr<nsIRDFLiteral> pulseLiteral;
            rv = mRDFService->GetLiteral(NS_LITERAL_STRING("12").get(),
                getter_AddRefs(pulseLiteral));
            if (NS_FAILED(rv)) return rv;

            return NS_NewSingletonEnumerator(targets, pulseLiteral);
        }
    }

    return NS_NewEmptyEnumerator(targets);
}



NS_IMETHODIMP
FileSystemDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       bool tv)
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
                             bool tv,
                             bool *hasAssertion /* out */)
{
    NS_PRECONDITION(source != nullptr, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(property != nullptr, "null ptr");
    if (! property)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(target != nullptr, "null ptr");
    if (! target)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(hasAssertion != nullptr, "null ptr");
    if (! hasAssertion)
        return NS_ERROR_NULL_POINTER;

    // we only have positive assertions in the file system data source.
    *hasAssertion = false;

    if (! tv) {
        return NS_OK;
    }

    if ((source == mNC_FileSystemRoot) || isFileURI(source))
    {
        if (property == mRDF_type)
        {
            nsCOMPtr<nsIRDFResource> resource( do_QueryInterface(target) );
            if (resource.get() == mRDF_type)
            {
                *hasAssertion = true;
            }
        }
#ifdef USE_NC_EXTENSION
        else if (property == mNC_extension)
        {
            // Cheat just a little here by making dirs always match
            if (isDirURI(source))
            {
                *hasAssertion = true;
            }
            else
            {
                nsCOMPtr<nsIRDFLiteral> extension;
                GetExtension(source, getter_AddRefs(extension));
                if (extension.get() == target)
                {
                    *hasAssertion = true;
                }
            }
        }
#endif
        else if (property == mNC_IsDirectory)
        {
            bool isDir = isDirURI(source);
            bool isEqual = false;
            target->EqualsNode(mLiteralTrue, &isEqual);
            if (isEqual)
            {
                *hasAssertion = isDir;
            }
            else
            {
                target->EqualsNode(mLiteralFalse, &isEqual);
                if (isEqual)
                    *hasAssertion = !isDir;
            }
        }
    }

    return NS_OK;
}



NS_IMETHODIMP 
FileSystemDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, bool *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP 
FileSystemDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, bool *result)
{
    *result = false;

    if (aSource == mNC_FileSystemRoot)
    {
        *result = (aArc == mNC_Child || aArc == mNC_pulse);
    }
    else if (isFileURI(aSource))
    {
        if (aArc == mNC_pulse)
        {
            *result = true;
        }
        else if (isDirURI(aSource))
        {
#ifdef  XP_WIN
            *result = isValidFolder(aSource);
#else
            *result = true;
#endif
        }
        else if (aArc == mNC_pulse || aArc == mNC_Name || aArc == mNC_Icon ||
                 aArc == mNC_URL || aArc == mNC_Length || aArc == mWEB_LastMod ||
                 aArc == mNC_FileSystemObject || aArc == mRDF_InstanceOf ||
                 aArc == mRDF_type)
        {
            *result = true;
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
    NS_PRECONDITION(source != nullptr, "null ptr");
    if (! source)
    return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(labels != nullptr, "null ptr");
    if (! labels)
    return NS_ERROR_NULL_POINTER;

    nsresult rv;

    if (source == mNC_FileSystemRoot)
    {
        nsCOMPtr<nsISupportsArray> array;
        rv = NS_NewISupportsArray(getter_AddRefs(array));
        if (NS_FAILED(rv)) return rv;

        array->AppendElement(mNC_Child);
        array->AppendElement(mNC_pulse);

        return NS_NewArrayEnumerator(labels, array);
    }
    else if (isFileURI(source))
    {
        nsCOMPtr<nsISupportsArray> array;
        rv = NS_NewISupportsArray(getter_AddRefs(array));
        if (NS_FAILED(rv)) return rv;

        if (isDirURI(source))
        {
#ifdef  XP_WIN
            if (isValidFolder(source))
            {
                array->AppendElement(mNC_Child);
            }
#else
            array->AppendElement(mNC_Child);
#endif
            array->AppendElement(mNC_pulse);
        }

        return NS_NewArrayEnumerator(labels, array);
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
    return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::RemoveObserver(nsIRDFObserver *n)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
                                       bool* aResult)
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
FileSystemDataSource::GetVolumeList(nsISimpleEnumerator** aResult)
{
    nsresult rv;
    nsCOMPtr<nsISupportsArray> volumes;

    rv = NS_NewISupportsArray(getter_AddRefs(volumes));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> vol;

#ifdef XP_WIN

    PRInt32         driveType;
    PRUnichar       drive[32];
    PRInt32         volNum;
    char            *url;

    for (volNum = 0; volNum < 26; volNum++)
    {
        swprintf( drive, L"%c:\\", volNum + (PRUnichar)'A');

        driveType = GetDriveTypeW(drive);
        if (driveType != DRIVE_UNKNOWN && driveType != DRIVE_NO_ROOT_DIR)
        {
            if (nullptr != (url = PR_smprintf("file:///%c|/", volNum + 'A')))
            {
                rv = mRDFService->GetResource(nsDependentCString(url),
                                              getter_AddRefs(vol));
                PR_Free(url);

                if (NS_FAILED(rv)) return rv;
                volumes->AppendElement(vol);
            }
        }
    }
#endif

#ifdef XP_UNIX
    mRDFService->GetResource(NS_LITERAL_CSTRING("file:///"), getter_AddRefs(vol));
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
            if (nullptr != (url = PR_smprintf("file:///%c|/", volNum + 'A')))
            {
                rv = mRDFService->GetResource(nsDependentCString(url), getter_AddRefs(vol));
                PR_Free(url);

                if (NS_FAILED(rv)) return rv;
                volumes->AppendElement(vol);
            }
        }

    }
#endif

    return NS_NewArrayEnumerator(aResult, volumes);
}



#ifdef  XP_WIN
bool
FileSystemDataSource::isValidFolder(nsIRDFResource *source)
{
    bool    isValid = true;
    if (ieFavoritesDir.IsEmpty())    return(isValid);

    nsresult        rv;
    nsCString       uri;
    rv = source->GetValueUTF8(uri);
    if (NS_FAILED(rv)) return(isValid);

    NS_ConvertUTF8toUTF16 theURI(uri);
    if (theURI.Find(ieFavoritesDir) == 0)
    {
        isValid = false;

        nsCOMPtr<nsISimpleEnumerator>   folderEnum;
        if (NS_SUCCEEDED(rv = GetFolderList(source, true, false, getter_AddRefs(folderEnum))))
        {
            bool        hasAny = false, hasMore;
            while (NS_SUCCEEDED(folderEnum->HasMoreElements(&hasMore)) &&
                   hasMore)
            {
                hasAny = true;

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
                    isValid = true;
                    break;
                }
            }
            if (!hasAny) isValid = true;
        }
    }
    return(isValid);
}
#endif



nsresult
FileSystemDataSource::GetFolderList(nsIRDFResource *source, bool allowHidden,
                bool onlyFirst, nsISimpleEnumerator** aResult)
{
    if (!isDirURI(source))
        return(NS_RDF_NO_VALUE);

    nsresult                    rv;
    nsCOMPtr<nsISupportsArray>  nameArray;

    rv = NS_NewISupportsArray(getter_AddRefs(nameArray));
    if (NS_FAILED(rv))
        return(rv);

    const char      *parentURI = nullptr;
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
        return NS_OK;

    nsCOMPtr<nsIFile>   aDir;
    if (NS_FAILED(rv = fileURL->GetFile(getter_AddRefs(aDir))))
        return(rv);

    // ensure that we DO NOT resolve aliases
    aDir->SetFollowLinks(false);

    nsCOMPtr<nsISimpleEnumerator>   dirContents;
    if (NS_FAILED(rv = aDir->GetDirectoryEntries(getter_AddRefs(dirContents))))
        return(rv);
    if (!dirContents)
        return(NS_ERROR_UNEXPECTED);

    bool            hasMore;
    while(NS_SUCCEEDED(rv = dirContents->HasMoreElements(&hasMore)) &&
          hasMore)
    {
        nsCOMPtr<nsISupports>   isupports;
        if (NS_FAILED(rv = dirContents->GetNext(getter_AddRefs(isupports))))
            break;

        nsCOMPtr<nsIFile>   aFile = do_QueryInterface(isupports);
        if (!aFile)
            break;

        if (!allowHidden)
        {
            bool            hiddenFlag = false;
            if (NS_FAILED(rv = aFile->IsHidden(&hiddenFlag)))
                break;
            if (hiddenFlag)
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

        char    *escLeafStr = nsEscape(NS_ConvertUTF16toUTF8(leafStr).get(), url_Path);
        leafStr.Truncate();

        if (!escLeafStr)
            continue;
  
        nsCAutoString           leaf(escLeafStr);
        NS_Free(escLeafStr);
        escLeafStr = nullptr;

        // using nsEscape() [above] doesn't escape slashes, so do that by hand
        PRInt32         aOffset;
        while ((aOffset = leaf.FindChar('/')) >= 0)
        {
            leaf.Cut((PRUint32)aOffset, 1);
            leaf.Insert("%2F", (PRUint32)aOffset);
        }

        // append the encoded name
        fullURI.Append(leaf);

        bool            dirFlag = false;
        rv = aFile->IsDirectory(&dirFlag);
        if (NS_SUCCEEDED(rv) && dirFlag)
        {
            fullURI.Append('/');
        }

        nsCOMPtr<nsIRDFResource>    fileRes;
        mRDFService->GetResource(fullURI, getter_AddRefs(fileRes));

        nameArray->AppendElement(fileRes);

        if (onlyFirst)
            break;
    }

    return NS_NewArrayEnumerator(aResult, nameArray);
}

nsresult
FileSystemDataSource::GetLastMod(nsIRDFResource *source, nsIRDFDate **aResult)
{
    *aResult = nullptr;

    nsresult        rv;
    const char      *uri = nullptr;

    rv = source->GetValueConst(&uri);
    if (NS_FAILED(rv)) return(rv);
    if (!uri)
        return(NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIURI>    aIURI;
    if (NS_FAILED(rv = NS_NewURI(getter_AddRefs(aIURI), nsDependentCString(uri))))
        return(rv);

    nsCOMPtr<nsIFileURL>    fileURL = do_QueryInterface(aIURI);
    if (!fileURL)
        return NS_OK;

    nsCOMPtr<nsIFile>   aFile;
    if (NS_FAILED(rv = fileURL->GetFile(getter_AddRefs(aFile))))
        return(rv);
    if (!aFile)
        return(NS_ERROR_UNEXPECTED);

    // ensure that we DO NOT resolve aliases
    aFile->SetFollowLinks(false);

    PRInt64 lastModDate;
    if (NS_FAILED(rv = aFile->GetLastModifiedTime(&lastModDate)))
        return(rv);

    // convert from milliseconds to seconds
    PRTime      temp64, thousand;
    LL_I2L(thousand, PR_MSEC_PER_SEC);
    LL_MUL(temp64, lastModDate, thousand);

    mRDFService->GetDateLiteral(temp64, aResult);

    return(NS_OK);
}



nsresult
FileSystemDataSource::GetFileSize(nsIRDFResource *source, nsIRDFInt **aResult)
{
    *aResult = nullptr;

    nsresult        rv;
    const char      *uri = nullptr;

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
        return NS_OK;

    nsCOMPtr<nsIFile>   aFile;
    if (NS_FAILED(rv = fileURL->GetFile(getter_AddRefs(aFile))))
        return(rv);
    if (!aFile)
        return(NS_ERROR_UNEXPECTED);

    // ensure that we DO NOT resolve aliases
    aFile->SetFollowLinks(false);

    // don't do anything with directories
    bool    isDir = false;
    if (NS_FAILED(rv = aFile->IsDirectory(&isDir)))
        return(rv);
    if (isDir)
        return(NS_RDF_NO_VALUE);

    PRInt64     aFileSize64;
    if (NS_FAILED(rv = aFile->GetFileSize(&aFileSize64)))
        return(rv);

    // convert 64bits to 32bits
    PRInt32     aFileSize32 = 0;
    LL_L2I(aFileSize32, aFileSize64);

    mRDFService->GetIntLiteral(aFileSize32, aResult);

    return(NS_OK);
}



nsresult
FileSystemDataSource::GetName(nsIRDFResource *source, nsIRDFLiteral **aResult)
{
    nsresult        rv;
    const char      *uri = nullptr;

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
        return NS_OK;

    nsCOMPtr<nsIFile>   aFile;
    if (NS_FAILED(rv = fileURL->GetFile(getter_AddRefs(aFile))))
        return(rv);
    if (!aFile)
        return(NS_ERROR_UNEXPECTED);

    // ensure that we DO NOT resolve aliases
    aFile->SetFollowLinks(false);

    nsAutoString name;
    if (NS_FAILED(rv = aFile->GetLeafName(name)))
        return(rv);
    if (name.IsEmpty())
        return(NS_ERROR_UNEXPECTED);

#ifdef  XP_WIN
    // special hack for IE favorites under Windows; strip off the
    // trailing ".url" or ".lnk" at the end of IE favorites names
    PRInt32 nameLen = name.Length();
    if ((strncmp(uri, ieFavoritesDir.get(), ieFavoritesDir.Length()) == 0) && (nameLen > 4))
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

    mRDFService->GetLiteral(name.get(), aResult);

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
        mRDFService->GetLiteral(EmptyString().get(), aResult);
    }
    else
    {
        nsAutoString extension;
        filename.Right(extension, (filename.Length() - lastDot));
        mRDFService->GetLiteral(extension.get(), aResult);
    }

    return NS_OK;
}
#endif

#ifdef  XP_WIN
nsresult
FileSystemDataSource::getIEFavoriteURL(nsIRDFResource *source, nsString aFileURL, nsIRDFLiteral **urlLiteral)
{
    nsresult        rv = NS_OK;
    
    *urlLiteral = nullptr;

    nsCOMPtr<nsIFile> f;
    NS_GetFileFromURLSpec(NS_ConvertUTF16toUTF8(aFileURL), getter_AddRefs(f)); 

    bool value;

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
        bool    isEOF;
        rv = linereader->ReadLine(cLine, &isEOF);
        CopyASCIItoUTF16(cLine, line);

        if (isEOF)
        {
            if (line.Find("URL=", true) == 0)
            {
                line.Cut(0, 4);
                rv = mRDFService->GetLiteral(line.get(), urlLiteral);
                break;
            }
            else if (line.Find("CDFURL=", true) == 0)
            {
                line.Cut(0, 7);
                rv = mRDFService->GetLiteral(line.get(), urlLiteral);
                break;
            }
            line.Truncate();
        }
    }

    return(rv);
}
#endif



nsresult
FileSystemDataSource::GetURL(nsIRDFResource *source, bool *isFavorite, nsIRDFLiteral** aResult)
{
    if (isFavorite) *isFavorite = false;

    nsresult        rv;
    nsCString       uri;
	
    rv = source->GetValueUTF8(uri);
    if (NS_FAILED(rv))
        return(rv);

    NS_ConvertUTF8toUTF16 url(uri);

#ifdef  XP_WIN
    // under Windows, if its an IE favorite, munge the URL
    if (!ieFavoritesDir.IsEmpty())
    {
        if (url.Find(ieFavoritesDir) == 0)
        {
            if (isFavorite) *isFavorite = true;
            rv = getIEFavoriteURL(source, url, aResult);
            return(rv);
        }
    }
#endif

    // if we fall through to here, its not any type of bookmark
    // stored in the platform native file system, so just set the URL

    mRDFService->GetLiteral(url.get(), aResult);

    return(NS_OK);
}
