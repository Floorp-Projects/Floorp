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
#include "nsIRDFCursor.h"
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
#include "nsFileSystemDataSource.h"
#include "nsFileSpec.h"


#ifdef	XP_WIN
#include "windef.h"
#include "winbase.h"
#endif



static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kIRDFServiceIID,              NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,           NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFFileSystemDataSourceIID, NS_IRDFFILESYSTEMDATAOURCE_IID);
static NS_DEFINE_IID(kIRDFAssertionCursorIID,      NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,               NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,        NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFResourceIID,             NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFNodeIID,                 NS_IRDFNODE_IID);



static const char kURINC_FileSystemRoot[] = "NC:FilesRoot";

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, URL);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, FileSystemObject);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Seq);


static	nsIRDFService		*gRDFService = nsnull;
static	FileSystemDataSource	*gFileSystemDataSource = nsnull;

PRInt32 FileSystemDataSource::gRefCnt;

nsIRDFResource		*FileSystemDataSource::kNC_FileSystemRoot;
nsIRDFResource		*FileSystemDataSource::kNC_Child;
nsIRDFResource		*FileSystemDataSource::kNC_Name;
nsIRDFResource		*FileSystemDataSource::kNC_URL;
nsIRDFResource		*FileSystemDataSource::kNC_FileSystemObject;
nsIRDFResource		*FileSystemDataSource::kRDF_InstanceOf;
nsIRDFResource		*FileSystemDataSource::kRDF_type;



static PRBool
peq(nsIRDFResource* r1, nsIRDFResource* r2)
{
	PRBool		retVal=PR_FALSE, result;

	if (NS_SUCCEEDED(r1->EqualsResource(r2, &result)))
	{
		if (result)
		{
			retVal = PR_TRUE;
		}
	}
	return(retVal);
}


static PRBool
isFileURI(nsIRDFResource *r)
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
                                                   kIRDFServiceIID,
                                                   (nsISupports**) &gRDFService);

        PR_ASSERT(NS_SUCCEEDED(rv));

	gRDFService->GetResource(kURINC_FileSystemRoot, &kNC_FileSystemRoot);
	gRDFService->GetResource(kURINC_child, &kNC_Child);
	gRDFService->GetResource(kURINC_Name, &kNC_Name);
	gRDFService->GetResource(kURINC_URL, &kNC_URL);
	gRDFService->GetResource(kURINC_FileSystemObject, &kNC_FileSystemObject);

	gRDFService->GetResource(kURIRDF_instanceOf, &kRDF_InstanceOf);
	gRDFService->GetResource(kURIRDF_type, &kRDF_type);

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
        NS_RELEASE(kRDF_InstanceOf);
        NS_RELEASE(kRDF_type);

        gFileSystemDataSource = nsnull;
        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
        gRDFService = nsnull;
    }
}



// NS_IMPL_ISUPPORTS(FileSystemDataSource, kIRDFFileSystemDataSourceIID);
NS_IMPL_ISUPPORTS(FileSystemDataSource, kIRDFDataSourceIID);



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
	nsresult rv = NS_ERROR_RDF_NO_VALUE;
	return rv;
}



