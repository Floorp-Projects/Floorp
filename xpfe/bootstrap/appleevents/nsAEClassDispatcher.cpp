/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Simon Fraser <sfraser@netscape.com>
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


#include "nsAEUtils.h"

#include "nsAEGenericClass.h"
#include "nsAEClassDispatcher.h"


/*----------------------------------------------------------------------------
	AEDispatchHandler 
	
	Ownership of the hanlder passes to this object
----------------------------------------------------------------------------*/

AEDispatchHandler::AEDispatchHandler(DescType handlerClass, AEGenericClass* handler, Boolean deleteOnRemove /* = true*/ )
:	mDeleteHandler(deleteOnRemove)
,	mHandlerClass(handlerClass)
,	mHandler(handler)
{
	AE_ASSERT(mHandler, "No handler");
}


/*----------------------------------------------------------------------------
	AEDispatchHandler 
	
----------------------------------------------------------------------------*/
AEDispatchHandler::~AEDispatchHandler()
{
	if (mDeleteHandler)
		delete mHandler;
}


/*----------------------------------------------------------------------------
	DispatchEvent 
	
----------------------------------------------------------------------------*/
void AEDispatchHandler::DispatchEvent(				AEDesc *			token,
											const AppleEvent *	appleEvent,
											AppleEvent *		reply)
{
	AE_ASSERT(mHandler, "No handler");
	mHandler->DispatchEvent(token, appleEvent, reply);
}

/*----------------------------------------------------------------------------
	GetProperty 
	
----------------------------------------------------------------------------*/
void AEDispatchHandler::GetProperty(				DescType			desiredClass,
											const AEDesc*		containerToken,
											DescType			containerClass,
											DescType			keyForm,
											const AEDesc*		keyData,
											AEDesc*			resultToken)
{
	AE_ASSERT(mHandler, "No handler");
	mHandler->GetProperty(desiredClass, containerToken, containerClass, keyForm, keyData, resultToken);
}

/*----------------------------------------------------------------------------
	GetDataFromListOrObject 
	
----------------------------------------------------------------------------*/
void	AEDispatchHandler::GetDataFromListOrObject(		const AEDesc *		tokenOrTokenList,
											AEDesc *			desiredTypes,
											AEDesc *			data)
{
	AE_ASSERT(mHandler, "No handler");
	mHandler->GetDataFromListOrObject(tokenOrTokenList, desiredTypes, data);
}


/*----------------------------------------------------------------------------
	GetItemFromContainer 
	
----------------------------------------------------------------------------*/
void	AEDispatchHandler::GetItemFromContainer(			DescType			desiredClass,
												const AEDesc*		containerToken,
												DescType			containerClass, 
												DescType			keyForm,
												const AEDesc*		keyData,
												AEDesc*			resultToken)
{
	AE_ASSERT(mHandler, "No handler");
	mHandler->GetItemFromContainer(desiredClass, containerToken, containerClass, keyForm, keyData, resultToken);
}


/*----------------------------------------------------------------------------
	CompareObjects 
	
----------------------------------------------------------------------------*/
void AEDispatchHandler::CompareObjects(					DescType			comparisonOperator,
												const AEDesc *		object,
												const AEDesc *		descriptorOrObject,
												Boolean *			result)
{
	AE_ASSERT(mHandler, "No handler");
	mHandler->CompareObjects(comparisonOperator, object, descriptorOrObject, result);
}



/*----------------------------------------------------------------------------
	CountObjects 
	
----------------------------------------------------------------------------*/
void AEDispatchHandler::CountObjects(					DescType 		 	desiredType,
												DescType 		 	containerClass,
												const AEDesc *		container,
								   				long *			result)
{
	AE_ASSERT(mHandler, "No handler");
	mHandler->CountObjects(desiredType, containerClass, container, result);
}


/*----------------------------------------------------------------------------
	CreateSelfSpecifier 
	
----------------------------------------------------------------------------*/
void AEDispatchHandler::CreateSelfSpecifier(				const AEDesc *		token,
												AEDesc *			outSpecifier)
{
	AE_ASSERT(mHandler, "No handler");
	mHandler->CreateSelfSpecifier(token, outSpecifier);
}

#pragma mark -


/*----------------------------------------------------------------------------
	AEDispatchTree 
	
----------------------------------------------------------------------------*/
AEDispatchTree::AEDispatchTree()
:	mTree(nil)
{
	
	mTree = PatriciaInitTree(8 * sizeof(DescType));
	ThrowIfNil(mTree);
}


/*----------------------------------------------------------------------------
	~AEDispatchTree 
	
----------------------------------------------------------------------------*/
AEDispatchTree::~AEDispatchTree()
{
	if (mTree)
		PatriciaFreeTree(mTree, FreeDispatchTreeNodeData, this);
}


/*----------------------------------------------------------------------------
	InsertHandler 
	
----------------------------------------------------------------------------*/
void AEDispatchTree::InsertHandler(DescType handlerClass, AEGenericClass *handler, Boolean isDuplicate /* = false */)
{
	AEDispatchHandler	*newHandler = new AEDispatchHandler(handlerClass, handler, !isDuplicate);
	unsigned char		key[5] = {0};				
	int				result;
	
	*(DescType *)key = handlerClass;
	
	result = PatriciaInsert(mTree, nil, key, newHandler, nil);
	if (result == kDuplicateKeyError || result == 1)
	{
		ThrowIfOSErr(kDuplicateKeyError);
	}
	else if (result != 0)
	{
		ThrowIfOSErr(result);
	}
}


/*----------------------------------------------------------------------------
	FindHandler 
	

----------------------------------------------------------------------------*/
AEDispatchHandler* AEDispatchTree::FindHandler(DescType handlerClass)
{
	AEDispatchHandler*	foundClass = nil;
	unsigned char		key[5] = {0};				
	
	*(DescType *)key = handlerClass;

	(void)PatriciaSearch(mTree, key, (void**)&foundClass);
	
	return foundClass;
}

/*----------------------------------------------------------------------------
	ReplaceDispatchTreeNode 
	
	static
	
	if this ever gets called, it means we tried to insert a node for a duplicate class,
	which is an error. So return an error. We don't want to throw because the
	patricia code may not be exception-safe.
----------------------------------------------------------------------------*/
int AEDispatchTree::ReplaceDispatchTreeNode(void *nodeData, unsigned char *key, void *replaceData)
{
	return kDuplicateKeyError;
}

/*----------------------------------------------------------------------------
	FreeDispatchTreeNodeData 
	
	static
----------------------------------------------------------------------------*/
int AEDispatchTree::FreeDispatchTreeNodeData(void *nodeData, unsigned char *key, void *refCon)
{
	AEDispatchTree*	dispatchTree = reinterpret_cast<AEDispatchTree *>(refCon);
	AEDispatchHandler*	handler = reinterpret_cast<AEDispatchHandler *>(nodeData);
	
	delete handler;
	return 0;
}

