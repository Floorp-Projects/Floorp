/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Douglas Turner <dougt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "PatchableAppleSingle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

OSErr PAS_EncodeFile(FSSpec *inSpec, FSSpec *outSpec);
OSErr PAS_DecodeFile(FSSpec *inSpec, FSSpec *outSpec);

OSErr PAS_encodeResource(FSSpec *inFile, short outRefNum);
OSErr PAS_decodeResource(PASEntry *entry, FSSpec *outFile, short inRefNum);

OSErr PAS_encodeMisc(FSSpec *inFile, short outRefNum);
OSErr PAS_decodeMisc(PASEntry *entry, FSSpec *outFile, short inRefNum);

OSErr PAS_encodeData(FSSpec *inFile, short outRefNum);
OSErr PAS_decodeData(PASEntry *entry, FSSpec *outFile, short inRefNum);

OSErr PAS_encodeHeader(short refnum);
OSErr PAS_decodeHeader(short refNum, PASHeader *header);


unsigned long PAS_getDataSize(FSSpec *spec);
short PAS_getResourceID(Handle resource);

OSErr PAS_flattenResource(ResType type, short *ids, long count, short source, short dest);
OSErr PAS_unflattenResource(PASResource *pasRes, Ptr buffer);

void PAS_sortTypes(short sourceRefNum, ResType **resTypePtr, long *count);
void PAS_sortIDs(short sourceRefNum, OSType theType, short **IdPtr, long *count);
void PAS_bubbleSortResType(ResType *types, long count);
void PAS_bubbleSortIDS(short *ids, long count);

OSErr PAS_EncodeFile(FSSpec *inSpec, FSSpec *outSpec)
{
	OSErr		err;
	short		outRefNum;
	
	PASEntry 	dataEntry, miscEntry, resourceEntry;
	long		sizeOfEntry;


	if (inSpec == NULL || outSpec == NULL)
		return paramErr;
		

	memset(&dataEntry, 0, sizeof(PASEntry));
	memset(&miscEntry, 0, sizeof(PASEntry));
	memset(&resourceEntry, 0, sizeof(PASEntry));
	
	FSpDelete( outSpec ) ;
	
	err = FSpCreate( outSpec, kCreator, kType ,smSystemScript );
	
	if (err != noErr) 	return err;
	
	
	err = FSpOpenDF(outSpec, fsRdWrPerm, &outRefNum);
	
	if (err != noErr)  goto error;
	

	// Write Out Header
		
	err = PAS_encodeHeader(outRefNum);
	if (err != noErr)  goto error;
	
	/* Why am I using three (3)?
		
		E stand for entry.
		
	   The data for the entry is after the THREE headers 
	
		|---------|----|----|----|---------------------->
		   header    E    E    E
	
	*/
	
	
	// Write Out Data Entry 
	dataEntry.entryID		=	ePas_Data;
	dataEntry.entryLength	=	PAS_getDataSize(inSpec);
	dataEntry.entryOffset	=	sizeof(PASHeader) + (3 * sizeof(PASEntry));
	
	sizeOfEntry			=	sizeof(PASEntry);
	if(dataEntry.entryLength < 0)
	{
		err	= dataEntry.entryLength;
		goto error;
	}
	
	err = FSWrite(outRefNum, &sizeOfEntry, &dataEntry);
	if (err != noErr) 	goto error;
	
	
	
	// Write Out Misc Entry 
	miscEntry.entryID		=	ePas_Misc;
	miscEntry.entryLength	=	sizeof(PASMiscInfo);
	miscEntry.entryOffset	=	sizeof(PASHeader) + (3 * sizeof(PASEntry)) + dataEntry.entryLength;

	sizeOfEntry			=	sizeof(PASEntry);
	err = FSWrite(outRefNum, &sizeOfEntry, &miscEntry);
	if (err != noErr) 	goto error;
	
	
	// Write Out Resource Entry 
	resourceEntry.entryID		=	ePas_Resource;
	resourceEntry.entryLength	=	-1;
	resourceEntry.entryOffset	=	sizeof(PASHeader) + (3 * sizeof(PASEntry)) + dataEntry.entryLength + miscEntry.entryLength;


	sizeOfEntry			=	sizeof(PASEntry);
	err = FSWrite(outRefNum, &sizeOfEntry, &resourceEntry);
	if (err != noErr) 	goto error;
	
	err =  PAS_encodeData(inSpec, outRefNum);	
	if (err != noErr) 	goto error;
	
	
	err =  PAS_encodeMisc(inSpec, outRefNum);	
	if (err != noErr) 	goto error;
	
	err =  PAS_encodeResource(inSpec, outRefNum);	
	
	if (err == kResFileNotOpened)
	{
		// there was no resource fork
		err = noErr;
	}
	else if (err != noErr)
	{
		goto error;
	}
		
	FSClose(outRefNum);
	
	return noErr;
	
	
	
error:
		
		
	if (outRefNum != kResFileNotOpened)
	{
		FSClose(outRefNum);
	}
		
	FSpDelete( outSpec ) ;
	 
	return err;

}



