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


#include <string.h>


#include "nsAETokens.h"
#include "nsAEUtils.h"

static OSErr AECoerceDescData(const AEDesc *theAEDesc, DescType typeCode, void *dataPtr, Size maximumSize);

/*----------------------------------------------------------------------------
	CreateAliasAEDesc 
	
	Create an AE descriptor for an alias
----------------------------------------------------------------------------*/

OSErr CreateAliasAEDesc(AliasHandle theAlias, AEDesc *ioDesc)
{
	char		state = HGetState((Handle)theAlias);
	OSErr	err;

	HLock((Handle)theAlias);
	err = AECreateDesc(typeAlias, *theAlias, GetHandleSize((Handle)theAlias), ioDesc);
	HSetState((Handle)theAlias, state);
	return err;
}


/*----------------------------------------------------------------------------
	GetTextFromAEDesc 
	
	Get a text handle from an AEDesc
----------------------------------------------------------------------------*/
OSErr GetTextFromAEDesc(const AEDesc *inDesc, Handle *outTextHandle)
{
	Handle	textHandle = nil;
	Size		textLength;
	OSErr	err;

	textLength = AEGetDescDataSize(inDesc);

	err = MyNewHandle(textLength, &textHandle);
	if (err != noErr) return err;
	
	MyHLock(textHandle);
	err = AECoerceDescData(inDesc, typeChar, *textHandle, textLength);
	MyHUnlock(textHandle);
	
	if (err != noErr)
		goto exit;
	
	*outTextHandle = textHandle;
	return noErr;
	
exit:
	MyDisposeHandle(textHandle);
	return err;
}


#if !TARGET_CARBON

/*----------------------------------------------------------------------------
	AEGetDescData 
	
	Get a copy of the data from the AE desc. The will attempt to coerce to the
	requested type, returning an error on failure.
----------------------------------------------------------------------------*/

OSErr AEGetDescData(const AEDesc *theAEDesc, void *dataPtr, Size maximumSize)
{
  OSErr err = noErr;
  
	if (theAEDesc->dataHandle)
	{
		Size dataLength = GetHandleSize(theAEDesc->dataHandle);
		BlockMoveData(*theAEDesc->dataHandle, dataPtr, Min(dataLength, maximumSize));
	}
	else
		err = paramErr;
		
	return err;
}

/*----------------------------------------------------------------------------
	AEGetDescDataSize 
	
	Get the size of the datahandle.
----------------------------------------------------------------------------*/
Size AEGetDescDataSize(const AEDesc *theAEDesc)
{
	Size		dataSize = 0;
	
	if (theAEDesc->dataHandle)
		dataSize = GetHandleSize(theAEDesc->dataHandle);
	
	return dataSize;
}

/*----------------------------------------------------------------------------
	AEReplaceDescData 
	
	Replace the data in the descriptor
----------------------------------------------------------------------------*/
OSErr AEReplaceDescData(DescType typeCode, const void *dataPtr, Size dataSize, AEDesc* theAEDesc)
{
	AEDisposeDesc(theAEDesc);
	return AECreateDesc(typeCode, dataPtr, dataSize, theAEDesc);
}

#endif //TARGET_CARBON

static OSErr AECoerceDescData(const AEDesc *theAEDesc, DescType typeCode, void *dataPtr, Size maximumSize)
{
	OSErr err;
	
	if (theAEDesc->descriptorType != typeCode)
	{
		AEDesc	coercedDesc = { typeNull, nil };
		err = AECoerceDesc(theAEDesc, typeCode, &coercedDesc);
		if (err != noErr) return err;
		
		err = AEGetDescData(&coercedDesc, dataPtr, maximumSize);
		AEDisposeDesc(&coercedDesc);
		return err;
	}
	else
	{
	    return AEGetDescData(theAEDesc, dataPtr, maximumSize);
	}
}

#pragma mark -


/*----------------------------------------------------------------------------
	CreateThreadAEInfo 
	
	Allocate a block for the thread info, and fill it.
----------------------------------------------------------------------------*/
OSErr CreateThreadAEInfo(const AppleEvent *event, AppleEvent *reply, TThreadAEInfoPtr *outThreadAEInfo)
{
	TThreadAEInfo	*threadAEInfo = nil;
	OSErr		err;
	
	err = MyNewBlockClear(sizeof(TThreadAEInfo), (void**)&threadAEInfo);
	if (err != noErr) return err;
	
	threadAEInfo->mAppleEvent = *event;
	threadAEInfo->mReply = *reply;
	threadAEInfo->mGotEventData = event && reply;
	threadAEInfo->mSuspendCount = nil;
	
	*outThreadAEInfo = threadAEInfo;
	return noErr;

exit:
	MyDisposeBlock(threadAEInfo);
	return err;
}


