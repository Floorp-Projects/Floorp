/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License. 
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1999
 * Netscape Communications Corporation. All Rights Reserved.  
 * 
 * Contributors:
 *     Samir Gehani <sgehani@netscape.com>
 */


#include "MacInstallWizard.h"


/*-----------------------------------------------------------*
 *   INI Parser
 *-----------------------------------------------------------*/

pascal void *
PullDownConfig(void *unused)
{
	short			vRefNum;
	long			dirID;
	OSErr 			err;
	FSSpec			idiSpec;

	SDISTRUCT		sdistruct;
	Str255			pIDIfname;

	ERR_CHECK_RET(GetCWD(&dirID, &vRefNum), (void*)0);
	
	GetIndString(pIDIfname, rStringList, sConfigIDIName);
	
	/* get idi filepath */
	ERR_CHECK_RET(FSMakeFSSpec(vRefNum, dirID, pIDIfname, &idiSpec), false);

	/* populate SDI struct */
	sdistruct.dwStructSize 	= sizeof(SDISTRUCT);
	sdistruct.fsIDIFile 	= idiSpec;
	sdistruct.dlDirVRefNum 	= vRefNum;
	sdistruct.dlDirID 		= dirID;
	sdistruct.hwndOwner    	= NULL;
	
	/* call SDI_NetInstall */
	gInstFunc(&sdistruct);

	return (void*)true;
}

void
ParseConfig(void)
{ 	
	OSErr	err;
	char 	*cfgText;

	gControls->cfg = (Config*) NewPtrClear(sizeof(Config));		
//dougt: what happens when allocation fails!		

	if(!ReadConfigFile(&cfgText))
	{
			ErrorHandler();
			return;
	}
	
	ERR_CHECK(PopulateLicWinKeys(cfgText));
	ERR_CHECK(PopulateWelcWinKeys(cfgText));
	ERR_CHECK(PopulateCompWinKeys(cfgText));
	ERR_CHECK(PopulateSetupTypeWinKeys(cfgText));
	ERR_CHECK(PopulateTermWinKeys(cfgText));
	ERR_CHECK(PopulateIDIKeys(cfgText));
	
    if (cfgText)
	    DisposePtr(cfgText);
}

Boolean
ReadConfigFile(char **text)
{
//dougt: nitpick, I would have the initial bSuccess set to false, and only change to true when this function really
//       succeeds.  If you also set text to null from the get go, you can return the else statement during the read
//       of the file.
    Boolean				bSuccess = true;
	OSErr 				err;
	FSSpec 				cfgFile;
	long				dirID, dataSize;
	short				vRefNum;
	Str255 				fname;
	
	ERR_CHECK_RET(GetCWD(&dirID, &vRefNum), false);
	
	/* open config.ini file */
	GetIndString(fname, rStringList, sConfigFName);
	if (err = FSMakeFSSpec(vRefNum, dirID, fname, &cfgFile) )
		return false;
	if (err = FSpOpenDF( &cfgFile, fsRdPerm, &vRefNum))
		return false;
		
	/* read in entire text */
	if (err = GetEOF(vRefNum, &dataSize))
		bSuccess = false;
	if (dataSize > 0)
	{
		*text = (char*) NewPtrClear(dataSize);
//dougt: what happens when allocation fails!	
        if (err = FSRead(vRefNum, &dataSize, *text))
			bSuccess = false;
	}
	else
	{
		*text = nil;
		bSuccess = false;
	}
		
	/* close file */
	if (err = FSClose(vRefNum))
		return false;

	return bSuccess;
}	

OSErr
PopulateLicWinKeys(char *cfgText)
{
	OSErr err = noErr;
	
	/* LicenseWin: license file name */
	gControls->cfg->licFileName = NewHandleClear(kValueMaxLen);
//dougt: what happens when allocation fails!	
	if (!FillKeyValueUsingResID(sLicDlg, sLicFile, gControls->cfg->licFileName, cfgText))
		err = eParseFailed;
	
	return err;
}