OSErr PAS_DecodeFile(FSSpec *inSpec, FSSpec *outSpec)
{
	OSErr		err;
	short		inRefNum;
	
	PASHeader	header;
	
	PASEntry 	dataEntry, miscEntry, resourceEntry;
	long		sizeOfEntry;

	if (inSpec == NULL || outSpec == NULL)
		return paramErr;
		
		
	FSpDelete( outSpec ) ;
	
	err = FSpCreate( outSpec, kCreator, kType ,smSystemScript );
	
	if (err != noErr) 	return err;
	
	
	
	err = FSpOpenDF(inSpec, fsRdPerm, &inRefNum);
	
	if (err != noErr)  goto error;

	
	// Read Header 
		
	err	= PAS_decodeHeader(inRefNum, &header);
	if (err != noErr)  goto error;
	
	if(	header.magicNum != PAS_MAGIC_NUM ||
		header.versionNum != PAS_VERSION)
	{
		err = -1;
		goto error;
	}
	
	
	
	// Read Data Entry 
	
	
	err = SetFPos(inRefNum, fsFromStart, sizeof(PASHeader));
	if (err != noErr) 	goto error;
	
	sizeOfEntry			=	sizeof(PASEntry);

	err = FSRead(inRefNum, &sizeOfEntry, &dataEntry);
	if (err != noErr) 	goto error;
	
	
	
	
	// Read Misc Entry 
	
	
	err = SetFPos(inRefNum, fsFromStart, (sizeof(PASHeader) + sizeof(PASEntry)));
	if (err != noErr) 	goto error;
	
	sizeOfEntry			=	sizeof(PASEntry);

	err = FSRead(inRefNum, &sizeOfEntry, &miscEntry);
	if (err != noErr) 	goto error;
	

	// Read Resource Entry 
	
	
	err = SetFPos(inRefNum, fsFromStart, (sizeof(PASHeader) + (2 * sizeof(PASEntry)))) ;
	if (err != noErr) 	goto error;
	
	sizeOfEntry			=	sizeof(PASEntry);

	err = FSRead(inRefNum, &sizeOfEntry, &resourceEntry);
	if (err != noErr) 	goto error;





	err =  PAS_decodeData(&dataEntry, outSpec, inRefNum);	
	if (err != noErr) 	goto error;

	err =  PAS_decodeMisc(&miscEntry, outSpec, inRefNum);
	if (err != noErr) 	goto error;

	err =  PAS_decodeResource(&resourceEntry, outSpec, inRefNum);	
	if (err == kResFileNotOpened)
	{
		// there was no resource fork
		err = noErr;
	}
	else if (err != noErr)
	{
		goto error;
	}

	
	FSClose(inRefNum);
	
	return noErr;
	
	
	
error:

	if (inRefNum != kResFileNotOpened)
	{
		FSClose(inRefNum);
	}
		
	FSpDelete( outSpec ) ;
	
	return err;
	
}




