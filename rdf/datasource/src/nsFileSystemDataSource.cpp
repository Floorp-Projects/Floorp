/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
#include "nsIRDFResourceFactory.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
#include "nsXPIDLString.h"
#include "nsRDFCID.h"
#include "rdfutil.h"
#include "nsIRDFService.h"
#include "xp_core.h"
#include "plhash.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "prio.h"
#include "rdf.h"
#include "nsFileSpec.h"
#include "nsIRDFFileSystem.h"
#include "nsFileSpec.h"
#include "nsEnumeratorUtils.h"


#ifdef	XP_WIN
#include "windef.h"
#include "winbase.h"
#endif



static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);

static const char kURINC_FileSystemRoot[] = "NC:FilesRoot";

class FileSystemDataSource : public nsIRDFFileSystemDataSource
{
private:
	char			*mURI;
	nsVoidArray		*mObservers;

    static PRInt32 gRefCnt;

    // pseudo-constants
	static nsIRDFResource		*kNC_FileSystemRoot;
	static nsIRDFResource		*kNC_Child;
	static nsIRDFResource		*kNC_Name;
	static nsIRDFResource		*kNC_URL;
	static nsIRDFResource		*kNC_FileSystemObject;
	static nsIRDFResource		*kNC_pulse;
	static nsIRDFResource		*kRDF_InstanceOf;
	static nsIRDFResource		*kRDF_type;

public:

	NS_DECL_ISUPPORTS

			FileSystemDataSource(void);
	virtual		~FileSystemDataSource(void);

	// nsIRDFDataSource methods

	NS_IMETHOD	Init(const char *uri);
	NS_IMETHOD	GetURI(char **uri);
	NS_IMETHOD	GetSource(nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv,
				nsIRDFResource **source /* out */);
	NS_IMETHOD	GetSources(nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv,
				nsISimpleEnumerator **sources /* out */);
	NS_IMETHOD	GetTarget(nsIRDFResource *source,
				nsIRDFResource *property,
				PRBool tv,
				nsIRDFNode **target /* out */);
	NS_IMETHOD	GetTargets(nsIRDFResource *source,
				nsIRDFResource *property,
				PRBool tv,
				nsISimpleEnumerator **targets /* out */);
	NS_IMETHOD	Assert(nsIRDFResource *source,
				nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv);
	NS_IMETHOD	Unassert(nsIRDFResource *source,
				nsIRDFResource *property,
				nsIRDFNode *target);
	NS_IMETHOD	HasAssertion(nsIRDFResource *source,
				nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv,
				PRBool *hasAssertion /* out */);
	NS_IMETHOD	ArcLabelsIn(nsIRDFNode *node,
				nsISimpleEnumerator **labels /* out */);
	NS_IMETHOD	ArcLabelsOut(nsIRDFResource *source,
				nsISimpleEnumerator **labels /* out */);
	NS_IMETHOD	GetAllResources(nsISimpleEnumerator** aResult);
	NS_IMETHOD	AddObserver(nsIRDFObserver *n);
	NS_IMETHOD	RemoveObserver(nsIRDFObserver *n);
	NS_IMETHOD	Flush();
    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator/*<nsIRDFResource>*/** commands);

    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult);

    NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray/*<nsIRDFResource>*/* aArguments);


    // helper methods
    static PRBool isFileURI(nsIRDFResource* aResource);

    static nsresult GetVolumeList(nsISimpleEnumerator **aResult);
    static nsresult GetFolderList(nsIRDFResource *source, nsISimpleEnumerator **aResult);
    static nsresult GetName(nsIRDFResource *source, nsIRDFLiteral** aResult);
    static nsresult GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult);
    static PRBool   isVisible(const nsNativeFileSpec& file);

};



static	nsIRDFService		*gRDFService = nsnull;
static	FileSystemDataSource	*gFileSystemDataSource = nsnull;

PRInt32 FileSystemDataSource::gRefCnt;

nsIRDFResource		*FileSystemDataSource::kNC_FileSystemRoot;
nsIRDFResource		*FileSystemDataSource::kNC_Child;
nsIRDFResource		*FileSystemDataSource::kNC_Name;
nsIRDFResource		*FileSystemDataSource::kNC_URL;
nsIRDFResource		*FileSystemDataSource::kNC_FileSystemObject;
nsIRDFResource		*FileSystemDataSource::kNC_pulse;
nsIRDFResource		*FileSystemDataSource::kRDF_InstanceOf;
nsIRDFResource		*FileSystemDataSource::kRDF_type;