NS_IMETHODIMP
FileSystemDataSource::GetSources(nsIRDFResource *property,
                           nsIRDFNode *target,
			   PRBool tv,
                           nsIRDFAssertionCursor **sources /* out */)
{
	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



nsresult	GetVolumeList(nsVoidArray **array);
nsresult	GetFolderList(nsIRDFResource *source, nsVoidArray **array /* out */);
nsresult	GetName(nsIRDFResource *source, nsVoidArray **array);
nsresult	GetURL(nsIRDFResource *source, nsVoidArray **array);
PRBool		isVisible(nsNativeFileSpec file);



NS_IMETHODIMP
FileSystemDataSource::GetTarget(nsIRDFResource *source,
                          nsIRDFResource *property,
                          PRBool tv,
                          nsIRDFNode **target /* out */)
{
	nsresult		rv = NS_ERROR_RDF_NO_VALUE;

	// we only have positive assertions in the file system data source.
	if (! tv)
		return rv;

	if (isFileURI(source))
	{
		nsVoidArray		*array = nsnull;

		if (peq(property, kNC_Name))
		{
			rv = GetName(source, &array);
		}
		else if (peq(property, kNC_URL))
		{
			rv = GetURL(source, &array);
		}
		else if (peq(property, kRDF_type))
		{
                    nsXPIDLCString uri;
			kNC_FileSystemObject->GetValue( getter_Copies(uri) );
			if (uri)
			{
				nsAutoString	url(uri);
				nsIRDFLiteral	*literal;
				gRDFService->GetLiteral(url, &literal);
				*target = literal;
				rv = NS_OK;
			}
			return(rv);
		}
		if (array != nsnull)
		{
			nsIRDFLiteral *literal = (nsIRDFLiteral *)(array->ElementAt(0));
			*target = (nsIRDFNode *)literal;
			delete array;
			rv = NS_OK;
		}
		else
		{
			rv = NS_ERROR_RDF_NO_VALUE;
		}
	}
	return(rv);
}



NS_IMETHODIMP
FileSystemDataSource::GetTargets(nsIRDFResource *source,
                           nsIRDFResource *property,
                           PRBool tv,
                           nsIRDFAssertionCursor **targets /* out */)
{
	nsVoidArray		*array = nsnull;
	nsresult		rv = NS_ERROR_FAILURE;

	// we only have positive assertions in the file system data source.
	if (! tv)
		return rv;

	if (peq(source, kNC_FileSystemRoot))
	{
		if (peq(property, kNC_Child))
		{
			rv = GetVolumeList(&array);
		}
	}
	else if (isFileURI(source))
	{
		if (peq(property, kNC_Child))
		{
			rv = GetFolderList(source, &array);
		}
		else if (peq(property, kNC_Name))
		{
			rv = GetName(source, &array);
		}
		else if (peq(property, kNC_URL))
		{
			rv = GetURL(source, &array);
		}
		else if (peq(property, kRDF_type))
		{
                    nsXPIDLCString uri;
			kNC_FileSystemObject->GetValue( getter_Copies(uri) );
			if (uri)
			{
				nsAutoString	url(uri);
				nsIRDFLiteral	*literal;
				gRDFService->GetLiteral(url, &literal);
				array = new nsVoidArray();
				if (array)
				{
					array->AppendElement(literal);
					rv = NS_OK;
				}
			}
		}
	}
	if ((rv == NS_OK) && (nsnull != array))
	{
		*targets = new FileSystemCursor(source, property, PR_FALSE, array);
		NS_ADDREF(*targets);
	}
	return(rv);
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
	PRBool			retVal = PR_FALSE;
	nsresult		rv = NS_ERROR_FAILURE;

	// we only have positive assertions in the file system data source.
	if (! tv)
		return rv;

	*hasAssertion = PR_FALSE;
	if (peq(source, kNC_FileSystemRoot) || isFileURI(source))
	{
		if (peq(property, kRDF_type))
		{
			if (peq((nsIRDFResource *)target, kRDF_type))
			{
				*hasAssertion = PR_TRUE;
				rv = NS_OK;
			}
		}
	}
	return (rv);
}



NS_IMETHODIMP
FileSystemDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsIRDFArcsInCursor ** labels /* out */)
{
	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::ArcLabelsOut(nsIRDFResource *source,
                             nsIRDFArcsOutCursor **labels /* out */)
{
	nsresult		rv = NS_ERROR_RDF_NO_VALUE;

	*labels = nsnull;

	if (peq(source, kNC_FileSystemRoot))
	{
		nsVoidArray *temp = new nsVoidArray();
		if (nsnull == temp)
			return NS_ERROR_OUT_OF_MEMORY;

		temp->AppendElement(kNC_Child);
		*labels = new FileSystemCursor(source, kNC_Child, PR_TRUE, temp);
		if (nsnull != *labels)
		{
			NS_ADDREF(*labels);
			rv = NS_OK;
		}
	}
	else if (isFileURI(source))
	{
		nsVoidArray *temp = new nsVoidArray();
		if (nsnull == temp)
			return NS_ERROR_OUT_OF_MEMORY;

                nsXPIDLCString uri;
		source->GetValue( getter_Copies(uri) );
		if (uri)
		{
			nsFileURL	fileURL(uri);
			nsFileSpec	fileSpec(fileURL);
			if (fileSpec.IsDirectory())
			{
				temp->AppendElement(kNC_Child);
			}
		}

		temp->AppendElement(kRDF_type);
		*labels = new FileSystemCursor(source, kRDF_type, PR_TRUE, temp);
		if (nsnull != *labels)
		{
			NS_ADDREF(*labels);
			rv = NS_OK;
		}
	}
	return(rv);

}



NS_IMETHODIMP
FileSystemDataSource::GetAllResources(nsIRDFResourceCursor** aCursor)
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
GetVolumeList(nsVoidArray **array)
{
	nsVoidArray		*volumes = new nsVoidArray();

	*array = volumes;
	if (nsnull == *array)
	{
		return(NS_ERROR_OUT_OF_MEMORY);
	}

	nsIRDFResource		*vol;

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
		if (NS_SUCCEEDED(gRDFService->GetResource(nsFileURL(fss).GetAsString(), (nsIRDFResource**)&vol)))
		{
			NS_ADDREF(vol);
			volumes->AppendElement(vol);
		}
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
				gRDFService->GetResource(url, (nsIRDFResource **)&vol);
				NS_ADDREF(vol);
				volumes->AppendElement(vol);
				PR_Free(url);
			}
		}
	}