/*----------------------------------------------------------------------------
	DisposeThreadAEInfo 
	
	Dispose of the thread AE info
----------------------------------------------------------------------------*/
void DisposeThreadAEInfo(TThreadAEInfo *threadAEInfo)
{
	AE_ASSERT(threadAEInfo && threadAEInfo->mSuspendCount == 0, "Bad suspend count");
	MyDisposeBlock(threadAEInfo);
}


/*----------------------------------------------------------------------------
	SuspendThreadAE 
	
	If this if the first suspend, suspend the event. Increment the suspend count.
----------------------------------------------------------------------------*/
OSErr SuspendThreadAE(TThreadAEInfo *threadAEInfo)
{
	if (threadAEInfo == nil) return noErr;
	if (!threadAEInfo->mGotEventData) return noErr;
	
	if (threadAEInfo->mSuspendCount == 0)
	{
		OSErr	err = AESuspendTheCurrentEvent(&threadAEInfo->mAppleEvent);
		if (err != noErr) return err;
	}
	
	++ threadAEInfo->mSuspendCount;
	return noErr;
}


/*----------------------------------------------------------------------------
	ResumeThreadAE 
	
	Decrement the suspend count. If this is the last resume, resume the event.
----------------------------------------------------------------------------*/

static OSErr AddErrorCodeToReply(TThreadAEInfo *threadAEInfo, OSErr threadError)
{
	long		errorValue = threadError;
	
	if (threadError == noErr) return noErr;
	
	return AEPutParamPtr(&threadAEInfo->mReply, keyErrorNumber, typeLongInteger,  (Ptr)&errorValue, sizeof(long));
}


/*----------------------------------------------------------------------------
	ResumeThreadAE 
	
	Decrement the suspend count. If this is the last resume, resume the event.
----------------------------------------------------------------------------*/
OSErr ResumeThreadAE(TThreadAEInfo *threadAEInfo, OSErr threadError)
{
	if (threadAEInfo == nil) return noErr;
	if (!threadAEInfo->mGotEventData) return noErr;

	-- threadAEInfo->mSuspendCount;
	
	AddErrorCodeToReply(threadAEInfo, threadError);
	
	if (threadAEInfo->mSuspendCount == 0)
		return AEResumeTheCurrentEvent(&threadAEInfo->mAppleEvent, &threadAEInfo->mReply, (AEEventHandlerUPP)kAENoDispatch, 0);

	return noErr;
}


#pragma mark -

/*----------------------------------------------------------------------------
	Copy ctor
	
	this can throw 
	
----------------------------------------------------------------------------*/

StAEDesc::StAEDesc(const StAEDesc& rhs)
{
	ThrowIfOSErr(AEDuplicateDesc(&rhs, this));
}

/*----------------------------------------------------------------------------
	operator =
	
	this can throw 
	
----------------------------------------------------------------------------*/

StAEDesc& StAEDesc::operator= (const StAEDesc& rhs)
{
	ThrowIfOSErr(AEDuplicateDesc(&rhs, this));
	return *this;
}


/*----------------------------------------------------------------------------
	Data getters
	
	These should try to coerce when necessary also.
----------------------------------------------------------------------------*/

Boolean StAEDesc::GetBoolean()
{
    Boolean result = false;
    OSErr err = ::AECoerceDescData(this, typeBoolean, &result, sizeof(result));
    if (err != noErr)
        ThrowOSErr(errAECoercionFail);
    return result;
}

SInt16 StAEDesc::GetShort()
{
    SInt16 result = 0;
    OSErr err = ::AECoerceDescData(this, typeShortInteger, &result, sizeof(result));
    if (err != noErr)
        ThrowOSErr(errAECoercionFail);
    return result;
}

SInt32 StAEDesc::GetLong()
{
    SInt32 result = 0;
    OSErr err = ::AECoerceDescData(this, typeLongInteger, &result, sizeof(result));
    if (err != noErr)
        ThrowOSErr(errAECoercionFail);
    return result;
}