OSErr
PopulateWelcWinKeys(char *cfgText)
{
	OSErr err = noErr;
	
	/* WelcomeWin: message strings */
	gControls->cfg->welcMsg[0] = NewHandleClear(kValueMaxLen);
//dougt: what happens when allocation fails!	
	if (!FillKeyValueUsingResID(sWelcDlg, sMsg0, gControls->cfg->welcMsg[0], cfgText))
		err = eParseFailed;  //dougt: shouldn't you return now?
		
	gControls->cfg->welcMsg[1] = NewHandleClear(kValueMaxLen);
//dougt: what happens when allocation fails!	

//dougt: why don;t you care about the error in this case?
    FillKeyValueUsingResID(sWelcDlg, sMsg1, gControls->cfg->welcMsg[1], cfgText);
		
	gControls->cfg->welcMsg[2] = NewHandleClear(kValueMaxLen);
//dougt: what happens when allocation fails!	

//dougt: why don;t you care about the error in this case?
FillKeyValueUsingResID(sWelcDlg, sMsg2, gControls->cfg->welcMsg[2], cfgText);
		
	return err;
}

OSErr
PopulateCompWinKeys(char *cfgText)
{
	OSErr 		err = noErr;
	int			i, j, hunVal, tenVal, unitVal;
	Ptr			currSNameBuf, currSName, currKeyBuf, currKey, idxCh;
	Str255		pSName, pkey, pidx;
	char		eof[1];
	
	eof[0] = 0;
	
	/* ComponentsWin: components and their descriptions, and other properties */
	gControls->cfg->selCompMsg = NewHandleClear(kValueMaxLen);
	FillKeyValueUsingResID(sCompDlg, sMsg0, gControls->cfg->selCompMsg, cfgText);

	gControls->cfg->numComps = 0;
	for (i=0; i<kMaxComponents; i++)
	{
		GetIndString(pSName, rParseKeys, sComponent);		
		currSNameBuf = PascalToC(pSName);
		currSName = NewPtrClear(kSNameMaxLen);
		strncpy(currSName, currSNameBuf, strlen(currSNameBuf));
		
		if(i>99) // concat 100's digit 
		{
			hunVal = i/100;
			GetIndString(pidx, rIndices, hunVal+1);
			idxCh = PascalToC(pidx);
			strncat(currSName, idxCh, 1);
			DisposePtr(idxCh);
		}
		else
			hunVal = 0;
		
		if(i>9)	// concat 10's digit 
		{
			tenVal = (i - (hunVal*100))/10;
			GetIndString(pidx, rIndices, tenVal+1);
			idxCh = PascalToC(pidx);
			strncat(currSName, idxCh, 1);
			DisposePtr(idxCh);
		}
		else
			tenVal = 0;
	
		unitVal = i - (hunVal*100) - (tenVal*10);
		GetIndString(pidx, rIndices, unitVal+1);
		idxCh = PascalToC(pidx);
		strncat(currSName, idxCh, 1);
		DisposePtr(idxCh);
		strncat(currSName, eof, 1);
		
		/* short description */
		GetIndString(pkey, rParseKeys, sDescShort);
		currKey = PascalToC(pkey);
		gControls->cfg->comp[i].shortDesc = NewHandleClear(kValueMaxLen);
		if (!FillKeyValueUsingName(currSName, currKey, gControls->cfg->comp[i].shortDesc, cfgText))
		{
			DisposePtr(currKey);
			break; // no more Components
		}
		DisposePtr(currKey);
		
		/* long description */
		GetIndString(pkey, rParseKeys, sDescLong);
		currKey = PascalToC(pkey);
		gControls->cfg->comp[i].longDesc = NewHandleClear(kValueMaxLen);
		FillKeyValueUsingName(currSName, currKey, gControls->cfg->comp[i].longDesc, cfgText);
		DisposePtr(currKey);
		
		/* archive */
		GetIndString(pkey, rParseKeys, sArchive);
		currKey = PascalToC(pkey);
		gControls->cfg->comp[i].archive = NewHandleClear(kArchiveMaxLen);
		FillKeyValueUsingName(currSName, currKey, gControls->cfg->comp[i].archive, cfgText);
		DisposePtr(currKey);
		
		/* install size */
		GetIndString(pkey, rParseKeys, sInstSize);
		currKey = PascalToC(pkey);
		Handle sizeH = NewHandleClear(4); // long is four bytes
		FillKeyValueUsingName(currSName, currKey, sizeH, cfgText);
		HLockHi(sizeH);
		gControls->cfg->comp[i].size = atol(*sizeH);
		HUnlock(sizeH);
		DisposeHandle(sizeH);
		DisposePtr(currKey);
		
		/* URLs for redundancy/retry/failover */
		gControls->cfg->comp[i].numURLs = 0;
		for (j=0; j<kMaxURLPerComp; j++)
		{
			GetIndString(pkey, rParseKeys, sURL);
			currKeyBuf = PascalToC(pkey);
			currKey = NewPtrClear(strlen(currKeyBuf)+3); // tens, unit, null termination
			strncpy(currKey, currKeyBuf, strlen(currKeyBuf));
			gControls->cfg->comp[i].url[j] = NewHandleClear(kURLMaxLen);
			
			GetIndString(pidx, rIndices, j+1);
			idxCh = PascalToC(pidx);
			strncat(currKey, idxCh, 1);
			DisposePtr(idxCh);
			strncat(currKey, eof, 1);
			
			if (!FillKeyValueUsingName(currSName, currKey, gControls->cfg->comp[i].url[j], cfgText))
			{
				DisposePtr(currKey);
				break;
			}
			gControls->cfg->comp[i].numURLs++;
			DisposePtr(currKey);
		}
	}
	gControls->cfg->numComps = i;
	
	return err;
}