#pragma mark -


OSErr PAS_encodeResource(FSSpec *inFile, short outRefNum)
{
	OSErr 			err;	
	short			inRefNum;
	PASResFork		resInfo;
	SInt32			currentWrite;
	
	ResType 		*resTypes;
	long			typeCount;
	
	short			*ids;
	long			idCount;
	
	short			oldResFile;
	
	oldResFile=CurResFile();
	inRefNum = FSpOpenResFile(inFile, fsRdPerm);
	if (inRefNum < noErr)	return inRefNum;

	UseResFile(inRefNum);
		
	memset(&resInfo, 0, sizeof(PASResFork));   
	
	PAS_sortTypes(inRefNum, &resTypes, &typeCount);

	resInfo.NumberOfTypes	=	typeCount;
	
	currentWrite	= sizeof(PASResFork);
	
	err = FSWrite(outRefNum, &currentWrite, &resInfo);
	if (err != noErr)	return err;
	
	for (typeCount = 0; ((typeCount < resInfo.NumberOfTypes) && (err == noErr)); typeCount++)
	{
		PAS_sortIDs(inRefNum, resTypes[typeCount], &ids, &idCount);
		err = PAS_flattenResource(resTypes[typeCount], ids, idCount, inRefNum, outRefNum);
		DisposePtr((Ptr)ids);
	}
	
	DisposePtr((Ptr)resTypes);
	
	
	UseResFile(oldResFile);		
	CloseResFile(inRefNum);
	
	return err;
}

OSErr PAS_decodeResource(PASEntry *entry, FSSpec *outFile, short inRefNum)
{
	OSErr 			err = noErr;	
	short			outRefNum;
	PASResFork		info;
	SInt32			infoSize;
	short			oldResFile;
	
	PASResource		pasRes;
	SInt32			pasResSize;
	
	long			bufSize;
	Handle			buffer;
	long			counter=0;
	
	infoSize	=	sizeof(PASResFork);
	
	err = SetFPos(inRefNum, fsFromStart, (*entry).entryOffset );
	if (err != noErr)	return err;

	err	= FSRead( inRefNum, &infoSize, &info);
	if (err != noErr)	return err;

	if(infoSize != sizeof(PASResFork))
	{
		err = -1;
		goto error;
	}
	
	oldResFile=CurResFile();
	
	outRefNum = FSpOpenResFile(outFile, fsRdWrPerm);
	if (outRefNum < noErr)	return outRefNum;
				
	UseResFile(outRefNum);
	
	
	while (1)
	{
		pasResSize	=	sizeof(PASResource);
		err	= FSRead( inRefNum, &pasResSize, &pasRes);
		
		if (err != noErr)
		{
			if(err == eofErr)
				err = noErr;
				
			break;
		}
		
		bufSize	=	pasRes.length;
		buffer	=	NewHandle(bufSize);
		HLock(buffer);
		
		if(buffer == NULL)
		{
			//  if we did not get our memory, try updateresfile 
		
			HUnlock(buffer);


			UpdateResFile(outRefNum);
			counter=0;
			
			buffer	=	NewHandle(bufSize);
			HLock(buffer);
			
			if(buffer == NULL)
			{
				err = memFullErr;
				break;
			}
		}
		
		err	= FSRead( inRefNum, &bufSize, &(**buffer));
		if (err != noErr && err != eofErr)	break;
		
		AddResource(buffer, pasRes.attrType, pasRes.attrID, pasRes.attrName);
		WriteResource(buffer);
		
		SetResAttrs(buffer, pasRes.attr);
		ChangedResource(buffer);	
		WriteResource(buffer);
		
		ReleaseResource(buffer);	
		
		if (counter++ > 100)
		{
			UpdateResFile(outRefNum);
			counter=0;
		}
	
	}

error:
		
	UseResFile(oldResFile);	
	CloseResFile(outRefNum);
		
	return err;
}

#pragma mark -