#endif

#ifdef	XP_UNIX
	gRDFService->GetResource("file:///", (nsIRDFResource **)&vol);
	NS_ADDREF(vol);
	volumes->AppendElement(vol);
#endif

	return NS_OK;
}



FileSystemCursor::FileSystemCursor(nsIRDFResource *source,
				nsIRDFResource *property,
				PRBool isArcsOut,
				nsVoidArray *array)
	: mSource(source),
	  mProperty(property),
	  mArcsOut(isArcsOut),
	  mArray(array),
	  mCount(0),
	  mTarget(nsnull),
	  mValue(nsnull)
{
	NS_INIT_REFCNT();
	NS_ADDREF(mSource);
	NS_ADDREF(mProperty);
}



FileSystemCursor::~FileSystemCursor(void)
{
	NS_IF_RELEASE(mSource);
	NS_IF_RELEASE(mValue);
	NS_IF_RELEASE(mProperty);
	NS_IF_RELEASE(mTarget);
	if (nsnull != mArray)
	{
		delete mArray;
	}
}



NS_IMETHODIMP
FileSystemCursor::Advance(void)
{
	if (!mArray)
		return NS_ERROR_NULL_POINTER;
	if (mArray->Count() <= mCount)
		return NS_ERROR_RDF_CURSOR_EMPTY;
	NS_IF_RELEASE(mValue);
	mTarget = mValue = (nsIRDFNode *)mArray->ElementAt(mCount++);
	NS_ADDREF(mValue);
	NS_ADDREF(mTarget);
	return NS_OK;
}



NS_IMETHODIMP
FileSystemCursor::GetValue(nsIRDFNode **aValue)
{
	if (nsnull == mValue)
		return NS_ERROR_NULL_POINTER;
	NS_ADDREF(mValue);
	*aValue = mValue;
	return NS_OK;
}



NS_IMETHODIMP
FileSystemCursor::GetDataSource(nsIRDFDataSource **aDataSource)
{
	NS_ADDREF(gFileSystemDataSource);
	*aDataSource = gFileSystemDataSource;
	return NS_OK;
}