OSErr
PopulateSetupTypeWinKeys(char *cfgText)
{
	OSErr 		err = noErr;
	int			i, j, hunVal, tenVal, unitVal, compNum, cNumIdx;
	Ptr			currSNameBuf, currSName, currKeyBuf, currKey, idxCh, currCompName, compIdx;
	Str255		pSName, pkey, pidx;
	char		eof[1];
	Handle		currVal;
	
	eof[0] = 0;
	
	/* SetupTypeWin: types and their descriptions */
	gControls->cfg->numSetupTypes = 0;
	for (i=0;i<kMaxSetupTypes;i++)   /* init stComp[][] lists */
	{
		for(j=0; j<gControls->cfg->numComps; j++)
		{
			gControls->cfg->st[i].comp[j] = kNotInSetupType;
		}
	}
	for (i=0; i<kMaxSetupTypes; i++)
	{
		GetIndString(pSName, rParseKeys, sSetupType);		
		currSNameBuf = PascalToC(pSName);
		currSName = NewPtrClear(kKeyMaxLen);
		strncpy(currSName, currSNameBuf, strlen(currSNameBuf));

		unitVal = i;
		GetIndString(pidx, rIndices, unitVal+1);
		idxCh = PascalToC(pidx);
		strncat(currSName, idxCh, 1);
		DisposePtr(idxCh);
		strncat(currSName, eof, 1);
		
		/* short description */
		GetIndString(pkey, rParseKeys, sDescShort);
		currKey = PascalToC(pkey);
		gControls->cfg->st[i].shortDesc = NewHandleClear(kValueMaxLen);
		if (!FillKeyValueUsingName(currSName, currKey, gControls->cfg->st[i].shortDesc, cfgText))
		{
			DisposePtr(currKey);
			break; // no more SetupTypes 
		}
		DisposePtr(currKey);

		/* long description */
		GetIndString(pkey, rParseKeys, sDescLong);
		currKey = PascalToC(pkey);
		gControls->cfg->st[i].longDesc = NewHandleClear(kValueMaxLen);
		FillKeyValueUsingName(currSName, currKey, gControls->cfg->st[i].longDesc, cfgText);
		DisposePtr(currKey);
		
		/* now search for all components in this SetupType and fill its stComp[][] array */
		gControls->cfg->st[i].numComps = 0;
		for (j=0; j<gControls->cfg->numComps; j++)
		{
			GetIndString(pkey, rParseKeys, sC);
			currKeyBuf = PascalToC(pkey);
			currKey = NewPtrClear(kKeyMaxLen);
			strncpy(currKey, currKeyBuf, strlen(currKeyBuf));
			
			if(j>99) // concat 100's digit 
			{
				hunVal = j/100;
				GetIndString(pidx, rIndices, hunVal+1);
				idxCh = PascalToC(pidx);
				strncat(currKey, idxCh, 1);
				DisposePtr(idxCh);
			}
			else
				hunVal = 0;
		
			if(j>9)	// concat 10's digit 
			{
				tenVal = (j - (hunVal*100))/10;
				GetIndString(pidx, rIndices, tenVal+1);
				idxCh = PascalToC(pidx);
				strncat(currKey, idxCh, 1);
				DisposePtr(idxCh);
			}
			else
				tenVal = 0;
	
			unitVal = j - (hunVal*100) - (tenVal*10);
			GetIndString(pidx, rIndices, unitVal+1);
			idxCh = PascalToC(pidx);
			strncat(currKey, idxCh, 1);
			DisposePtr(idxCh);
			strncat(currKey, eof, 1);
			
			currVal = NewHandleClear(kValueMaxLen);
			if (FillKeyValueUsingName(currSName, currKey, currVal, cfgText))
			{
				/* Parse the value to get the component index */
				GetIndString(pSName, rParseKeys, sComponent);		
				currSNameBuf = PascalToC(pSName);
				currCompName = NewPtrClear(kKeyMaxLen);
				strncpy(currCompName, currSNameBuf, strlen(currSNameBuf));
				
				HLockHi(currVal);
				cNumIdx = strspn(currCompName, *currVal);
				compIdx = *currVal+cNumIdx;
				compNum = atoi(compIdx);
				gControls->cfg->st[i].comp[compNum] = kInSetupType;
				gControls->cfg->st[i].numComps++;  /* not used? */
				HUnlock(currVal);
			}
		}
			
		DisposePtr(currKey);
		DisposePtr(currKeyBuf);
		DisposePtr(currSName);
		DisposePtr(currSNameBuf);
	}
	gControls->cfg->numSetupTypes = i;
	
	return err;
}
	