DescType StAEDesc::GetEnumType()
{
    DescType result = typeNull;
    OSErr err = ::AECoerceDescData(this, typeEnumeration, &result, sizeof(result));
    if (err != noErr)
        ThrowOSErr(errAECoercionFail);
    return result;
}

void StAEDesc::GetRect(Rect& outData)
{
    OSErr err = ::AECoerceDescData(this, typeQDRectangle, &outData, sizeof(Rect));
    if (err != noErr)
        ThrowOSErr(errAECoercionFail);
}


void StAEDesc::GetRGBColor(RGBColor& outData)
{
    OSErr err = ::AECoerceDescData(this, typeRGBColor, &outData, sizeof(RGBColor));
    if (err != noErr)
        ThrowOSErr(errAECoercionFail);
}

void StAEDesc::GetLongDateTime(LongDateTime& outDateTime)
{
    OSErr err = ::AECoerceDescData(this, typeLongDateTime, &outDateTime, sizeof(LongDateTime));
    if (err != noErr)
        ThrowOSErr(errAECoercionFail);
}

void StAEDesc::GetFileSpec(FSSpec &outFileSpec)
{
    OSErr err = ::AECoerceDescData(this, typeFSS, &outFileSpec, sizeof(FSSpec));
    if (err != noErr)
        ThrowOSErr(errAECoercionFail);
}

void StAEDesc::GetCString(char *outString, short maxLen)
{
	if (descriptorType == typeChar)
	{
		long dataSize = GetDataSize();
		dataSize = Min(dataSize, maxLen-1);
		if (AEGetDescData(this, outString, dataSize) == noErr)
    		outString[dataSize] = '\0';
	}
	else
	{
		StAEDesc tempDesc;
		if (::AECoerceDesc(this, typeChar, &tempDesc) == noErr)
		{
			long dataSize = tempDesc.GetDataSize();
    		dataSize = Min(dataSize, maxLen-1);
    		if (AEGetDescData(&tempDesc, outString, dataSize) == noErr)
        		outString[dataSize] = '\0';
		}
		else
			ThrowOSErr(errAECoercionFail);
	}
}

void StAEDesc::GetPString(Str255 outString)
{
	if (descriptorType == typeChar)
	{
		long stringLen = GetDataSize();
		if (stringLen > 255)
			stringLen = 255;
		AEGetDescData(this, outString+1, stringLen);
		outString[0] = stringLen;
	}
	else
	{
		StAEDesc	tempDesc;
		if (::AECoerceDesc(this, typeChar, &tempDesc) == noErr)
		{
			long stringLen = tempDesc.GetDataSize();
			if (stringLen > 255)
				stringLen = 255;
			AEGetDescData(&tempDesc, outString+1, stringLen);
			outString[0] = stringLen;
		}
		else
			ThrowOSErr(errAECoercionFail);
	}
}

Handle StAEDesc::GetTextHandle()
{
    Handle data = nil;
    
	if (descriptorType == typeChar)
	{
	    Size dataSize = GetDataSize();
	    data = ::NewHandle(dataSize);
	    if (data == NULL)
	        ThrowOSErr(memFullErr);
        ::HLock(data);
        ::AEGetDescData(this, *data, dataSize);
        ::HUnlock(data);
	}
	else
	{
    	StAEDesc tempDesc;
		if (::AECoerceDesc(this, typeChar, &tempDesc) == noErr)
		    data = tempDesc.GetTextHandle();
		else
		    ThrowOSErr(errAECoercionFail);
	}
	
	return data;
}


#pragma mark -

/*----------------------------------------------------------------------------
	GetFirstNonListToken
	
	Note: make sure the result descriptor is {typeNull, nil} when it is passed in to this
	
----------------------------------------------------------------------------*/
OSErr AEListUtils::GetFirstNonListToken(const AEDesc *token, AEDesc *result)
{
	OSErr 		err = noErr;
	AEDesc 		tempToken = { typeNull, nil };
	AEKeyword	keyword;
	long			numItems;
	long 			itemNum;
	
	if (result->descriptorType == typeNull)
	{
		if (TokenContainsTokenList(token) == false)
		{
			err = AEDuplicateDesc(token, result);
		}
		else
		{
			err = AECountItems(token, &numItems);
			
			for (itemNum = 1; itemNum <= numItems; itemNum++)
			{
				err = AEGetNthDesc((AEDescList *)token, itemNum, typeWildCard, &keyword, &tempToken);
				if (err != noErr)
					goto CleanUp;
					
				err = GetFirstNonListToken(&tempToken, result);				
				if ((err != noErr) || (result->descriptorType != typeNull))
					break;
					
				AEDisposeDesc(&tempToken);
			}
		}
	}
	
CleanUp:
	if (err != noErr)
		AEDisposeDesc(result);
		
	AEDisposeDesc(&tempToken);
	return err;
}

