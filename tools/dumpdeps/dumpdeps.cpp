// BEGIN WINDOWS SPECIFIC
#include <windows.h>
// END WINDOWS SPECIFIC

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <vector>
#include <string>

#include "nsIComponentManager.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"

#include "nsID.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIAllocator.h"
#include "nsIRegistry.h"
#include "nsILocalFile.h"

extern "C" NS_EXPORT nsresult NS_RegistryGetFactory(nsIFactory** aFactory);


inline wchar_t * A2W(const char *lpa, wchar_t *lpw,int nChars)
{
	lpw[0] = '\0';
// BEGIN WINDOWS SPECIFIC
	MultiByteToWideChar(CP_ACP, 0, lpa, -1, lpw, nChars);
// END WINDOWS SPECIFIC
	return lpw;
}

struct factory
{
	nsID mCID;
	std::string mLibrary;

	factory(nsID cid, const char * library) :
		mCID(cid),
		mLibrary(library)
	{
	}
};

struct progid2cid
{
	std::string mProgID;
	nsID mCID;

	progid2cid(const char *progID, nsID cid) :
		mProgID(progID),
		mCID(cid)
	{
	}
};

struct library
{
	std::string mFullPath;
	std::string mFile;

	library(const char *fullPath, const char *file) :
		mFullPath(fullPath),
		mFile(file)
	{
	}
};

// These should really be std::map types but aren't
std::vector<factory> listCLSIDs;
std::vector<progid2cid> listProgIDs;


std::vector<std::string> listFullPathComponents;
std::vector<library> listComponents;

int ReadFileToBuffer(const char *szFile, char **ppszBuffer, long *pnBufferSize);
int ScanBuffer(const char *pszBuffer, long nBufferSize, const void *szPattern, long nPatternSize);
nsresult ReadComponentRegistry();
void OutputCID(const nsID &cid);
void OutputProgId(const char *szProgID);
void OutputLibrary(const char *szLibrary);
PRBool LibraryImplementsCID(const char *szLibrary, const nsID &cid);
PRBool LibraryImplementsProgId(const char *szLibrary, const char *szProgId);

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, " Outputdeps <mozbindirectory>\n");
		return 1;
	}

	std::string sBinDirPath = argv[1];

	nsIServiceManager   *mServiceManager;
	nsILocalFile *pBinDirPath = nsnull;
	nsresult res = NS_NewLocalFile(sBinDirPath.c_str(), &pBinDirPath);

	// Initialise XPCOM
	if (pBinDirPath == nsnull)
	{
		fprintf(stderr, "Error: Could not create object to represent mozbindirectory - is the path correct?\n");
		return 1;
	}

	NS_InitXPCOM(&mServiceManager, pBinDirPath);
	NS_RELEASE(pBinDirPath);

	ReadComponentRegistry();
	
// BEGIN WINDOWS SPECIFIC
	// Build a list of components assuming them to have been passed as
	// command line arguments.
	for (int n = 0; n < 2; n++)
	{
		// Windows specific paths
		std::string sFolder;
		if (n == 0)
		{
			sFolder = sBinDirPath + "\\";
		}
		else if (n == 1)
		{
			sFolder = sBinDirPath + "\\components\\";
		}
		std::string sPattern = sFolder + "*.dll";
		WIN32_FIND_DATA findData;
		HANDLE h = FindFirstFile(sPattern.c_str(), &findData);
		if (h)
		{
			do {
				std::string fullPath = sFolder + findData.cFileName;
				listComponents.push_back(library(fullPath.c_str(), findData.cFileName));
			} while (FindNextFile(h, &findData));
		}
		FindClose(h);
	}
