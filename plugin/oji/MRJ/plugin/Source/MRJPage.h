/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
	MRJPage.h
	
	Encapsulates the MRJ data structure.
	
	by Patrick C. Beard.
 */

#ifndef __TYPES__
#include <Types.h>
#endif

#ifndef CALL_NOT_IN_CARBON
	#define CALL_NOT_IN_CARBON 1
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
