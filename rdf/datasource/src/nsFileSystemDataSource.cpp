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

/*
  To do:
      1) build up volume list correctly (platform dependant).
         Currently hardcoded, Windows only.
      2) Encode/decode URLs appropriately (0x20 <-> %20, 0x2F <> '/', etc)
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
#include "nsRDFCID.h"
#include "rdfutil.h"
#include "nsIRDFService.h"
#include "xp_core.h"
#include "plhash.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "prio.h"
#include "nsFileSpec.h"
#include "nsIRDFFileSystem.h"
#include "nsFileSystemDataSource.h"

#ifdef	XP_WIN
#include "windef.h"
#include "winbase.h"
#endif



static NS_DEFINE_CID(kRDFServiceCID,               NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kIRDFServiceIID,              NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFFileSystemDataSourceIID, NS_IRDFFILESYSTEMDATAOURCE_IID);
static NS_DEFINE_IID(kIRDFFileSystemIID,           NS_IRDFFILESYSTEM_IID);
static NS_DEFINE_IID(kIRDFAssertionCursorIID,      NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,               NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,        NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kISupportsIID,                NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFResourceIID,             NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFResourceFactoryIID,      NS_IRDFRESOURCEFACTORY_IID);
static NS_DEFINE_IID(kIRDFNodeIID,                 NS_IRDFNODE_IID);



static const char kURINC_FileSystemRoot[] = "NC:FilesRoot";

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, url);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Columns);

DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Bag);



static	nsIRDFService		*gRDFService = nsnull;
static	FileSystemDataSource	*gFileSystemDataSource = nsnull;



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



nsIRDFResource	*FileSystemDataSource::kNC_FileSystemRoot;
nsIRDFResource	*FileSystemDataSource::kNC_Child;
nsIRDFResource	*FileSystemDataSource::kNC_Name;
nsIRDFResource	*FileSystemDataSource::kNC_URL;
nsIRDFResource	*FileSystemDataSource::kNC_Columns;

nsIRDFResource	*FileSystemDataSource::kRDF_InstanceOf;
nsIRDFResource	*FileSystemDataSource::kRDF_Bag;



FileSystemDataSource::FileSystemDataSource(void)
	: mURI(nsnull),
	  mObservers(nsnull)
{
	NS_INIT_REFCNT();
	nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
		kIRDFServiceIID, (nsISupports**) &gRDFService);
	PR_ASSERT(NS_SUCCEEDED(rv));
	gFileSystemDataSource = this;
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

	nsrefcnt	refcnt;
	NS_RELEASE2(kNC_FileSystemRoot, refcnt);
	NS_RELEASE2(kNC_Child, refcnt);
	NS_RELEASE2(kNC_Name, refcnt);
	NS_RELEASE2(kNC_URL, refcnt);
	NS_RELEASE2(kNC_Columns, refcnt);

	NS_RELEASE2(kRDF_InstanceOf, refcnt);
	NS_RELEASE2(kRDF_Bag, refcnt);

	gFileSystemDataSource = nsnull;
	nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
	gRDFService = nsnull;
}



NS_IMPL_ISUPPORTS(FileSystemDataSource, kIRDFFileSystemDataSourceIID);



NS_IMETHODIMP
FileSystemDataSource::Init(const char *uri)
{
	nsresult	rv = NS_ERROR_OUT_OF_MEMORY;

	if ((mURI = PL_strdup(uri)) == nsnull)
		return rv;

	gRDFService->GetResource(kURINC_FileSystemRoot, &kNC_FileSystemRoot);
	gRDFService->GetResource(kURINC_child, &kNC_Child);
	gRDFService->GetResource(kURINC_name, &kNC_Name);
	gRDFService->GetResource(kURINC_url, &kNC_URL);
	gRDFService->GetResource(kURINC_Columns, &kNC_Columns);

	gRDFService->GetResource(kURIRDF_instanceOf, &kRDF_InstanceOf);
	gRDFService->GetResource(kURIRDF_Bag, &kRDF_Bag);

	//   if (NS_FAILED(rv = AddColumns()))
	//       return rv;

	// register this as a named data source with the service manager
	if (NS_FAILED(rv = gRDFService->RegisterDataSource(this)))
		return rv;

	return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::GetURI(const char **uri) const
{
	*uri = mURI;
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



NS_IMETHODIMP
FileSystemDataSource::GetTarget(nsIRDFResource *source,
                          nsIRDFResource *property,
                          PRBool tv,
                          nsIRDFNode **target /* out */)
{
	nsIRDFFileSystem	*fs;
	nsresult		rv = NS_ERROR_RDF_NO_VALUE;

	// we only have positive assertions in the file system data source.
	if (! tv)
		return rv;

	if (NS_SUCCEEDED(source->QueryInterface(kIRDFFileSystemIID, (void **) &fs)))
	{
		if (peq(property, kNC_Name))
		{
		//	rv = fs->GetName((nsIRDFLiteral **)target);
		}
		else
		{
			rv = NS_ERROR_RDF_NO_VALUE;
		}
		NS_IF_RELEASE(fs);
	}
	return(rv);
}