NS_IMETHODIMP
FileSystemCursor::GetSource(nsIRDFResource **aResource)
{
	NS_ADDREF(mSource);
	*aResource = mSource;
	return NS_OK;
}



NS_IMETHODIMP
FileSystemCursor::GetLabel(nsIRDFResource **aPredicate)
{
	if (mArcsOut == PR_FALSE)
	{
		NS_ADDREF(mProperty);
		*aPredicate = mProperty;
	}
	else
	{
		if (nsnull == mValue)
			return NS_ERROR_NULL_POINTER;
		NS_ADDREF(mValue);
		*(nsIRDFNode **)aPredicate = mValue;
	}
	return NS_OK;
}



NS_IMETHODIMP
FileSystemCursor::GetTarget(nsIRDFNode **aObject)
{
	if (nsnull != mTarget)
		NS_ADDREF(mTarget);
	*aObject = mTarget;
	return NS_OK;
}



NS_IMETHODIMP
FileSystemCursor::GetTruthValue(PRBool *aTruthValue)
{
	*aTruthValue = 1;
	return NS_OK;
}



NS_IMPL_ADDREF(FileSystemCursor);
NS_IMPL_RELEASE(FileSystemCursor);



NS_IMETHODIMP
FileSystemCursor::QueryInterface(REFNSIID iid, void **result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	if (iid.Equals(kIRDFAssertionCursorIID) ||
		iid.Equals(kIRDFCursorIID) ||
		iid.Equals(kIRDFArcsOutCursorIID) ||
		iid.Equals(kISupportsIID))
	{
		*result = NS_STATIC_CAST(nsIRDFAssertionCursor *, this);
		AddRef();
		return NS_OK;
	}
	return(NS_NOINTERFACE);
}



PRBool
isVisible(nsNativeFileSpec file)
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
		delete []basename;
	}
#endif

	return(isVisible);
}



nsresult
GetFolderList(nsIRDFResource *source, nsVoidArray **array /* out */)
{
	nsVoidArray	*nameArray = new nsVoidArray();
	*array = nameArray;
	if (nsnull == nameArray)
	{
		return(NS_ERROR_OUT_OF_MEMORY);
	}

        nsXPIDLCString uri;
	source->GetValue( getter_Copies(uri) );
	if (! uri)
	{
		return(NS_ERROR_FAILURE);
	}

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
			nsIRDFResource	*file;
			gRDFService->GetResource(childURL, (nsIRDFResource **)&file);
			nameArray->AppendElement(file);
		}
	}
	return(NS_OK);
}



nsresult
GetName(nsIRDFResource *source, nsVoidArray **array)
{
	nsVoidArray *nameArray = new nsVoidArray();
	*array = nameArray;
	if (nsnull == nameArray)
	{
		return(NS_ERROR_OUT_OF_MEMORY);
	}

        nsXPIDLCString uri;
	source->GetValue( getter_Copies(uri) );
	nsFileURL		url(uri);
	nsNativeFileSpec	native(url);
	char			*basename = native.GetLeafName();
	if (basename)
	{
		nsAutoString	name(basename);
		nsIRDFLiteral *literal;
		gRDFService->GetLiteral(name, &literal);
		nameArray->AppendElement(literal);
		delete []basename;
	}
	return(NS_OK);
}



nsresult
GetURL(nsIRDFResource *source, nsVoidArray **array)
{
	nsVoidArray *urlArray = new nsVoidArray();
	*array = urlArray;
	if (nsnull == urlArray)
	{
		return(NS_ERROR_OUT_OF_MEMORY);
	}

        nsXPIDLCString uri;
	source->GetValue( getter_Copies(uri) );
	nsAutoString	url(uri);

	nsIRDFLiteral	*literal;
	gRDFService->GetLiteral(url, &literal);
	urlArray->AppendElement(literal);
	return(NS_OK);
}