OSErr
PopulateTermWinKeys(char *cfgText)
{
	OSErr err = noErr;
	
	/* TerminalWin: start install msg */
	gControls->cfg->startMsg = NewHandleClear(kValueMaxLen);
	FillKeyValueUsingResID(sTermDlg, sMsg0, gControls->cfg->startMsg, cfgText);
	
	return err;
}

OSErr
PopulateIDIKeys(char *cfgText)
{
	OSErr err = noErr;
		
	/* "Tunneled" IDI Keys */
	gControls->cfg->coreFile = NewHandleClear(kValueMaxLen);
	FillKeyValueUsingSLID(rIDIKeys, sSDNSInstall, sCoreFile, gControls->cfg->coreFile, cfgText);
	
	gControls->cfg->coreDir = NewHandleClear(kValueMaxLen);
	FillKeyValueUsingSLID(rIDIKeys, sSDNSInstall, sCoreDir, gControls->cfg->coreDir, cfgText);
	
	gControls->cfg->noAds = NewHandleClear(kValueMaxLen);
	FillKeyValueUsingSLID(rIDIKeys, sSDNSInstall, sNoAds, gControls->cfg->noAds, cfgText);
	
	gControls->cfg->silent = NewHandleClear(kValueMaxLen);
	FillKeyValueUsingSLID(rIDIKeys, sSDNSInstall, sSilent, gControls->cfg->silent, cfgText);
	
	gControls->cfg->execution = NewHandleClear(kValueMaxLen);
	FillKeyValueUsingSLID(rIDIKeys, sSDNSInstall, sExecution, gControls->cfg->execution, cfgText);
	
	gControls->cfg->confirmInstall = NewHandleClear(kValueMaxLen);
	FillKeyValueUsingSLID(rIDIKeys, sSDNSInstall, sConfirmInstall, gControls->cfg->confirmInstall, cfgText);
	
	return err;
}
	