PRBool
FileSystemDataSource::isFileURI(nsIRDFResource *r)
{
	PRBool		isFileURI = PR_FALSE;
        nsXPIDLCString uri;
	
	r->GetValue( getter_Copies(uri) );
	if (!strncmp(uri, "file://", 7))
	{
		// XXX HACK HACK HACK
		if (!strchr(uri, '#'))
		{
			isFileURI = PR_TRUE;
		}
	}
	return(isFileURI);
}



FileSystemDataSource::FileSystemDataSource(void)
	: mURI(nsnull),
	  mObservers(nsnull)
{
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   nsIRDFService::GetIID(),
                                                   (nsISupports**) &gRDFService);

        PR_ASSERT(NS_SUCCEEDED(rv));

	gRDFService->GetResource(kURINC_FileSystemRoot,               &kNC_FileSystemRoot);
	gRDFService->GetResource(NC_NAMESPACE_URI "child",            &kNC_Child);
	gRDFService->GetResource(NC_NAMESPACE_URI "Name",             &kNC_Name);
	gRDFService->GetResource(NC_NAMESPACE_URI "URL",              &kNC_URL);
	gRDFService->GetResource(NC_NAMESPACE_URI "FileSystemObject", &kNC_FileSystemObject);
	gRDFService->GetResource(NC_NAMESPACE_URI "pulse",            &kNC_pulse);

	gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf",      &kRDF_InstanceOf);
	gRDFService->GetResource(RDF_NAMESPACE_URI "type",            &kRDF_type);

        gFileSystemDataSource = this;
    }
}



FileSystemDataSource::~FileSystemDataSource (void)
{
    gRDFService->UnregisterDataSource(this);

    PL_strfree(mURI);
    if (nsnull != mObservers)
	{
            for (PRInt32 i = mObservers->Count(); i >= 0; --i)
		{
                    nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
                    NS_RELEASE(obs);
		}
            delete mObservers;
            mObservers = nsnull;
	}

    if (--gRefCnt == 0) {
        NS_RELEASE(kNC_FileSystemRoot);
        NS_RELEASE(kNC_Child);
        NS_RELEASE(kNC_Name);
        NS_RELEASE(kNC_URL);
	NS_RELEASE(kNC_FileSystemObject);
	NS_RELEASE(kNC_pulse);
        NS_RELEASE(kRDF_InstanceOf);
        NS_RELEASE(kRDF_type);

        gFileSystemDataSource = nsnull;
        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
        gRDFService = nsnull;
    }
}

NS_IMPL_ISUPPORTS(FileSystemDataSource, nsIRDFDataSource::GetIID());