// END WINDOWS SPECIFIC

	// Open the component manager

    nsIComponentManager* cm = nsnull;
    nsresult rv = NS_GetGlobalComponentManager(&cm);
	if (cm == nsnull)
	{
		return 1;
	}

	// Enumerate through all libraries looking for dependencies

	std::vector<library>::const_iterator i;
	for (i = listComponents.begin(); i != listComponents.end(); i++)
	{
		char *pszBuffer = NULL;
		long nBufferSize = 0;
		if (ReadFileToBuffer((*i).mFullPath.c_str(), &pszBuffer, &nBufferSize) == 0)
		{
			printf("Library \"%s\"\n", (*i).mFullPath.c_str());

			// Print out implements
			printf("  Implements:\n");

			// Search for CIDs implemented by this library
			std::vector<factory>::const_iterator j;
			for (j = listCLSIDs.begin(); j != listCLSIDs.end(); j++)
			{
				if (LibraryImplementsCID((*i).mFile.c_str(), (*j).mCID))
				{
					OutputCID((*j).mCID);
				}
			}

			// Print out references
			printf("  References:\n");

			// Search for CIDs not implemented by this library but referenced
			for (j = listCLSIDs.begin(); j != listCLSIDs.end(); j++)
			{
				if (!LibraryImplementsCID((*i).mFile.c_str(), (*j).mCID) &&
					ScanBuffer(pszBuffer, nBufferSize, &(*j).mCID, sizeof((*j).mCID)) == 1)
				{
					OutputCID((*j).mCID);
				}
			}

			// Search for ProgIds
			wchar_t szWTmp[1024];		
			std::vector<progid2cid>::const_iterator k;
			for (k = listProgIDs.begin(); k != listProgIDs.end(); k++)
			{
				const char *szA = (*k).mProgID.c_str();
				wchar_t *szW = A2W((*k).mProgID.c_str(), szWTmp, sizeof(szWTmp) / sizeof(szWTmp[0]));
				int nLength = (*k).mProgID.length();
				if (ScanBuffer(pszBuffer, nBufferSize, szA, nLength) == 1 ||
					ScanBuffer(pszBuffer, nBufferSize, szW, nLength * sizeof(wchar_t)) == 1)
				{
					OutputProgId(szA);
				}
			}

			printf("  Depends on:\n");
			std::vector<library>::const_iterator l;
			for (l = listComponents.begin(); l != listComponents.end(); l++)
			{
				// Skip when the full path and the component point to the same thing
				if (i == l)
				{
					continue;
				}

				const char *szA = (*l).mFile.c_str();
				wchar_t *szW = A2W((*l).mFile.c_str(), szWTmp, sizeof(szWTmp) / sizeof(szWTmp[0]));
				int nLength = (*l).mFile.length();
				if (ScanBuffer(pszBuffer, nBufferSize, szA, nLength) == 1 ||
					ScanBuffer(pszBuffer, nBufferSize, szW, nLength * sizeof(wchar_t)) == 1)
				{
					OutputLibrary(szA);
				}
			}
			delete []pszBuffer;
		}
	}

	return 0;
}


int ReadFileToBuffer(const char *szFile, char **ppszBuffer, long *pnBufferSize)
{
	// Allocate a memory buffer to slurp up the whole file
	struct _stat srcStat;
	_stat(szFile, &srcStat);

	char *pszBuffer = new char[srcStat.st_size / sizeof(char)];
	if (pszBuffer == NULL)
	{
		printf("Error: Could not allocate buffer for file \"%s\"\n", szFile);
		return 1;
	}
	FILE *fSrc = fopen(szFile, "rb");
	if (fSrc == NULL)
	{
		printf("Error: Could not open file \"%s\"\n", szFile);
		delete []pszBuffer;
		return 1;
	}
	size_t sizeSrc = srcStat.st_size;
	size_t sizeRead = 0;

	// Dumb but effective
	sizeRead = fread(pszBuffer, 1, sizeSrc, fSrc);
	fclose(fSrc);

	if (sizeRead != sizeSrc)
	{
		printf("Error: Could not read all of file \"%s\"\n", szFile);
		delete []pszBuffer;
		return 1;
	}

	*ppszBuffer = pszBuffer;
	*pnBufferSize = sizeRead;

	return 0;
}

int ScanBuffer(const char *pszBuffer, long nBufferSize, const void *szPattern, long nPatternSize)
{
	// Scan through buffer, one byte at a time doing a memcmp against the
	// provided binary data.
	for (size_t i = 0; i < nBufferSize - nPatternSize; i++)
	{
		if (memcmp(&pszBuffer[i], szPattern, nPatternSize) == 0)
		{
			return 1;
		}
	}
	return 0;
}