OSErr PAS_encodeMisc(FSSpec *inFile, short outRefNum)
{
	OSErr 		err;	
	short		inRefNum;
	PASMiscInfo	infoBlock;
	FInfo		fInfo;
	SInt32		currentRead;
	
	err = FSpOpenDF(inFile, fsRdPerm, &inRefNum);
	if (err != noErr)	return err;
	
	memset(&infoBlock, 0, sizeof(PASMiscInfo));   
	
	err = FSpGetFInfo(inFile, &fInfo);
	if (err != noErr)	return err;
	
	infoBlock.fileType		=	fInfo.fdType;
	infoBlock.fileCreator	=	fInfo.fdCreator;
	infoBlock.fileFlags		=	fInfo.fdFlags;
	
		
	FSClose(inRefNum);
	
	
	inRefNum = FSpOpenResFile(inFile, fsRdPerm);
	if (inRefNum > noErr)
	{
		infoBlock.fileHasResFork	= 1;
		infoBlock.fileResAttrs 		= GetResFileAttrs(inRefNum);
		FSClose(inRefNum);
	}
	else
	{
		infoBlock.fileHasResFork	= 0;
		infoBlock.fileResAttrs 		= 0;
	}
	currentRead	= sizeof(PASMiscInfo);
	
	err = FSWrite(outRefNum, &currentRead, &infoBlock);
	if (err != noErr)	return err;
		
	CloseResFile(inRefNum);
	
	return noErr;
}


OSErr PAS_decodeMisc(PASEntry *entry, FSSpec *outFile, short inRefNum)
{
	OSErr 		err = noErr;	
	short		outRefNum;
	PASMiscInfo	info;
	SInt32		infoSize;
	FInfo		theFInfo;
		
	
	infoSize	=	sizeof(PASMiscInfo);
	
	err = SetFPos(inRefNum, fsFromStart, (*entry).entryOffset );
	if (err != noErr)	return err;

	err	= FSRead( inRefNum, &infoSize, &info);
	if (err != noErr)	return err;

	if(infoSize != sizeof(PASMiscInfo))
	{
		return -1;
	}

	err = FSpOpenDF(outFile, fsRdWrPerm, &outRefNum);
	if (err != noErr)	return err;
	
	memset(&theFInfo, 0, sizeof(FInfo));   
		
	theFInfo.fdType		=	info.fileType;
	theFInfo.fdCreator	=	info.fileCreator;	
	theFInfo.fdFlags	=	info.fileFlags;
	
	err = FSpSetFInfo(outFile, &theFInfo);
	if (err != noErr)	return err;
		
	FSClose(outRefNum);
	
	if (info.fileHasResFork)
	{
		outRefNum = FSpOpenResFile(outFile, fsRdWrPerm);
		if (outRefNum < noErr)
		{
			// maybe it does not have one!
			
			FSpCreateResFile(outFile, info.fileCreator, info.fileType, smSystemScript);
			
			outRefNum = FSpOpenResFile(outFile, fsRdWrPerm);	
			if (outRefNum < noErr) 
			{
				return outRefNum;
			}
		}
		
		SetResFileAttrs(outRefNum, info.fileResAttrs);
		
		
		CloseResFile(outRefNum);
	}	
	
	
	if(info.fileType == 'APPL')
	{
		// we need to add applications to the desktop database.
		
/*	FIX :: need to find DTSetAPPL() function	
		err = DTSetAPPL( NULL,
                    	 outFile->vRefNum,
			             info.fileCreator,
			             outFile->parID,
                         outFile->name);
*/	}
	
	
	return err;
	
	

}

#pragma mark -

OSErr PAS_encodeData(FSSpec *inFile, short outRefNum)
{
	OSErr 		err;	
	short		inRefNum;
	Ptr 		buffer;
	SInt32 		currentRead = 	PAS_BUFFER_SIZE;
		
	buffer	=	NewPtr(currentRead);

	err = FSpOpenDF(inFile, fsRdPerm, &inRefNum);
	if (err != noErr)	return err;
	
	while ( currentRead > 0 )
    {
		err	= FSRead( inRefNum, &currentRead, buffer);
		if (err != noErr && err != eofErr)	return err;
		
		err = FSWrite(outRefNum, &currentRead, buffer);
		if (err != noErr)	return err;
	}
	
	FSClose(inRefNum);
	
	DisposePtr(buffer);
	
	return noErr;
}