/*----------------------------------------------------------------------------
	FlattenAEList
	
----------------------------------------------------------------------------*/
OSErr AEListUtils::FlattenAEList(AEDescList *deepList, AEDescList *flatList)
{
	OSErr		err = noErr;
	AEDesc		item	= {typeNull, nil};
	AEKeyword	keyword;
	long			itemNum;
	long			numItems;

	err = AECountItems(deepList, &numItems);
	if (err != noErr)
		goto CleanUp;
	
	for (itemNum = 1; itemNum <= numItems; itemNum++)
	{
		err = AEGetNthDesc(deepList, itemNum, typeWildCard, &keyword, &item);
		if (err != noErr)
			goto CleanUp;
			
		if (item.descriptorType == typeAEList)
			err = FlattenAEList(&item, flatList);
		else
			err = AEPutDesc(flatList, 0L, &item);
		
		if (err != noErr)
			goto CleanUp;
			
		AEDisposeDesc(&item);
	}

CleanUp:
	if (err != noErr)
		AEDisposeDesc(flatList);

	AEDisposeDesc(&item);
	
	return err;
}


#pragma mark -

/*----------------------------------------------------------------------------
	StEventSuspender
	
----------------------------------------------------------------------------*/
StEventSuspender::StEventSuspender(const AppleEvent *appleEvent, AppleEvent *reply, Boolean deleteData)
:	mThreadAEInfo(nil)
,	mDeleteData(deleteData)
{
	ThrowIfOSErr(CreateThreadAEInfo(appleEvent, reply, &mThreadAEInfo));
}


/*----------------------------------------------------------------------------
	~StEventSuspender
	
----------------------------------------------------------------------------*/
StEventSuspender::~StEventSuspender()
{
	if (mDeleteData)
		DisposeThreadAEInfo(mThreadAEInfo);
}

/*----------------------------------------------------------------------------
	SuspendEvent
	
----------------------------------------------------------------------------*/
void StEventSuspender::SuspendEvent()
{
	ThrowIfOSErr(SuspendThreadAE(mThreadAEInfo));
}


/*----------------------------------------------------------------------------
	ResumeEvent
	
----------------------------------------------------------------------------*/
void StEventSuspender::ResumeEvent()
{
	ThrowIfOSErr(ResumeThreadAE(mThreadAEInfo, noErr));
}



#pragma mark -


/*----------------------------------------------------------------------------
	StHandleHolder
	
----------------------------------------------------------------------------*/
StHandleHolder::StHandleHolder(Handle inHandle)
:	mHandle(inHandle)
,	mLockCount(0)
{
}


/*----------------------------------------------------------------------------
	~StHandleHolder
	
----------------------------------------------------------------------------*/
StHandleHolder::~StHandleHolder()
{
	if (mHandle)
		DisposeHandle(mHandle);
}


/*----------------------------------------------------------------------------
	operator=
	
----------------------------------------------------------------------------*/
StHandleHolder& StHandleHolder::operator=(Handle rhs)
{
	AE_ASSERT(mLockCount == 0, "Bad lock count");
	mLockCount = 0;
	
	if (mHandle)
		DisposeHandle(mHandle);
		
	mHandle = rhs;
	return *this;
}

/*----------------------------------------------------------------------------
	Lock
	
----------------------------------------------------------------------------*/
void StHandleHolder::Lock()
{
	ThrowIfNil(mHandle);
	if (++mLockCount > 1) return;
	mOldHandleState = HGetState(mHandle);
	HLock(mHandle);
}

/*----------------------------------------------------------------------------
	Unlock
	
----------------------------------------------------------------------------*/
void StHandleHolder::Unlock()
{
	ThrowIfNil(mHandle);
	AE_ASSERT(mLockCount > 0, "Bad lock count");
	if (--mLockCount == 0)
		HSetState(mHandle, mOldHandleState);
}


#pragma mark -


