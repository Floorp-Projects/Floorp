/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