OSErr PAS_decodeData(PASEntry *entry, FSSpec *outFile, short inRefNum)
{
	OSErr 		err;	
	short		outRefNum;
	Ptr 		buffer;
	SInt32 		currentWrite = 	PAS_BUFFER_SIZE;
	SInt32		totalSize;
	
	
	buffer = NewPtr(currentWrite);
	
	
	err = FSpOpenDF(outFile, fsRdWrPerm, &outRefNum);
	if (err != noErr)	return err;
	
	
	err = SetFPos(inRefNum, fsFromStart, (*entry).entryOffset );
	if (err != noErr)	return err;
	
	err = SetFPos(outRefNum, fsFromStart, 0 );
	if (err != noErr)	return err;
	
	totalSize = (*entry).entryLength;
	
	while(totalSize > 0)
	{	
		currentWrite = PAS_BUFFER_SIZE;

		if (totalSize < currentWrite)
		{
			currentWrite = totalSize;
		}

		err	= FSRead( inRefNum, &currentWrite, buffer);
		if (err != noErr && err != eofErr)	return err;
	
		err = FSWrite(outRefNum, &currentWrite, buffer);
		if (err != noErr)	return err;
		
		totalSize = totalSize - currentWrite;

	}
	
	FSClose(outRefNum);
	
	DisposePtr(buffer);
	
	return noErr;

}

#pragma mark -

OSErr PAS_encodeHeader(short refnum)
{
	PASHeader 	header;
	long		sizeOfHeader;
	OSErr		err;
	
	
	sizeOfHeader = sizeof(PASHeader);
		
	memset(&header, 0, sizeOfHeader);   
	
	header.magicNum 	= PAS_MAGIC_NUM;
	header.versionNum 	= PAS_VERSION;
	header.numEntries 	= 3;
	
	// Write Out Header 
	err = FSWrite(refnum, &sizeOfHeader, &header);
	
	return err;

}

OSErr PAS_decodeHeader(short refNum, PASHeader *header)
{
	OSErr 	err;
	long 	sizeOfHeader = sizeof(PASHeader);
	
	memset(header, 0, sizeOfHeader);   
	
	err = FSRead(refNum, &sizeOfHeader, header);
	
	return err;
}

#pragma mark -


unsigned long PAS_getDataSize(FSSpec *spec)
{
	short		refNum;
	OSErr		err;
	Str255 		temp;	
	CInfoPBRec 	cbrec;
	err = FSpOpenDF(spec, fsRdPerm, &refNum);

	memcpy( temp, spec->name, spec->name[0] + 1);

	cbrec.hFileInfo.ioNamePtr 		= temp;
	cbrec.hFileInfo.ioDirID 		= spec->parID;
	cbrec.hFileInfo.ioVRefNum 		= spec->vRefNum;
	cbrec.hFileInfo.ioFDirIndex     = 0;

	err = PBGetCatInfoSync(&cbrec);
	FSClose(refNum);
	
	if(err != noErr)
	{
		cbrec.hFileInfo.ioFlLgLen = err;
	}

	return (cbrec.hFileInfo.ioFlLgLen);
}

short PAS_getResourceID(Handle resource)
{
	ResType			theType;
	Str255			name;
	short			theID;
	
	memset(&name, 0, sizeof(Str255));  
			
	GetResInfo(resource, &theID, &theType, name);
	
	return theID;
}


