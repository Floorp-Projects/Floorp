/*
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
	MRJPage.h
	
	Encapsulates the MRJ data structure.
	
	by Patrick C. Beard.
 */

#ifndef __TYPES__
#include <Types.h>
#endif

#include "JManager.h"

// For now.
typedef struct OpaqueJMAppletPageRef* 	JMAppletPageRef;

class MRJSession;

struct MRJPageAttributes {
	UInt32 documentID;
	const char* codeBase;
	const char* archive;
	Boolean mayScript;
};

class MRJPage {
public:
	MRJPage(MRJSession* session, UInt32 documentID, const char* codeBase, const char* archive, Boolean mayScript);
	MRJPage(MRJSession* session, const MRJPageAttributes& attributes);
	~MRJPage();
	
	// Pages are reference counted.
	UInt16 AddRef(void);
	UInt16 Release(void);
	
	JMAppletPageRef getPageRef() { return mPageRef; }
	
	UInt32 getDocumentID() { return mDocumentID; }
	const char* getCodeBase() { return mCodeBase; }
	const char* getArchive() { return mArchive; }
	Boolean getMayScript() { return mMayScript; }

	// Creating AWTContexts.
	Boolean createContext(JMAWTContextRef* outContext,
							const JMAWTContextCallbacks * callbacks,
							JMClientData data);

    // Accessing the list of instances.
    static MRJPage* getFirstPage(void);
    MRJPage* getNextPage(void);

private:
	void pushPage();
	void popPage();

private:
	UInt16 mRefCount;
	MRJPage* mNextPage;
	MRJSession* mSession;
	JMAppletPageRef mPageRef;
	UInt32 mDocumentID;
	char* mCodeBase;
	char* mArchive;
	Boolean mMayScript;	
};