/*----------------------------------------------------------------------------
	StAEListIterator
	
----------------------------------------------------------------------------*/
AEListIterator::AEListIterator(AEDesc *token)
:	mNumItems(0)
,	mCurItem(0)
,	mIsListDesc(false)
{
	ThrowIfNil(token);
	mListToken = *token;
	mIsListDesc = AEListUtils::TokenContainsTokenList(&mListToken);
	if (mIsListDesc)
	{
		ThrowIfOSErr(::AECountItems(token, &mNumItems));
		mCurItem = 1;
	}
	else
	{
		mCurItem = 0;
		mNumItems = 1;
	}
}


/*----------------------------------------------------------------------------
	Next
	
----------------------------------------------------------------------------*/
Boolean AEListIterator::Next(AEDesc* outItemData)
{
	if (mIsListDesc)
	{
		AEKeyword	keyword;
		
		if (mCurItem == 0 || mCurItem > mNumItems)
			return false;
		
		ThrowIfOSErr(::AEGetNthDesc(&mListToken, mCurItem, typeWildCard, &keyword, outItemData));
		
		// what about nested lists?
		AE_ASSERT(!AEListUtils::TokenContainsTokenList(outItemData), "Nested list found");
	}
	else
	{
		if (mCurItem > 0)
			return false;
		
		ThrowIfOSErr(::AEDuplicateDesc(&mListToken, outItemData));
	}
	
	mCurItem ++;
	return true;
}



#pragma mark -

/*----------------------------------------------------------------------------
	CheckForUnusedParameters 
	
	Check to see if there exist any additional required parameters in the Apple Event.
	If so, return an err to the calling routine, because we didn't extract them all.
----------------------------------------------------------------------------*/
OSErr  CheckForUnusedParameters(const AppleEvent* appleEvent)
{
	OSErr		err 		= noErr;
	
	DescType	actualType	= typeNull;
	Size		actualSize	= 0L;
	
	err = AEGetAttributePtr(appleEvent, 
									keyMissedKeywordAttr, 
									typeWildCard,
									&actualType, 
									nil, 
									0, 
									&actualSize);
									
	if (err == errAEDescNotFound)
		err = noErr;
	else
		err = errAEParamMissed;
		
	return err;
}

/*----------------------------------------------------------------------------
	PutReplyErrorNumber 
	
	If a reply is expected, the err number is returned in the reply parameter.
----------------------------------------------------------------------------*/
OSErr  PutReplyErrorNumber(AppleEvent* reply, long errorNumber)
{
	OSErr err = noErr;
	
	if (reply->dataHandle != nil && errorNumber != noErr)
	{
		err = AEPutParamPtr(reply, 
									 keyErrorNumber, 
									 typeLongInteger, 
									 (Ptr)&errorNumber, 
									 sizeof(long));
	}
	return err;
}

/*----------------------------------------------------------------------------
	PutReplyErrorMessage 
	
	If a reply is expected, the err message is inserted into the reply parameter.
----------------------------------------------------------------------------*/
OSErr  PutReplyErrorMessage(AppleEvent* reply, char *message)
{
	OSErr err = noErr;
	
	if (reply->dataHandle != nil && message != NULL)
	{
		err = AEPutParamPtr(reply, 
									 keyErrorString, 
									 typeChar, 
									 (Ptr)message, 
									 strlen(message));
	}
	return err;
}


/*----------------------------------------------------------------------------
	GetObjectClassFromAppleEvent 
	
	This is used to extract the type of object that an appleevent is supposed
	to work with. In the Core events, this includes:
		count <reference> each <typeOfObject>
		make new <typeOfObject>
		save <reference> in <alias> as <typeOfObject>
---------------------------------------------------------------------------*/
OSErr GetObjectClassFromAppleEvent(const AppleEvent *appleEvent, DescType *objectClass)
{
	OSErr	err     = noErr;
	OSType	typeCode;					// should be typeType
	long		actualSize;
	
	// Get the class of object that we will count

	err = AEGetParamPtr(appleEvent, 
								 keyAEObjectClass, 
								 typeType, 
								 &typeCode,
								 (Ptr)objectClass, 
								 sizeof(DescType), 
								 &actualSize);
								 
	if (typeCode != typeType) 
		err = errAECoercionFail;
	
	return err;
}

#pragma mark -