Boolean
FillKeyValueUsingResID(short dlgID, short keyID, Handle dest, char *cfgText)
{
	/* Fill key-value pair using resIDs for the dlg/section name, and the key */
	
	return FillKeyValueUsingSLID(rParseKeys, dlgID, keyID, dest, cfgText);
}

Boolean
FillKeyValueForIDIKey(short keyID, Handle dest, char *cfgText)
{
	/* Fill key-value pair in IDI stringlist */
	
	return FillKeyValueUsingSLID(rIDIKeys, sSDNSInstall, keyID, dest, cfgText);
}

Boolean
FillKeyValueUsingSLID(short stringListID, short dlgID, short keyID, Handle dest, char *cfgText)
{
	/* Fill key-value pair using stringlist ID */

	unsigned char	pkey[kKeyMaxLen], psectionName[kSNameMaxLen];
	char			*key, *sectionName;
	Boolean			bFound = false;
	
	GetIndString(pkey, stringListID, keyID);		
	key = PascalToC(pkey);
	
	GetIndString(psectionName, stringListID, dlgID);
	sectionName = PascalToC(psectionName);
	
	if (FillKeyValueUsingName(sectionName, key, dest, cfgText))
		bFound = true;

	if(sectionName)
        DisposePtr(sectionName);
	if(key)
        DisposePtr(key);
	return bFound;
}

Boolean
FillKeyValueUsingName(char *sectionName, char *keyName, Handle dest, char *cfgText)
{
	/* Fill key-value pair using the pascal string section name and key name */
	
	char		*value;
	long		len;
	Boolean		bFound = false;
	
	value = (char*) NewPtrClear(kValueMaxLen);
//dougt: what happens when allocation fails!		
	if (FindKeyValue(cfgText, sectionName, keyName, value))
	{
		HLock(dest);
		len = strlen(value);
		strncpy(*dest, value, len);
//dougt: would it be better here to do the accual allocation, PtrToHandle()?  
// This way we could reduce the mem footprint by not having to allocate the max each time.
		HUnlock(dest);
		bFound = true;
	}
	if (value)
	    DisposePtr(value);
	return bFound;
}

Boolean
FindKeyValue(const char *cfg, const char *inSectionName, const char *inKey, char *outValue)
{
	char 	*sectionName, *section, *key, *cfgPtr[1], *sectionPtr[1];
	
	*cfgPtr = cfg;
	
	sectionName = 	(char *) NewPtrClear( kSNameMaxLen );
	section = 		(char *) NewPtrClear( kSectionMaxLen ); 
	key = 			(char *) NewPtrClear( kKeyMaxLen );
//dougt: what happens when allocation fails!	
    
	/* find next section   [cfgPtr moved past next section per iteration] */
//dougt: you are passing a pointer (cfgPtr) to a function that wants a char**.  evil
    while(GetNextSection(cfgPtr, sectionName, section))
	{
//dougt: why not use strcmp here?  
        if (strncmp(sectionName, inSectionName, strlen(inSectionName)) == 0)	
		{
			*sectionPtr = section;
			
			/* find next key   [sectionPtr moved past next key per iteration] */
//dougt: you are passing a pointer (sectionPtr) to a function that wants a char**.  evil
            while(GetNextKeyVal(sectionPtr, key, outValue))
			{
//dougt: why not use strcmp here?
				if (strncmp(key, inKey, strlen(key)) == 0)
				{
					if(key) 
                        DisposePtr(key);
					if(sectionName) 
                        DisposePtr(sectionName);
					if(section) 
                        DisposePtr(section);
					return true;
				}
			}
		}
	}
    if(key) 
        DisposePtr(key);
    if(sectionName) 
        DisposePtr(sectionName);
    if(section) 
        DisposePtr(section);
	
	return false; 
}