void OutputCID(const nsID &cid)
{
	std::vector<progid2cid>::const_iterator i;
	for (i = listProgIDs.begin(); i != listProgIDs.end(); i++)
	{
		if (memcmp(&cid, &(*i).mCID, sizeof(nsID)) == 0)
		{
			printf("    ProgId = \"%s\"\n", (*i).mProgID.c_str());
			printf("      CLSID = %s\n", cid.ToString());
			std::vector<factory>::const_iterator j;
			for (j = listCLSIDs.begin(); j != listCLSIDs.end(); j++)
			{
				if (memcmp(&cid, &(*j).mCID, sizeof(nsID)) == 0)
				{
					printf("      Library = %s\n", (*j).mLibrary.c_str());
				}
			}
			return;
		}
	}
	printf("    CLSID = %s\n", cid.ToString());
}


void OutputProgId(const char *szProgID)
{
	std::vector<progid2cid>::const_iterator i;
	for (i = listProgIDs.begin(); i != listProgIDs.end(); i++)
	{
		if (strcmp((*i).mProgID.c_str(), szProgID) == 0)
		{
			printf("    ProgId  = \"%s\"\n", (*i).mProgID.c_str());
			printf("      CLSID = %s\n", (*i).mCID.ToString());
			std::vector<factory>::const_iterator j;
			for (j = listCLSIDs.begin(); j != listCLSIDs.end(); j++)
			{
				if (memcmp(&(*i).mCID, &(*j).mCID, sizeof(nsID)) == 0)
				{
					printf("      Library = %s\n", (*j).mLibrary.c_str());
				}
			}
			return;
		}
	}
	printf("    ProgID \"%s\"\n", szProgID);
}


void OutputLibrary(const char *szLibrary)
{
	printf("    DLL    \"%s\"\n", szLibrary);
}


PRBool LibraryImplementsCID(const char *szLibrary, const nsID &cid)
{
	// Search for CID - vector should really be a map, but it isn't
	std::vector<factory>::const_iterator i;
	for (i = listCLSIDs.begin(); i != listCLSIDs.end(); i++)
	{
		if (memcmp(&cid, &(*i).mCID, sizeof(nsID)) == 0)
		{
			if (strstr((*i).mLibrary.c_str(), szLibrary) != NULL)
			{
				return PR_TRUE;
			}

			return PR_FALSE;
		}
	}
	return PR_FALSE;
}

PRBool LibraryImplementsProgId(const char *szLibrary, const char *szProgId)
{
	// Search for CID - vector should really be a map, but it isn't for now
	return PR_FALSE;
}


// The following horrific-ness is ripped from nsComponentManager.cpp and sewn
// back together which is why it may look nasty.
//
// Basically it opens the registry and finds the CIDs, libraries and ProgIDs
// therein and adds them to some lists for easy lookup.