/*----------------------------------------------------------------------------
	DescToPString 
	
	Converts descriptor dataHandle  to a pascal string
	
---------------------------------------------------------------------------*/
OSErr DescToPString(const AEDesc* desc, Str255 aPString, short maxLength)
{
	if (desc->descriptorType == typeChar)
	{
		long stringLen = AEGetDescDataSize(desc);
		if (stringLen > maxLength)
			stringLen = maxLength;
		AEGetDescData(desc, aPString+1, stringLen);
		aPString[0] = stringLen;
	}
	else
	{
    	StAEDesc tempDesc;
		if (AECoerceDesc(desc, typeChar, &tempDesc) == noErr) {
    		long stringLen = tempDesc.GetDataSize();
    		if (stringLen > maxLength)
    			stringLen = maxLength;
    		AEGetDescData(&tempDesc, aPString+1, stringLen);
    		aPString[0] = stringLen;
		} else
			return errAECoercionFail;
	}
	return noErr;
}


/*----------------------------------------------------------------------------
	DescToCString 
	
	Converts descriptor dataHandle  to a C string
	
--------------------------------------------------------------------------- */
OSErr DescToCString(const AEDesc* desc, CStr255 aCString, short maxLength)
{
	if (desc->descriptorType == typeChar)
	{
	    long stringLen = AEGetDescDataSize(desc);
	    if (stringLen >= maxLength)
	        stringLen = maxLength - 1;
		if (AEGetDescData(desc, aCString, stringLen) == noErr)
    		aCString[stringLen] = '\0';
    	else
			return errAECoercionFail;
	}
	else
	{
    	StAEDesc tempDesc;
		if (AECoerceDesc(desc, typeChar, &tempDesc) == noErr) {
    	    long stringLen = AEGetDescDataSize(&tempDesc);
    	    if (stringLen >= maxLength)
    	        stringLen = maxLength - 1;
    		if (AEGetDescData(&tempDesc, aCString, stringLen) == noErr)
        		aCString[stringLen] = '\0';
        	else
    			return errAECoercionFail;
		} else
			return errAECoercionFail;
	}
	return noErr;
}

//----------------------------------------------------------------------------------
//	Converts descriptor dataHandle  to a DescType
//----------------------------------------------------------------------------------

OSErr DescToDescType(const AEDesc *desc, DescType *descType)
{
	if (AEGetDescDataSize(desc) == sizeof(DescType))
	    return AEGetDescData(desc, descType, sizeof(DescType));
	else
		return errAECoercionFail;
}

//----------------------------------------------------------------------------------
//	Converts descriptor dataHandle  to a boolean
//----------------------------------------------------------------------------------

OSErr DescToBoolean(const AEDesc* desc, Boolean* aBoolean)
{
    return AECoerceDescData(desc, typeBoolean, aBoolean, sizeof(Boolean));
}

//----------------------------------------------------------------------------------
//	Converts descriptor dataHandle  to a Fixed
//----------------------------------------------------------------------------------

OSErr DescToFixed(const  AEDesc* desc, Fixed* aFixed)
{
    return AECoerceDescData(desc, typeFixed, aFixed, sizeof(Fixed));
}

//----------------------------------------------------------------------------------
//	Converts descriptor dataHandle  to a float
//----------------------------------------------------------------------------------

OSErr DescToFloat(const  AEDesc* desc, float* aFloat)
{
    return AECoerceDescData(desc, typeFloat, aFloat, sizeof(float));
}

//----------------------------------------------------------------------------------
//	Converts descriptor dataHandle  to a long
//----------------------------------------------------------------------------------

OSErr DescToLong(const AEDesc* desc, long* aLong)
{
    return AECoerceDescData(desc, typeLongInteger, aLong, sizeof(long));
}

//----------------------------------------------------------------------------------
//	Converts descriptor dataHandle  to a RGBColor 
//----------------------------------------------------------------------------------

OSErr DescToRGBColor(const  AEDesc* desc, RGBColor* aRGBColor)
{
    return AECoerceDescData(desc, typeRGBColor, aRGBColor, sizeof(RGBColor));
}

//----------------------------------------------------------------------------------
//	Converts descriptor dataHandle  to a short.
//----------------------------------------------------------------------------------

OSErr DescToShort(const  AEDesc* desc, short* aShort)
{
    return AECoerceDescData(desc, typeShortInteger, aShort, sizeof(short));
}

//----------------------------------------------------------------------------------
//	Copies descriptor dataHandle to another handle, if its text
//----------------------------------------------------------------------------------