#pragma mark -
OSErr PAS_flattenResource(ResType type, short *ids, long count, short source, short dest)
{
	long 			idIndex;
	
	
	Handle			resToCopy;
	long			handleLength;
	
	PASResource		pasResource;
	long			pasResLen;
	
	OSErr			err;
	
	for (idIndex=0; idIndex < count; idIndex++)
	{
		if( (type == 'SIZE') && ( ids[idIndex] == 1 || ids[idIndex] == 0  ) )
		{
			/* 	
				We do not want to encode/flatten SIZE 0 or 1 because this
				is the resource that the user can modify.  Most applications
				will not be affected if we remove these resources
			*/
		}
		else
		{	
			resToCopy=Get1Resource(type,ids[idIndex]);
					
			if(!resToCopy)	
			{
				return resNotFound;
			}	
				
			memset(&pasResource, 0, sizeof(PASResource));	
				
			GetResInfo(	resToCopy, 	
						&pasResource.attrID, 
						&pasResource.attrType, 
						pasResource.attrName);	
			
			pasResource.attr = GetResAttrs(resToCopy);
			
			DetachResource(resToCopy);	
			HLock(resToCopy);
			
			pasResource.length 	= GetHandleSize(resToCopy);
			handleLength 		= pasResource.length;
			
			pasResLen			=	sizeof(PASResource);
			
			err = FSWrite(dest, &pasResLen, &pasResource);
			
			if(err != noErr) 
			{
				return err;
			}
			
			err = FSWrite(dest, &handleLength, &(**resToCopy));

			if(err != noErr) 
			{
				return err;
			}
			
			HUnlock(resToCopy);
			DisposeHandle(resToCopy);		
		}
	}
	
	
	return noErr;
}


#pragma mark -

void PAS_sortTypes(short sourceRefNum, ResType **resTypePtr, long *count)
{
	short				oldRef;
	short				typeIndex;
	short				numberOfTypes;
		
	*count	=	-1;
	
	oldRef = CurResFile();
		
	UseResFile(sourceRefNum);

	numberOfTypes = Count1Types();
	
	*resTypePtr	=	(ResType*) NewPtrClear( numberOfTypes * sizeof(OSType) );
	
	for (typeIndex=1; typeIndex <= numberOfTypes; typeIndex++)
	{
		Get1IndType(&(*resTypePtr)[typeIndex-1], typeIndex);
	}
	
	UseResFile(oldRef);
	
	PAS_bubbleSortResType(*resTypePtr, numberOfTypes);
	
	*count = numberOfTypes;
}	


void PAS_sortIDs(short sourceRefNum, OSType theType, short **IdPtr, long *count)
{
	short				oldRef;
	Handle				theHandle;
	short				resCount;
	short				resIndex;
	
	*count	=	-1;
	
	oldRef = CurResFile();
		
	UseResFile(sourceRefNum);

	resCount = Count1Resources(theType);
	
	*IdPtr	=	(short*) NewPtrClear( resCount * sizeof(short) );
	
	for (resIndex=1; resIndex <= resCount; resIndex++)
	{
		theHandle = Get1IndResource(theType, resIndex);
		
		if(theHandle == NULL) return;
	
		(*IdPtr)[resIndex-1] = PAS_getResourceID(theHandle);
		
		ReleaseResource(theHandle);
	}
	
	UseResFile(oldRef);
	
	PAS_bubbleSortIDS(*IdPtr, resCount);
	
	
	*count = resCount;
}

#pragma mark -

void
PAS_bubbleSortResType(ResType *types, long count)
{
	long 	x, y;
	OSType	temp;
	
	for (x=0; x < count-1; x++)      
	{
    	for (y=0; y < count-x-1; y++)        
		{
        	if (types[y] > types[y+1])        
	        {  
				temp=types[y];  
            	types[y]=types[y+1];  
            	types[y+1]=temp;        
        	} 
		}
	}
}

void
PAS_bubbleSortIDS(short *ids, long count)
{
	long 	x, y;
	short	temp;
	
	for (x=0; x < count-1; x++)      
	{
    	for (y=0; y < count-x-1; y++)        
		{
        	if (ids[y] > ids[y+1])        
	        {  
				temp=ids[y];  
            	ids[y]=ids[y+1];  
            	ids[y+1]=temp;        
        	} 
		}
	}
}

