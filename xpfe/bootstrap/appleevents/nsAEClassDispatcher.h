/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Simon Fraser <sfraser@netscape.com>
 */


#ifndef __AECLASSDISPATCHER__
#define __AECLASSDISPATCHER__


#include "patricia.h"

// this class is used to dispatch calls to handlers on the basis of their class. It avoids
// us having to switch on the class in the code.

class AEGenericClass;

class AEDispatchHandler
{
public:
						AEDispatchHandler(DescType handlerClass, AEGenericClass* handler, Boolean deleteOnRemove = true);
						~AEDispatchHandler();
						
						
	void					DispatchEvent(					AEDesc *			token,
													const AppleEvent *	appleEvent,
													AppleEvent *		reply);

	void					GetProperty(					DescType			desiredClass,
													const AEDesc*		containerToken,
													DescType			containerClass,
													DescType			keyForm,
													const AEDesc*		keyData,
													AEDesc*			resultToken);
													
	void					GetDataFromListOrObject(			const AEDesc *		tokenOrTokenList,
													AEDesc *			desiredTypes,
													AEDesc *			data);

	void					GetItemFromContainer(			DescType			desiredClass,
													const AEDesc*		containerToken,
													DescType			containerClass, 
													DescType			keyForm,
													const AEDesc*		keyData,
													AEDesc*			resultToken);

	void					CompareObjects(				DescType			comparisonOperator,
													const AEDesc *		object,
													const AEDesc *		descriptorOrObject,
													Boolean *			result);


	void					CountObjects(					DescType 		 	desiredType,
													DescType 		 	containerClass,
													const AEDesc *		container,
									   				long *			result);

	void					CreateSelfSpecifier(				const AEDesc *		token,
													AEDesc *			outSpecifier);
													
protected:
	
	Boolean				mDeleteHandler;
	DescType				mHandlerClass;
	AEGenericClass*		mHandler;
};



class AEDispatchTree
{
public:
						AEDispatchTree();
						~AEDispatchTree();
						
	void					InsertHandler(DescType handlerClass, AEGenericClass *handler, Boolean isDuplicate = false);
	AEDispatchHandler*		FindHandler(DescType handlerClass);
	
protected:

	enum {
		kDuplicateKeyError = 750
	};
	
	static int 				FreeDispatchTreeNodeData(void *nodeData, unsigned char *key, void *refCon);
	static int 				ReplaceDispatchTreeNode(void *nodeData, unsigned char *key, void *replaceData);
	
	
protected:

	PatriciaTreeRef		mTree;
};



#endif /* __AECLASSDISPATCHER__ */