OSErr DescToTextHandle(const AEDesc* desc, Handle *text)
{
    Handle data = nil;
    
	if (desc->descriptorType == typeChar)
	{
	    Size dataSize = ::AEGetDescDataSize(desc);
	    data = ::NewHandle(dataSize);
	    if (data == NULL)
	        return memFullErr;
        ::HLock(data);
        ::AEGetDescData(desc, *data, dataSize);
        ::HUnlock(data);
	}
	else
	{
    	StAEDesc tempDesc;
		if (::AECoerceDesc(desc, typeChar, &tempDesc) == noErr)
		    data = tempDesc.GetTextHandle();
		else
		    return errAECoercionFail;
	}
	*text = data;
	return noErr;
}

//----------------------------------------------------------------------------------
//	Converts descriptor dataHandle  to a Rectangle 
//----------------------------------------------------------------------------------

OSErr DescToRect(const  AEDesc* desc, Rect* aRect)
{
    return AECoerceDescData(desc, typeRectangle, aRect, sizeof(Rect));
}

//----------------------------------------------------------------------------------
//	Converts descriptor dataHandle  to a Point 
//----------------------------------------------------------------------------------

OSErr DescToPoint(const  AEDesc* desc, Point* aPoint)
{
    return AECoerceDescData(desc, typeQDPoint, aPoint, sizeof(Point));
}

#pragma mark -

/*----------------------------------------------------------------------------
	NormalizeAbsoluteIndex 
	
	 Handles formAbsolutePosition resolution
	
---------------------------------------------------------------------------*/
OSErr NormalizeAbsoluteIndex(const AEDesc *keyData, long *index, long maxIndex, Boolean *isAllItems)
{
	OSErr err = noErr;
	
	*isAllItems = false;								// set to true if we receive kAEAll constant
	
	// Extract the formAbsolutePosition data, either a integer or a literal constant
	
	switch (keyData->descriptorType)
	{
		case typeLongInteger:						// positve or negative index
			if (DescToLong(keyData, index) != noErr)
				return errAECoercionFail;

			if (*index < 0)							// convert a negative index from end of list to a positive index from beginning of list
				*index = maxIndex + *index + 1;
			break;
			
	   case typeAbsoluteOrdinal:										// 'abso'
	   		DescType		ordinalDesc;
			if (DescToDescType((AEDesc*)keyData, &ordinalDesc) != noErr)
				ThrowOSErr(errAECoercionFail);
			
			switch (ordinalDesc)
			{
			   	case kAEFirst:
			   		*index = 1;
					break;
					
				case kAEMiddle:
					*index = (maxIndex >> 1) + (maxIndex % 2);
					break;
						
			   	case kAELast:
			   		*index = maxIndex;
					break;
						
			   	case kAEAny:
					*index = (TickCount() % maxIndex) + 1;		// poor man's random
					break;
						
			   	case kAEAll:
			   		*index = 1;
			   		*isAllItems = true;
					break;
			}
			break;

		default:
			return errAEWrongDataType;
			break;
	}

	// range-check the new index number
	if ((*index < 1) || (*index > maxIndex))					
	{
		err = errAEIllegalIndex;
	}

	return err;
}


//----------------------------------------------------------------------------------
// Handles formRange resolution of boundary objects

OSErr ProcessFormRange(AEDesc *keyData, AEDesc *start, AEDesc *stop)
{
	OSErr 		err = noErr;
	StAEDesc 		rangeRecord;
	StAEDesc		ospec;
	
	// coerce the range record data into an AERecord 
	
 	err = AECoerceDesc(keyData, typeAERecord, &rangeRecord);
 	if (err != noErr)
 		return err;
	 
	// object specifier for first object in the range
	err = AEGetKeyDesc(&rangeRecord, keyAERangeStart, typeWildCard, &ospec);
 	if (err == noErr && ospec.descriptorType == typeObjectSpecifier)
 		err = AEResolve(&ospec, kAEIDoMinimum, start);
 		
 	if (err != noErr)
 		return err;
 		
	ospec.Clear();
		
	// object specifier for last object in the range
	
	err = AEGetKeyDesc(&rangeRecord, keyAERangeStop, typeWildCard, &ospec);
  	if (err == noErr && ospec.descriptorType == typeObjectSpecifier)
 		err = AEResolve(&ospec, kAEIDoMinimum, stop);

	return err;
}