nsresult GetVolumeList(nsVoidArray **array);



NS_IMETHODIMP
FileSystemDataSource::GetTargets(nsIRDFResource *source,
                           nsIRDFResource *property,
                           PRBool tv,
                           nsIRDFAssertionCursor **targets /* out */)
{
	nsIRDFFileSystem	*fs;
	nsVoidArray		*array = nsnull;
	nsresult		rv = NS_ERROR_FAILURE;

	if (peq(source, kNC_FileSystemRoot))
	{
		if (peq(property, kNC_Child))
		{
			rv = GetVolumeList(&array);
		}
	}
	else if (NS_OK == source->QueryInterface(kIRDFFileSystemIID, (void **)&fs))
	{
		if (peq(property, kNC_Child))
		{
			rv = fs->GetFolderList(source, &array);
		}
		else if (peq(property, kNC_Name))
		{
			rv = fs->GetName(&array);
		}
		else if (peq(property, kNC_URL))
		{
			rv = fs->GetURL(&array);
		}
		NS_IF_RELEASE(fs);
	}
	if ((rv == NS_OK) && (nsnull != array))
	{
		*targets = new FileSystemCursor(source, property, array);
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
	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
	PR_ASSERT(0);
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
FileSystemDataSource::HasAssertion(nsIRDFResource *source,
                             nsIRDFResource *property,
                             nsIRDFNode *target,
                             PRBool tv,
                             PRBool *hasAssertion /* out */)
{
	nsIRDFFileSystem	*fs = nsnull;
	PRBool			retVal = PR_FALSE;
	nsresult		rv = 0;				// xxx

	*hasAssertion = PR_FALSE;
	if (NS_SUCCEEDED(source->QueryInterface(kIRDFFileSystemIID, (void**) &fs)))
	{
		if (peq(property, kRDF_InstanceOf))
		{
			if (peq((nsIRDFResource *)target, kRDF_Bag))
			{
				*hasAssertion = PR_TRUE;
				rv = NS_OK;
			}
		}
		NS_IF_RELEASE(fs);
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
	nsIRDFFileSystem	*fs;
	nsresult		rv = NS_ERROR_RDF_NO_VALUE;

	if (NS_SUCCEEDED(source->QueryInterface(kIRDFFileSystemIID, (void**) &fs)))
	{
		nsVoidArray *temp = new nsVoidArray();
		if (nsnull == temp)
			return NS_ERROR_OUT_OF_MEMORY;

		temp->AppendElement(kNC_Child);
		temp->AppendElement(kNC_Name);
		temp->AppendElement(kNC_URL);
		temp->AppendElement(kNC_Columns);
		*labels = new FileSystemCursor(source, kNC_Child, temp);
		if (nsnull != *labels)
		{
			NS_ADDREF(*labels);
			rv = NS_OK;
		}
		NS_RELEASE(fs);
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
FileSystemDataSource::IsCommandEnabled(const char* aCommand,
                                 nsIRDFResource* aCommandTarget,
                                 PRBool* aResult)
{
	return NS_OK;
}



NS_IMETHODIMP
FileSystemDataSource::DoCommand(const char* aCommand,
                          nsIRDFResource* aCommandTarget)
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
GetVolumeList(nsVoidArray **array)
{
	nsIRDFFileSystem	*vol;
	nsVoidArray			*volumes = new nsVoidArray();

#if 1 
#ifdef	XP_WIN

	PRInt32				driveType;
	char				drive[32];
	PRInt32				volNum;
	char				*url;

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
	*array = volumes;
	return NS_OK;
#else
#endif

#else
	gRDFService->GetResource("file:///c|/", (nsIRDFResource **)&vol);
	volumes->AppendElement(vol);
//	NS_ADDREF(vol);
	gRDFService->GetResource("file:///d|/", (nsIRDFResource **)&vol);
	volumes->AppendElement(vol);
//	NS_ADDREF(vol);
	*array = volumes;
	return NS_OK;
#endif
}






FileSystemCursor::FileSystemCursor(nsIRDFResource *source,
				nsIRDFResource *property,
				nsVoidArray *array)
{
	mSource = source;
	mProperty = property;
	mArray = array;
	NS_ADDREF(mSource);
	NS_ADDREF(mProperty);
	mCount = 0;
	mTarget = nsnull;
	mValue = nsnull;
}



FileSystemCursor::~FileSystemCursor(void)
{
	NS_IF_RELEASE(mSource);
	NS_IF_RELEASE(mValue);
	NS_IF_RELEASE(mProperty);
	NS_IF_RELEASE(mTarget);
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
FileSystemCursor::GetSubject(nsIRDFResource **aResource)
{
	NS_ADDREF(mSource);
	*aResource = mSource;
	return NS_OK;
}



NS_IMETHODIMP
FileSystemCursor::GetPredicate(nsIRDFResource **aPredicate)
{
	NS_ADDREF(mProperty);
	*aPredicate = mProperty;
	return NS_OK;
}



NS_IMETHODIMP
FileSystemCursor::GetObject(nsIRDFNode **aObject)
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
	return NS_NOINTERFACE;
}



/********************************** FileSystem **************************************
 ************************************************************************************/



FileSystem::FileSystem(const char *uri)
{
	NS_INIT_REFCNT();
	mURI = PL_strdup(uri);
}



FileSystem::~FileSystem(void)
{
	gRDFService->UnCacheResource(this);
	PL_strfree(mURI);
}



NS_IMPL_IRDFRESOURCE(FileSystem);
NS_IMPL_ADDREF(FileSystem);
NS_IMPL_RELEASE(FileSystem);



NS_IMETHODIMP
FileSystem::QueryInterface(REFNSIID iid, void **result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	if (iid.Equals(kIRDFResourceIID) ||
		iid.Equals(kIRDFNodeIID) ||
		iid.Equals(kIRDFFileSystemIID) ||
		iid.Equals(kISupportsIID))
	{
		*result = NS_STATIC_CAST(nsIRDFFileSystem *, this);
		AddRef();
		return NS_OK;
	}
	return NS_NOINTERFACE;
}



static PRBool
isDirectory(const char *parent, const char *optFile)
{
	PRFileInfo	info;
	PRStatus	err;
	PRBool		isDirFlag = PR_FALSE;
	char		*url;

	nsAutoString	pathname(parent);
	if (nsnull != optFile)
	{
		pathname += optFile;
	}
	url = pathname.ToNewCString();
	if (nsnull != url)
	{
		if ((err = PR_GetFileInfo(url, &info)) == PR_SUCCESS)
		{
			if (info.type == PR_FILE_DIRECTORY)
			{
				isDirFlag = PR_TRUE;
			}
		}
	}
	return(isDirFlag);
}



NS_IMETHODIMP
FileSystem::GetFolderList(nsIRDFResource *source, nsVoidArray **array /* out */) const
{
	nsAutoString		name(mURI);

	*array = nsnull;

	if (name.Find("file:///") == 0)
	{
#ifdef	XP_UNIX
		name.Cut(0, strlen("file://"));
#else
		name.Cut(0, strlen("file:///"));
#endif

#ifdef	XP_WIN
		// on Windows, replace pipe with colon
		if (name.Length() > 2)
		{
			if (name.CharAt(1) == '|')
			{
				name.CharAt(1) = ':';
			}
		}
#endif

		PRDir			*dir;
		PRDirEntry		*de;
		PRInt32			n = PR_SKIP_BOTH;
		nsIRDFFileSystem 	*file;

		nsVoidArray	*nameArray = new nsVoidArray();

		char		*dirName = name.ToNewCString();

#define	DEPTH_LIMIT	2

#ifdef	DEPTH_LIMIT
		char		*p;
		PRInt32		slashCount = 0;

		char		*temp = name.ToNewCString();
		p = strchr(temp, '/');
		while (nsnull != p)
		{
			++ slashCount;
			++p;
			p = strchr(p, '/');
		}
		delete temp;
		if (slashCount > DEPTH_LIMIT)
		{
			*array = nsnull;
			delete dirName;
			delete nameArray;
			return NS_OK;
		}
#endif

		if (nsnull != dirName)
		{
			if (nsnull != (dir = PR_OpenDir(dirName)))
			{
				while (nsnull != (de = PR_ReadDir(dir, (PRDirFlags)(n++))))
				{
					nsAutoString pathname(mURI);
					pathname += de->name;

					if (PR_TRUE == isDirectory(dirName, de->name))
					{
						pathname += "/";
					}
					char *filename = pathname.ToNewCString();
					if (nsnull != filename)
					{
						gRDFService->GetResource(filename, (nsIRDFResource **)&file);
						delete filename;
						nameArray->AppendElement(file);
					}
				}
				PR_CloseDir(dir);
			}
			delete dirName;
		}

		*array = nameArray;
	}
	return NS_OK;
}



NS_IMETHODIMP
FileSystem::GetName(nsVoidArray **array) const
{
	nsString		*basename = nsnull;
	nsAutoString		name(mURI);

	PRInt32			len = name.Length();
	if (len > 0)
	{
		if (name.CharAt(len-1) == '/')
		{
			name.SetLength(--len);
		}
	}

	PRInt32			slashIndex = name.RFind("/");
	if (slashIndex > 0)
	{
		basename = new nsString;
		name.Mid(*basename, slashIndex+1, len-slashIndex);

#ifdef	XP_WIN
		// on Windows, replace pipe with colon and uppercase drive letter
		if (basename->Length() == 2)
		{
			if (basename->CharAt(1) == '|')
			{
				basename->CharAt(1) = ':';
				basename->ToUpperCase();
			}
		}
#endif
	}

	*array = nsnull;
	if (nsnull != basename)
	{
		nsIRDFLiteral *literal;
		gRDFService->GetLiteral(*basename, &literal);
		// NS_ADDREF(literal);
		delete basename;
		nsVoidArray *nameArray = new nsVoidArray();
		nameArray->AppendElement(literal);
		*array = nameArray;
	}
	return NS_OK;
}



NS_IMETHODIMP
FileSystem::GetURL(nsVoidArray **array) const
{
	nsVoidArray *urlArray = new nsVoidArray();
	*array = urlArray;
	if (nsnull != urlArray)
	{
		nsIRDFLiteral	*literal;

		nsAutoString	url(mURI);

		gRDFService->GetLiteral(url, &literal);
		NS_ADDREF(literal);
		urlArray->AppendElement(literal);
	}
	return NS_OK;
}



class FileSystemResourceFactoryImpl : public nsIRDFResourceFactory
{
public:
			FileSystemResourceFactoryImpl(void);
	virtual		~FileSystemResourceFactoryImpl(void);

	NS_DECL_ISUPPORTS

	NS_IMETHOD	CreateResource(const char* aURI, nsIRDFResource** aResult);
};



FileSystemResourceFactoryImpl::FileSystemResourceFactoryImpl(void)
{
	NS_INIT_REFCNT();
}



FileSystemResourceFactoryImpl::~FileSystemResourceFactoryImpl(void)
{
}



NS_IMPL_ISUPPORTS(FileSystemResourceFactoryImpl, kIRDFResourceFactoryIID);



NS_IMETHODIMP
FileSystemResourceFactoryImpl::CreateResource(const char* aURI, nsIRDFResource** aResult)
{
	if (! aResult)
		return NS_ERROR_NULL_POINTER;
	FileSystem *fs = new FileSystem(aURI);
	if (!fs)
		return NS_ERROR_OUT_OF_MEMORY;
	NS_ADDREF(fs);
	*aResult = fs;
	return NS_OK;
}



nsresult
NS_NewRDFFileSystemResourceFactory(nsIRDFResourceFactory** aResult)
{
	if (! aResult)
		return NS_ERROR_NULL_POINTER;
	FileSystemResourceFactoryImpl *factory = new FileSystemResourceFactoryImpl();
	if (! factory)
		return NS_ERROR_OUT_OF_MEMORY;
	NS_ADDREF(factory);
	*aResult = factory;
	return NS_OK;
}