Boolean
GetNextSection(char **ioTxt, char *outSectionName, char *outSection)
{
	Boolean exists = false;
	char *txt, *snbuf, *sbuf;
	long cnt;
	
	txt = *ioTxt;
	
	while (*txt != START_SECTION)
	{
		if (*txt == MY_EOF)
			return false;
		else
			txt++;
	}
	
	if (*txt == START_SECTION) /* section name start */
	{
		txt++; 
		snbuf = outSectionName;
		cnt = 0;
		while (*txt != END_SECTION) /* section name end */
		{
			if ((*txt==MAC_EOL) || (*txt==WIN_EOL)) /* incomplete section name so die */
			{	
				return false;
			}
			
			if( kSNameMaxLen-1 >= cnt++) /* prevent falling of end of outSectionName buffer */
			{
				*snbuf = *txt;
				snbuf++; txt++;
			}
			else
				txt++;
		}
		*snbuf = MY_EOF;  /* close string */
		txt++; /* skip over section name end char ']' */
	}
		
	/* adminstration before section contents encountered */
	while( (*txt == MAC_EOL) || (*txt == WIN_EOL) || (*txt == ' ') || (*txt == '\t'))
	{
		txt++;
	}
	
	sbuf = outSection;
	cnt = 0;
	while (*txt != START_SECTION && *txt != MY_EOF) /* next section encountered */
	{
		if( kSectionMaxLen-1 >= cnt++)	/* prevent from falling of end of outSection buffer */				
		{
			*sbuf = *txt;
			sbuf++; txt++;
		}
		else
			txt++;
	}
	*sbuf = MY_EOF;   	/* close string */
	*ioTxt = txt;		/* move txt ptr to next section */
	exists = true;
	
	return exists;
}

Boolean
GetNextKeyVal(char **inSection, char *outKey, char *outVal)
{
	Boolean exists = false;
	char *key, *sbuf, *val;
	long cnt;
	
	sbuf = *inSection;
	
	/* clear out extra carriage returns above next key */
	while( (*sbuf == MAC_EOL) || (*sbuf == WIN_EOL) || (*sbuf == ' ') || (*sbuf == '\t')) 
	{
		if (*sbuf == MY_EOF) /* no more keys */
			return false;
		else
			sbuf++;
	}
	
	key = outKey;
	cnt = 0;
	while((*sbuf != MAC_EOL) && (*sbuf != WIN_EOL))
	{
		if (*sbuf == KV_DELIM)
		{
			if (key != nil) /* key not empty */
				exists = true;
			break;
		}
		
		if ( kKeyMaxLen-1 >= cnt++) /* prevent from falling of end of outKey buffer */
		{
			*key = *sbuf;
			key++; sbuf++;
		}
		else
			sbuf++;
	}
	*key = MY_EOF; /* close string */
	sbuf++;
	
	if (exists) /* key found so now get value */
	{
		val = outVal;
		cnt = 0;
		while((*sbuf != MAC_EOL) && (*sbuf != WIN_EOL))
		{
			if ( kValueMaxLen-1 >= cnt++) /* prevent from falling of end of outValue buffer */
			{
				*val = *sbuf;
				val++; sbuf++;
			}
			else
				sbuf++;
		}
	}
	*val = MY_EOF; /* close string */
	sbuf++;
	
	*inSection = sbuf; /* capture current position in section for future GetNextKeyValue() */

	return exists;
}


/*
 * Makes a copy of the C string, converts the copy to a Pascal string,
 * and returns the Pascal string copy
 */

//dougt: can you use the toolbox routines?  what about double bite chars?

unsigned char *CToPascal(char *str)
{
	register char *p,*q;
	long len;
	char* cpy;

	len = strlen(str);
	cpy = (char*)NewPtrClear(len);
	if (!cpy)
		return 0;
	strncpy(cpy, str, len);
	if (len > 255) len = 255;
	p = cpy + len;
	q = p-1;
	while (p != cpy) *p-- = *q--;
	*cpy = len;
	return((unsigned char *)cpy);
}

char *PascalToC(unsigned char *str)
{
	register unsigned char *p,*q,*end;
	unsigned char * cpy; 
	
	cpy = (unsigned char*)NewPtrClear( ((long)*str+1) ); 
	strncpy((char*)cpy, (char*) str, (long)*str+1);
	end = cpy + *cpy;
	q = (p=cpy) + 1;
	while (p < end) *p++ = *q++;
	*p = '\0';
	return((char *)cpy);
}