nsresult ReadComponentRegistry()
{
	nsIRegistry *mRegistry = nsnull;
	nsresult rv = NS_OK;

	// We need to create our registry. Since we are in the constructor
	// we haven't gone as far as registering the registry factory.
	// Hence, we hand create a registry.
	if (mRegistry == nsnull) {            
		nsIFactory *registryFactory = nsnull;
		rv = NS_RegistryGetFactory(&registryFactory);
		if (NS_SUCCEEDED(rv))
		{
			rv = registryFactory->CreateInstance(NULL, NS_GET_IID(nsIRegistry),(void **)&mRegistry);
			if (NS_FAILED(rv)) return rv;
			NS_RELEASE(registryFactory);
		}
	}

    // Open the App Components registry. We will keep it open forever!
	rv = mRegistry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
	if (NS_FAILED(rv)) return rv;


	nsRegistryKey       mXPCOMKey;
	nsRegistryKey       mClassesKey;
	nsRegistryKey       mCLSIDKey;

	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	const char xpcomKeyName[]="software/mozilla/XPCOM";
	const char classesKeyName[]="progID";
	const char classIDKeyName[]="classID";
	const char componentsKeyName[]="components";
	const char componentLoadersKeyName[]="componentLoaders";
	const char xpcomComponentsKeyName[]="software/mozilla/XPCOM/components";
	const char classIDValueName[]="ClassID";
	const char classNameValueName[]="ClassName";
	const char inprocServerValueName[]="InprocServer";
	const char componentTypeValueName[]="ComponentType";
	const char nativeComponentType[]="application/x-mozilla-native";

	nsRegistryKey xpcomKey;
	rv = mRegistry->AddSubtree(nsIRegistry::Common, xpcomKeyName, &xpcomKey);
	if (NS_FAILED(rv)) return rv;

	rv = mRegistry->AddSubtree(xpcomKey, componentsKeyName, &mXPCOMKey);
	if (NS_FAILED(rv)) return rv;

	rv = mRegistry->AddSubtree(xpcomKey, classesKeyName, &mClassesKey);
	if (NS_FAILED(rv)) return rv;

	rv = mRegistry->AddSubtree(xpcomKey, classIDKeyName, &mCLSIDKey);
	if (NS_FAILED(rv)) return rv;

    // Read in all CID entries and populate the mFactories
    nsCOMPtr<nsIEnumerator> cidEnum;
    rv = mRegistry->EnumerateSubtrees( mCLSIDKey, getter_AddRefs(cidEnum));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRegistryEnumerator> regEnum = do_QueryInterface(cidEnum, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = regEnum->First();
    for (rv = regEnum->First();
         NS_SUCCEEDED(rv) && (regEnum->IsDone() != NS_OK);
         rv = regEnum->Next())
    {
        const char *cidString;
        nsRegistryKey cidKey;
        /*
         * CurrentItemInPlaceUTF8 will give us back a _shared_ pointer in 
         * cidString.  This is bad XPCOM practice.  It is evil, and requires
         * great care with the relative lifetimes of cidString and regEnum.
         *
         * It is also faster, and less painful in the allocation department.
         */
        rv = regEnum->CurrentItemInPlaceUTF8(&cidKey, &cidString);
        if (NS_FAILED(rv))  continue;

        // Create the CID entry
        nsXPIDLCString library;
        rv = mRegistry->GetStringUTF8(cidKey, inprocServerValueName,
                                      getter_Copies(library));
        if (NS_FAILED(rv)) continue;
        nsCID aClass;

        if (!(aClass.Parse(cidString))) continue;

        nsXPIDLCString componentType;
        if (NS_FAILED(mRegistry->GetStringUTF8(cidKey, componentTypeValueName,
                                           getter_Copies(componentType))))
            continue;

		listCLSIDs.push_back(factory(aClass, library));
    }

    // Finally read in PROGID -> CID mappings
    nsCOMPtr<nsIEnumerator> progidEnum;
    rv = mRegistry->EnumerateSubtrees( mClassesKey, getter_AddRefs(progidEnum));
    if (NS_FAILED(rv)) return rv;

    regEnum = do_QueryInterface(progidEnum, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = regEnum->First();
    for (rv = regEnum->First();
         NS_SUCCEEDED(rv) && (regEnum->IsDone() != NS_OK);
         rv = regEnum->Next())
    {
        const char *progidString;
        nsRegistryKey progidKey;
        /*
         * CurrentItemInPlaceUTF8 will give us back a _shared_ pointer in 
         * progidString.  This is bad XPCOM practice.  It is evil, and requires
         * great care with the relative lifetimes of progidString and regEnum.
         *
         * It is also faster, and less painful in the allocation department.
         */
        rv = regEnum->CurrentItemInPlaceUTF8(&progidKey, &progidString);
        if (NS_FAILED(rv)) continue;

        nsXPIDLCString cidString;
        rv = mRegistry->GetStringUTF8(progidKey, classIDValueName,
                                      getter_Copies(cidString));
        if (NS_FAILED(rv)) continue;

        nsCID *aClass = new nsCID();
        if (!aClass) continue;		// Protect against out of memory.
        if (!(aClass->Parse(cidString)))
        {
            delete aClass;
            continue;
        }

        // put the {progid, Cid} mapping into our map
		listProgIDs.push_back(progid2cid(progidString, *aClass));

		delete aClass;
    }

	// ------------------------------------------------------------------------

	NS_RELEASE(mRegistry);

	return rv;
}