/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// BEGIN WINDOWS SPECIFIC
#include <windows.h>
// END WINDOWS SPECIFIC

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <vector>
#include <string>

#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"

#include "nsID.h"
#include "nsCOMPtr.h"
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

// These should really be std::map types to aid searching but aren't
std::vector<factory> listCLSIDs;
std::vector<progid2cid> listProgIDs;
std::vector<library> listComponents;

// Function declarations
PRBool ReadFileToBuffer(const char *szFile, char **ppszBuffer, long *pnBufferSize);
int ScanBuffer(const char *pszBuffer, long nBufferSize, const void *szPattern, long nPatternSize);
nsresult ReadComponentRegistry();
void OutputCID(const nsID &cid);
void OutputProgId(const char *szProgID);
void OutputLibrary(const char *szLibrary);
PRBool LibraryImplementsCID(const char *szLibrary, const nsID &cid);

int main(int argc, char *argv[])
{
	// These should be set by command switches but aren't at the moment
	PRBool bSearchForUnicodeProgIDs = PR_FALSE;
	PRBool bSearchForUnicodeLibraries = PR_FALSE;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: dumpdeps <mozbindirectory>\n");
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

	// Read the component registry to fill the arrays with CLSIDs
	ReadComponentRegistry();
	
	// Locate all libraries and add them to the components list
// BEGIN WINDOWS SPECIFIC
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

	// Enumerate through all libraries looking for dependencies
	std::vector<library>::const_iterator i;
	for (i = listComponents.begin(); i != listComponents.end(); i++)
	{
		char *pszBuffer = NULL;
		long nBufferSize = 0;
		if (ReadFileToBuffer((*i).mFullPath.c_str(), &pszBuffer, &nBufferSize))
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
				// Skip ProgIds that the library implements
				if (LibraryImplementsCID((*i).mFile.c_str(), (*k).mCID))
				{
					continue;
				}

				// Search for ANSI strings
				const char *szA = (*k).mProgID.c_str();
				int nLength = (*k).mProgID.length();
				if (ScanBuffer(pszBuffer, nBufferSize, szA, nLength) == 1)
				{
					OutputProgId(szA);
					continue;
				}

				if (bSearchForUnicodeProgIDs)
				{
					// Search for Unicode strings
					wchar_t *szW = A2W((*k).mProgID.c_str(), szWTmp, sizeof(szWTmp) / sizeof(szWTmp[0]));
					if (ScanBuffer(pszBuffer, nBufferSize, szW, nLength * sizeof(wchar_t)) == 1)
					{
						printf("UNICODE Progid\n");
						OutputProgId(szA);
						continue;
					}
				}
			}

			printf("  Statically linked to:\n");
			std::vector<library>::const_iterator l;
			for (l = listComponents.begin(); l != listComponents.end(); l++)
			{
				// Skip when the full path and the component point to the same thing
				if (i == l)
				{
					continue;
				}

				// Search for ANSI strings
				const char *szA = (*l).mFile.c_str();
				int nLength = (*l).mFile.length();
				if (ScanBuffer(pszBuffer, nBufferSize, szA, nLength) == 1)
				{
					OutputLibrary(szA);
					continue;
				}

				if (bSearchForUnicodeLibraries)
				{
					// Search for Unicode strings
					wchar_t *szW = A2W((*l).mFile.c_str(), szWTmp, sizeof(szWTmp) / sizeof(szWTmp[0]));
					if (ScanBuffer(pszBuffer, nBufferSize, szW, nLength * sizeof(wchar_t)) == 1)
					{
						printf("UNICODE library\n");
						OutputLibrary(szA);
						continue;
					}
				}
			}
			delete []pszBuffer;
		}
	}

	return 0;
}


// Reads a file into a big buffer and returns it
PRBool ReadFileToBuffer(const char *szFile, char **ppszBuffer, long *pnBufferSize)
{
	// Allocate a memory buffer to slurp up the whole file
	struct _stat srcStat;
	_stat(szFile, &srcStat);

	char *pszBuffer = new char[srcStat.st_size / sizeof(char)];
	if (pszBuffer == NULL)
	{
		printf("Error: Could not allocate buffer for file \"%s\"\n", szFile);
		return PR_FALSE;
	}
	FILE *fSrc = fopen(szFile, "rb");
	if (fSrc == NULL)
	{
		printf("Error: Could not open file \"%s\"\n", szFile);
		delete []pszBuffer;
		return PR_FALSE;
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
		return PR_FALSE;
	}

	*ppszBuffer = pszBuffer;
	*pnBufferSize = sizeRead;

	return PR_TRUE;
}

// Searchs for the specified pattern in the buffer
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

// Prints a CID with the ProgId and library if they are there too.
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

// Outputs the ProgId with the CID and library if they are there too.
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

// Outputs a library name
void OutputLibrary(const char *szLibrary)
{
	printf("    DLL \"%s\"\n", szLibrary);
}


// Tests whether a library implements the specified CID
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