NS_IMETHODIMP
FileSystemDataSource::Init(const char *uri)
{
	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;

	if ((mURI = PL_strdup(uri)) == nsnull)
		return rv;

	// register this as a named data source with the service manager
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this, PR_FALSE)))
		return rv;
	return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::GetURI(char **uri)
{
    if ((*uri = nsXPIDLCString::Copy(mURI)) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    else
        return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::GetSource(nsIRDFResource* property,
                                nsIRDFNode* target,
                                PRBool tv,
                                nsIRDFResource** source /* out */)
{
	return NS_RDF_NO_VALUE;
}



NS_IMETHODIMP
FileSystemDataSource::GetSources(nsIRDFResource *property,
                           nsIRDFNode *target,
			   PRBool tv,
                           nsISimpleEnumerator **sources /* out */)
{
	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::GetTarget(nsIRDFResource *source,
                                nsIRDFResource *property,
                                PRBool tv,
                                nsIRDFNode **target /* out */)
{
	nsresult		rv = NS_RDF_NO_VALUE;

	// we only have positive assertions in the file system data source.
	if (! tv)
		return NS_RDF_NO_VALUE;

	if (source == kNC_FileSystemRoot)
	{
		if (property == kNC_pulse)
		{
			nsAutoString	pulse("12");
			nsIRDFLiteral	*pulseLiteral;
			gRDFService->GetLiteral(pulse.GetUnicode(), &pulseLiteral);
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
                    if (NS_FAILED(rv)) return rv;

                    return name->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
		}
		else if (property == kNC_URL)
		{
                    nsCOMPtr<nsIRDFLiteral> url;
                    rv = GetURL(source, getter_AddRefs(url));
                    if (NS_FAILED(rv)) return rv;

                    return url->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
		}
		else if (property == kRDF_type)
		{
			nsXPIDLCString uri;
			rv = kNC_FileSystemObject->GetValue( getter_Copies(uri) );
                        if (NS_FAILED(rv)) return rv;

                        nsAutoString	url(uri);
                        nsIRDFLiteral	*literal;
                        gRDFService->GetLiteral(url.GetUnicode(), &literal);
                        rv = literal->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
                        NS_RELEASE(literal);
                        return rv;
		}
		else if (property == kNC_pulse)
		{
			nsAutoString	pulse("12");
			nsIRDFLiteral	*pulseLiteral;
			gRDFService->GetLiteral(pulse.GetUnicode(), &pulseLiteral);
                        rv = pulseLiteral->QueryInterface(nsIRDFNode::GetIID(), (void**) target);
                        NS_RELEASE(pulseLiteral);
                        return rv;
		}
	}

	return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
FileSystemDataSource::GetTargets(nsIRDFResource *source,
                           nsIRDFResource *property,
                           PRBool tv,
                           nsISimpleEnumerator **targets /* out */)
{
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
			nsAutoString	pulse("12");
			nsIRDFLiteral	*pulseLiteral;
			gRDFService->GetLiteral(pulse.GetUnicode(), &pulseLiteral);
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
			return GetFolderList(source, targets);
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
                    rv = GetURL(source, getter_AddRefs(url));
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
			nsXPIDLCString uri;
			rv = kNC_FileSystemObject->GetValue( getter_Copies(uri) );
                        if (NS_FAILED(rv)) return rv;

                        nsAutoString	url(uri);
                        nsIRDFLiteral	*literal;

                        rv = gRDFService->GetLiteral(url.GetUnicode(), &literal);
                        if (NS_FAILED(rv)) return rv;

                        nsISimpleEnumerator* result = new nsSingletonEnumerator(literal);
                        NS_RELEASE(literal);

                        if (! result)
                            return NS_ERROR_OUT_OF_MEMORY;

                        NS_ADDREF(result);
                        *targets = result;
                        return NS_OK;
		}
		else if (property == kNC_pulse)
		{
			nsAutoString	pulse("12");
			nsIRDFLiteral	*pulseLiteral;
			rv = gRDFService->GetLiteral(pulse.GetUnicode(), &pulseLiteral);
                        if (NS_FAILED(rv)) return rv;

                        nsISimpleEnumerator* result = new nsSingletonEnumerator(pulseLiteral);
                        NS_RELEASE(pulseLiteral);

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
//	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
//	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::HasAssertion(nsIRDFResource *source,
                             nsIRDFResource *property,
                             nsIRDFNode *target,
                             PRBool tv,
                             PRBool *hasAssertion /* out */)
{
	// we only have positive assertions in the file system data source.
	if (! tv) {
            *hasAssertion = PR_FALSE;
            return NS_OK;
        }

	if ((source == kNC_FileSystemRoot) || isFileURI(source))
	{
		if (property == kRDF_type)
		{
                    nsCOMPtr<nsIRDFResource> resource( do_QueryInterface(target) );
                    if (resource.get() == kRDF_type) {
                        *hasAssertion = PR_TRUE;
                        return NS_OK;
                    }
		}
	}

	*hasAssertion = PR_FALSE;
	return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsISimpleEnumerator ** labels /* out */)
{
	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::ArcLabelsOut(nsIRDFResource *source,
                             nsISimpleEnumerator **labels /* out */)
{
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

            nsXPIDLCString uri;
            rv = source->GetValue( getter_Copies(uri) );
            if (NS_FAILED(rv)) return rv;

            nsFileURL	fileURL(uri);
            nsFileSpec	fileSpec(fileURL);
            if (fileSpec.IsDirectory()) {
                array->AppendElement(kNC_Child);
                array->AppendElement(kNC_pulse);
            }

            array->AppendElement(kRDF_type);

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
	if (nsnull == mObservers)
	{
		if ((mObservers = new nsVoidArray()) == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
	}
	mObservers->AppendElement(n);
	return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::RemoveObserver(nsIRDFObserver *n)
{
	if (nsnull == mObservers)
		return NS_OK;
	mObservers->RemoveElement(n);
	return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::Flush()
{
	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileSystemDataSource::GetAllCommands(nsIRDFResource* source,
                                     nsIEnumerator/*<nsIRDFResource>*/** commands)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileSystemDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                       nsIRDFResource*   aCommand,
                                       nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                       PRBool* aResult)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileSystemDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
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

#ifdef	XP_MAC
	StrFileName     fname;
	HParamBlockRec	pb;
	for (int16 volNum = 1; ; volNum++)
	{
		pb.volumeParam.ioCompletion = NULL;
		pb.volumeParam.ioVolIndex = volNum;
		pb.volumeParam.ioNamePtr = (StringPtr)fname;
		if (PBHGetVInfo(&pb,FALSE) != noErr)
			break;
		nsFileSpec fss(pb.volumeParam.ioVRefNum, fsRtParID, fname);
		rv = gRDFService->GetResource(nsFileURL(fss).GetAsString(), getter_AddRefs(vol));
                if (NS_FAILED(rv)) return rv;

                volumes->AppendElement(vol);
	}
#endif

#ifdef	XP_WIN
	PRInt32			driveType;
	char			drive[32];
	PRInt32			volNum;
	char			*url;

	for (volNum = 0; volNum < 26; volNum++)
	{
		sprintf(drive, "%c:\\", volNum + 'A');
		driveType = GetDriveType(drive);
		if (driveType != DRIVE_UNKNOWN && driveType != DRIVE_NO_ROOT_DIR)
		{
			if (nsnull != (url = PR_smprintf("file:///%c|/", volNum + 'A')))
			{
                            rv = gRDFService->GetResource(url, getter_AddRefs(vol));
                            PR_Free(url);

                            if (NS_FAILED(rv)) return rv;
                            volumes->AppendElement(vol);
			}
		}
	}
#endif

#ifdef	XP_UNIX
	gRDFService->GetResource("file:///", getter_AddRefs(vol));
	volumes->AppendElement(vol);
#endif

        nsISimpleEnumerator* result = new nsArrayEnumerator(volumes);
        if (! result)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(result);
        *aResult = result;

	return NS_OK;
}


PRBool
FileSystemDataSource::isVisible(const nsNativeFileSpec& file)
{
	PRBool		isVisible = PR_TRUE;

#ifdef	XP_MAC

	FSSpec		fss = file;
	OSErr		err;

	CInfoPBRec	cInfo;

	cInfo.hFileInfo.ioCompletion = NULL;
	cInfo.hFileInfo.ioVRefNum = fss.vRefNum;
	cInfo.hFileInfo.ioDirID = fss.parID;
	cInfo.hFileInfo.ioNamePtr = (StringPtr)fss.name;
	cInfo.hFileInfo.ioFVersNum = 0;
	if (!(err = PBGetCatInfoSync(&cInfo)))
	{
		if (cInfo.hFileInfo.ioFlFndrInfo.fdFlags & kIsInvisible)
		{
			isVisible = PR_FALSE;
		}
	}

#else
	char		*basename = file.GetLeafName();
	if (nsnull != basename)
	{
		if ((!strcmp(basename, ".")) || (!strcmp(basename, "..")))
		{
			isVisible = PR_FALSE;
		}
		nsCRT::free(basename);
	}
#endif

	return(isVisible);
}



nsresult
FileSystemDataSource::GetFolderList(nsIRDFResource *source, nsISimpleEnumerator** aResult)
{
        nsresult rv;
        nsCOMPtr<nsISupportsArray> nameArray;
        rv = NS_NewISupportsArray(getter_AddRefs(nameArray));
        if (NS_FAILED(rv)) return rv;

        nsXPIDLCString uri;
	rv = source->GetValue( getter_Copies(uri) );
        if (NS_FAILED(rv)) return rv;

	nsFileURL 		parentDir(uri);
	nsNativeFileSpec 	nativeDir(parentDir);
	for (nsDirectoryIterator i(nativeDir); i.Exists(); i++)
	{
		const nsNativeFileSpec	nativeSpec = (const nsNativeFileSpec &)i;
		if (!isVisible(nativeSpec))	continue;
		nsFileURL		fileURL(nativeSpec);
		const char		*childURL = fileURL.GetAsString();
		if (childURL != nsnull)
		{
			nsCOMPtr<nsIRDFResource> file;
			gRDFService->GetResource(childURL, getter_AddRefs(file));
			nameArray->AppendElement(file);
		}
	}

        nsISimpleEnumerator* result = new nsArrayEnumerator(nameArray);
        if (! result)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(result);
        *aResult = result;

        return NS_OK;
}



nsresult
FileSystemDataSource::GetName(nsIRDFResource *source, nsIRDFLiteral **aResult)
{
        nsXPIDLCString uri;
	source->GetValue( getter_Copies(uri) );
	nsFileURL		url(uri);
	nsNativeFileSpec	native(url);
	char			*basename = native.GetLeafName();

	if (! basename)
            return NS_ERROR_OUT_OF_MEMORY;

        nsAutoString	name(basename);
        nsIRDFLiteral *literal;
        gRDFService->GetLiteral(name.GetUnicode(), &literal);
        *aResult = literal;
        nsCRT::free(basename);

	return NS_OK;
}



nsresult
FileSystemDataSource::GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult)
{
        nsXPIDLCString uri;
	source->GetValue( getter_Copies(uri) );
	nsAutoString	url(uri);

	nsIRDFLiteral	*literal;
	gRDFService->GetLiteral(url.GetUnicode(), &literal);
        *aResult = literal;
        return NS_OK;
}


