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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
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

#include "MacInstallWizard.h"


/*-----------------------------------------------------------*
 *   INI Parser
 *-----------------------------------------------------------*/

void
ParseConfig(void)
{ 	
	OSErr	err;
	char 	*cfgText;

	gControls->cfg = (Config*) NewPtrClear(sizeof(Config));			

	if(!gControls->cfg)
	{
        ErrorHandler(eMem, nil);
		return;
	}
	
	if (!ReadINIFile(sConfigFName, &cfgText))
	{
        ErrorHandler(eCfgRead, nil);
	    return;
	}
	
	ERR_CHECK(PopulateGeneralKeys(cfgText));
	ERR_CHECK(PopulateLicWinKeys(cfgText));
	ERR_CHECK(PopulateWelcWinKeys(cfgText));
	ERR_CHECK(PopulateCompWinKeys(cfgText));
	ERR_CHECK(PopulateSetupTypeWinKeys(cfgText));
	ERR_CHECK(PopulateTermWinKeys(cfgText));
	ERR_CHECK(PopulateIDIKeys(cfgText));
	ERR_CHECK(PopulateMiscKeys(cfgText));
	
	ERR_CHECK(MapDependees());
    
    if (cfgText)
	    DisposePtr(cfgText);
}

void
ParseInstall(void)
{ 	
    OSErr 				err;
    char 	*instText;

    gStrings = (InstINIRes*) NewPtrClear(sizeof(InstINIRes));			

    if(!gStrings)
    {
        ErrorHandler(eMem, nil);
    	return;
    }
    
    if (!ReadINIFile(sInstallFName, &instText))
    {
        ErrorHandler(eInstRead, nil);
        return;
    }
    
    ERR_CHECK(PopulateInstallKeys(instText));
    
    ERR_CHECK(ReadSystemErrors(instText));
    
    if (instText)
        DisposePtr(instText);
}

Boolean
ReadINIFile(int sINIFName, char **text)
{
    Boolean				bSuccess = false, isDir = false;
	OSErr 				err;
	FSSpec 				cfgFile;
	long				cwdDirID, dirID, dataSize;
	short				vRefNum, fileRefNum;
	Str255 				fname, pModulesDir;
	
	*text = nil;
	
	ERR_CHECK_RET(GetCWD(&cwdDirID, &vRefNum), false);
	
	/* get the "Installer Modules" relative subdir */
	GetIndString(pModulesDir, rStringList, sInstModules);
	GetDirectoryID(vRefNum, cwdDirID, pModulesDir, &dirID, &isDir);
	if (!isDir)		/* bail if we can't find the "Installer Modules" dir */
		return false;
		
    /* open config.ini or install.ini file */
    GetIndString(fname, rStringList, sINIFName);
	if ((err = FSMakeFSSpec(vRefNum, dirID, fname, &cfgFile)) != noErr )
		return false;
	if ((err = FSpOpenDF( &cfgFile, fsRdPerm, &fileRefNum)) != noErr)
		return false;
		
	/* read in entire text */
	if ( (err = GetEOF(fileRefNum, &dataSize)) != noErr)
		bSuccess = false;
	if (dataSize > 0)
	{
		*text = (char*) NewPtrClear(dataSize + 1);		// ensure null termination
		if (!(*text))
		{
            ErrorHandler(eMem, nil);
			return false;
		}
			
        if ((err = FSRead(fileRefNum, &dataSize, *text)) == noErr)
			bSuccess = true;
	}
	
	/* close file */
	if (!bSuccess || ((err = FSClose(fileRefNum))!=noErr))   
		return false;

	return bSuccess;
}	

#pragma mark -

 OSErr
PopulateInstallKeys(char *instText)
{
    OSErr err = noErr;
    Handle tmp;
    int i;
    
    tmp = NewHandleClear(kValueMaxLen);
    if (!tmp)
    {
        ErrorHandler(eMem, nil);
       	return eParseFailed;
    }

    for( i = 1; i < instKeysNum + 1; i++ )
    {
        FillKeyValueUsingSLID(rInstList, sInstGeneral, i, tmp, instText);
        my_c2pstrcpy(*tmp, gStrings->iKey[i]);
    }

    for( i = 1; i < instMenuNum + 1; i++ )
    {
        FillKeyValueUsingSLID(rInstMenuList, sMenuGeneral, i, tmp, instText);
        my_c2pstrcpy(*tmp, gStrings->iMenu[i]);
    }
    
    for( i = 1; i < instErrsNum + 1; i++ )
    {
        FillKeyValueUsingSLID(rErrorList, eErrorMessage, i, tmp, instText);
        my_c2pstrcpy(*tmp, gStrings->iErr[i]);
    }

    DisposeHandle(tmp);
    return err;
}

#define kSystemErrors "System Errors"

OSErr
ReadSystemErrors(const char *cfg)
{
  char  *sectionName, *section, *key, *cfgPtr[1], *sectionPtr[1];
  char      *value, *outValue;
  extern short gErrTableSize;
  unsigned char *pStr255;
  extern ErrTableEnt *gErrTable;  
  OSErr ret = noErr;  

  *cfgPtr = (char*) cfg;
    
  value = (char*) calloc(kValueMaxLen, sizeof( char ) );
  outValue = (char*) calloc(kValueMaxLen, sizeof( char ) );
  sectionName =     (char *) calloc( kSNameMaxLen, sizeof( char ) );
  section =         (char *) calloc( kSectionMaxLen, sizeof( char ) ); 
  key =             (char *) calloc( kKeyMaxLen, sizeof( char ) );
  if (!sectionName || !section || !key || !value || !outValue)
  {
    ErrorHandler(eMem, nil);
    ret = eParseFailed;
  } 
  else {
    
    /* find next section   [cfgPtr moved past next section per iteration] */
    
    while(GetNextSection(cfgPtr, sectionName, section))
    { 
      if (strncmp(sectionName, kSystemErrors, strlen(kSystemErrors)) == 0)    
      {
        *sectionPtr = section;
            
      /* find next key   [sectionPtr moved past next key per iteration] */
        while(GetNextKeyVal(sectionPtr, key, outValue))
        {             
          gErrTable = (ErrTableEnt *) realloc( gErrTable, (gErrTableSize + 1) * sizeof( ErrTableEnt ) );
          if ( gErrTable != NULL ) {
            gErrTable[gErrTableSize].num = atoi(key);
            pStr255 = CToPascal(outValue);
            if ( pStr255 ) {
              BlockMoveData(&pStr255[0], &gErrTable[gErrTableSize++].msg[0], pStr255[0] + 1);
              DisposePtr((char *) pStr255);
            }
          }                      
        }
      }
    }
  }
  
  if(key) 
    free(key);
  if(sectionName) 
    free(sectionName);
  if(section) 
    free(section);
  if(value) 
    free(value);
  if(outValue) 
    free(outValue);
    
  return ret; 
}

OSErr
PopulateGeneralKeys(char *cfgText)
{
    short rv;
    int i;
    char cSection[255], cKeyRoot[255], cKey[255];
    Str255 pSection, pKeyRoot;
    
    /* General section: subdir */
    gControls->cfg->targetSubfolder = NewHandleClear(kValueMaxLen);
    if (!gControls->cfg->targetSubfolder)
    {
        ErrorHandler(eMem, nil);
        return eParseFailed;
    }
    
    /* don't check retval siunce we don't care if we don't find this: it's optional */
    FillKeyValueUsingResID(sGeneral, sSubfolder, gControls->cfg->targetSubfolder, cfgText);
    
    /* General section: global URL */
    GetIndString(pSection, rParseKeys, sGeneral);
    GetIndString(pKeyRoot, rParseKeys, sURL);
    CopyPascalStrToC(pKeyRoot, cKeyRoot);
    CopyPascalStrToC(pSection, cSection);
    gControls->cfg->numGlobalURLs = 0;
    for (i = 0; i < kMaxGlobalURLs; ++i)
    {
        gControls->cfg->globalURL[i] = NewHandleClear(kValueMaxLen);
        if (!gControls->cfg->globalURL[i])
        {
            ErrorHandler(eMem, nil);
            return eParseFailed;
        }

        /* not compulsory */
        sprintf(cKey, "%s%d", cKeyRoot, i);  // URL0, URL1, ... URL4
        rv  = FillKeyValueUsingName(cSection, cKey, gControls->cfg->globalURL[i], cfgText);
        if (rv == false)
            break;
        gControls->cfg->numGlobalURLs++;
    }
    
    return noErr;
}

OSErr
PopulateLicWinKeys(char *cfgText)
{
	OSErr err = noErr;
	
	/* LicenseWin: license file name */
	gControls->cfg->licFileName = NewHandleClear(kValueMaxLen);
	if (!gControls->cfg->licFileName)
	{
        ErrorHandler(eMem, nil);
		return eParseFailed;
	}
	
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
	if (!gControls->cfg->welcMsg[0])
	{
        ErrorHandler(eMem, nil);
		return eParseFailed;
	}
		
	if (!FillKeyValueUsingResID(sWelcDlg, sMsg0, gControls->cfg->welcMsg[0], cfgText))
	{
        ErrorHandler(eParseFailed, nil);
		return eParseFailed;
	}
		
	gControls->cfg->welcMsg[1] = NewHandleClear(kValueMaxLen);
	if (!gControls->cfg->welcMsg[1])
	{
        ErrorHandler(eParseFailed, nil);
		return eParseFailed;
	}	

    FillKeyValueUsingResID(sWelcDlg, sMsg1, gControls->cfg->welcMsg[1], cfgText);
		
	gControls->cfg->welcMsg[2] = NewHandleClear(kValueMaxLen);
	if (!gControls->cfg->welcMsg[2])
	{
        ErrorHandler(eParseFailed, nil);
		return eParseFailed;
	}

	FillKeyValueUsingResID(sWelcDlg, sMsg2, gControls->cfg->welcMsg[2], cfgText);
	/*
	** NOTE:
	** We don't care if the second and third messages are not filled since by
	** definition we require only one message string to be specified in the INI
	** file. Msgs 2 and 3 are optional.
	*/
	
	/* get readme file name and app to launch it with */
	gControls->cfg->readmeFile = NewHandleClear(kValueMaxLen);
	if (!gControls->cfg->readmeFile)
	{
        ErrorHandler(eMem, nil);
		return eParseFailed;
	}
	if (FillKeyValueUsingResID(sWelcDlg, sReadmeFilename, gControls->cfg->readmeFile, cfgText))
		gControls->cfg->bReadme = true;
	else
		gControls->cfg->bReadme = false;
	
	if (gControls->cfg->bReadme)
	{
		gControls->cfg->readmeApp = NewHandleClear(kValueMaxLen);
		if (!gControls->cfg->readmeApp)
		{
            ErrorHandler(eMem, nil);
			return eParseFailed;
		}
		
		if (!FillKeyValueUsingResID(sWelcDlg, sReadmeApp, gControls->cfg->readmeApp, cfgText))
		{
            ErrorHandler(eMem, nil);
			return eParseFailed;
		}
	}
		
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
	char 		*currDepNum;
	long		randomPercent;
	Boolean		bRandomSet;
	
	eof[0] = 0;
	
	/* ComponentsWin: components and their descriptions, and other properties */
	gControls->cfg->selCompMsg = NewHandleClear(kValueMaxLen);
	FillKeyValueUsingResID(sCompDlg, sMsg0, gControls->cfg->selCompMsg, cfgText);
	
	/* AdditionsWin: additional components */
	gControls->cfg->selAddMsg = NewHandleClear(kValueMaxLen);
	FillKeyValueUsingResID(sAddDlg, sMsg0, gControls->cfg->selAddMsg, cfgText);
	gControls->cfg->bAdditionsExist = false;
	
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
		    DisposePtr(currSName);
		    DisposePtr(currSNameBuf);
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
		HLock(sizeH);
		gControls->cfg->comp[i].size = atol(*sizeH);
		HUnlock(sizeH);
		DisposeHandle(sizeH);
		DisposePtr(currKey);
		
		// set the dirty flag to indicate the CRC has not passed
		
		gControls->cfg->comp[i].dirty = true;
		
		/* random install percentage */
		GetIndString(pkey, rParseKeys, sRandomInstall);
		currKey = PascalToC(pkey);
		Handle randomH = NewHandleClear(4);
		if (FillKeyValueUsingName(currSName, currKey, randomH, cfgText))
		{
			bRandomSet = true;
			HLock(randomH);
			randomPercent = atol(*randomH);
			HUnlock(randomH);
			
			if (randomPercent != 0) /* idiot proof for those giving 0 as the rand percent */
			{
				if (RandomSelect(randomPercent))
				{
					gControls->cfg->comp[i].selected = true;
					gControls->cfg->comp[i].refcnt = 1;
				}
				else
				{
					gControls->cfg->comp[i].selected = false;
					gControls->cfg->comp[i].refcnt = 0;
				}
			}
			else
				bRandomSet = false;
		}
		else
			bRandomSet = false;
		if (randomH)
			DisposeHandle(randomH);
		if (currKey)
			DisposePtr(currKey);
		
		/* attributes (SELECTED|INVISIBLE|LAUNCHAPP|ADDITIONAL|DOWNLOAD_ONLY) */
		GetIndString(pkey, rParseKeys, sAttributes);
		currKey = PascalToC(pkey);
		Handle attrValH = NewHandleClear(255);
		if (FillKeyValueUsingName(currSName, currKey, attrValH, cfgText))
		{
			HLock(attrValH);
		
			char *attrType = NULL;
			GetIndString(pkey, rParseKeys, sSELECTED);
			attrType = PascalToC(pkey);
			if (!bRandomSet) /* when random key specified then selected attr is overriden */
			{
				if (NULL != strstr(*attrValH, attrType))
				{
					gControls->cfg->comp[i].selected = true;
					gControls->cfg->comp[i].refcnt = 1;
				}
				else
				{
					gControls->cfg->comp[i].selected = false;
					gControls->cfg->comp[i].refcnt = 0;
				}
			}
			if (attrType)
				DisposePtr(attrType);
		
			GetIndString(pkey, rParseKeys, sINVISIBLE);
			attrType = PascalToC(pkey);
			if (NULL != strstr(*attrValH, attrType))
				gControls->cfg->comp[i].invisible = true;
			else
				gControls->cfg->comp[i].invisible = false;
			if (attrType)
				DisposePtr(attrType);
				
			GetIndString(pkey, rParseKeys, sLAUNCHAPP);
			attrType = PascalToC(pkey);
			if (NULL != strstr(*attrValH, attrType))
				gControls->cfg->comp[i].launchapp = true;
			else
				gControls->cfg->comp[i].launchapp = false;
			if (attrType)
				DisposePtr(attrType);
			
			GetIndString(pkey, rParseKeys, sADDITIONAL);
			attrType = PascalToC(pkey);
			if (NULL != strstr(*attrValH, attrType))
			{
				gControls->cfg->comp[i].additional = true;
				gControls->cfg->bAdditionsExist = true; /* doesn't matter if set multiple times */
			}
			else
				gControls->cfg->comp[i].additional = false;
			if (attrType)
				DisposePtr(attrType);	
			
			GetIndString(pkey, rParseKeys, sDOWNLOAD_ONLY);
			attrType = PascalToC(pkey);
			if (NULL != strstr(*attrValH, attrType))
				gControls->cfg->comp[i].download_only = true;
			else
				gControls->cfg->comp[i].download_only = false;
			if (attrType)
				DisposePtr(attrType);	
				
			HUnlock(attrValH);
		}
		if (attrValH)
			DisposeHandle(attrValH);
			
		/* initialize to not highlighted */
		gControls->cfg->comp[i].highlighted = false;
		
		/* dependees for other components */
		gControls->cfg->comp[i].numDeps = 0;
		GetIndString(pkey, rParseKeys, sDependee);
		currKeyBuf = PascalToC(pkey);
		for (j=0; j<kMaxComponents; j++)
		{
			// currKey = "Dependee<j>"
			currDepNum = ltoa(j);
			currKey = NewPtrClear(strlen(currKeyBuf) + strlen(currDepNum) + 1);
			strcpy(currKey, currKeyBuf);
			strcat(currKey, currDepNum);

			gControls->cfg->comp[i].depName[j] = NewHandleClear(kValueMaxLen);
			if (!FillKeyValueUsingName(currSName, currKey, gControls->cfg->comp[i].depName[j], cfgText))
			{
				if (currKey)
					DisposePtr(currKey);
				if (currDepNum)
					free(currDepNum);
				break;
			}
			
			gControls->cfg->comp[i].numDeps++;
			if (currKey)
				DisposePtr(currKey);
			if (currDepNum)
				free(currDepNum);
		}
		if (currKeyBuf)
			DisposePtr(currKeyBuf);
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
			DisposePtr(currSName);
			DisposePtr(currSNameBuf);
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
				
				HLock(currVal);
				cNumIdx = strspn(currCompName, *currVal);
				compIdx = *currVal+cNumIdx;
				compNum = atoi(compIdx);
				gControls->cfg->st[i].comp[compNum] = kInSetupType;
				gControls->cfg->st[i].numComps++;
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
	OSErr 	err = noErr;
	short 	i;
	Str255 	pSection, pIdRoot, pDomainRoot, pDescRoot, pSubpath;
	char  	*cSection = NULL, *cId = NULL, *cDomain = NULL, 
	        *cDesc = NULL, *cSubpath = NULL, *cIndex = NULL;
	Boolean bMoreSites = true;
	
	/* TerminalWin: start install msg */
	gControls->cfg->startMsg = NewHandleClear(kValueMaxLen);
	FillKeyValueUsingResID(sTermDlg, sMsg0, gControls->cfg->startMsg, cfgText);
	
	/* site selector keys */
	gControls->cfg->numSites = 0;
	GetIndString(pSection, rParseKeys, sSiteSelector);
	GetIndString(pIdRoot, rParseKeys, sIdentifier);
	GetIndString(pDomainRoot, rParseKeys, sDomain);
	GetIndString(pDescRoot, rParseKeys, sDescription);
	cSection = PascalToC(pSection);
	cId = NewPtrClear(pIdRoot[0] + 4);
	cDesc = NewPtrClear(pDescRoot[0] + 4);
	cDomain = NewPtrClear(pDomainRoot[0] + 4);
	if (!cSection || !cDesc || !cDomain)
		return eParseFailed;
	for (i = 0; i < kMaxSites; i++)
	{	
		cIndex = ltoa(i);
		if (!cIndex) 
			break;
		
		memset(cId, 0, pIdRoot[0] + 4);
		strncpy(cId, (char*)&pIdRoot[1], pIdRoot[0]);
		strcat(cId, cIndex);
		
		memset(cDesc, 0, pDescRoot[0] + 4);
		strncpy(cDesc, (char*)&pDescRoot[1], pDescRoot[0]);
		strcat(cDesc, cIndex);
		
		memset(cDomain, 0 , pDomainRoot[0] + 4);
		strncpy(cDomain, (char*)&pDomainRoot[1], pDomainRoot[0]);
		strcat(cDomain, cIndex);
		
		gControls->cfg->site[i].id = NewHandleClear(kValueMaxLen);
		if (!FillKeyValueUsingName(cSection, cId, gControls->cfg->site[i].id, cfgText))
		    bMoreSites = false;
		if (bMoreSites)
		{
			gControls->cfg->site[i].desc = NewHandleClear(kValueMaxLen);
		    if (!FillKeyValueUsingName(cSection, cDesc, gControls->cfg->site[i].desc, cfgText))
			    bMoreSites = false;
			gControls->cfg->site[i].domain = NewHandleClear(kValueMaxLen);
			if (!FillKeyValueUsingName(cSection, cDomain, gControls->cfg->site[i].domain, cfgText))
				bMoreSites = false;
			else
				gControls->cfg->numSites++;
		}

		if (cIndex)
			free(cIndex);
		cIndex = NULL;
		
		if (!bMoreSites)
			break;
	}
	
	if (cSection)
		DisposePtr((Ptr) cSection);
	if (cId)
	    DisposePtr((Ptr) cId);
	if (cDesc)
		DisposePtr((Ptr) cDesc);
	if (cDomain)
		DisposePtr((Ptr) cDomain);
		
	/* redirect for remote site selector section */
	GetIndString(pSection, rParseKeys, sRedirect);
	GetIndString(pSubpath, rParseKeys, sSubpath);
	cSection = PascalToC(pSection);
	cDesc = PascalToC(pDescRoot);
	cSubpath = PascalToC(pSubpath);
	if (!cSubpath || !cSection || !cDesc)
		return eMem;
		
	gControls->cfg->redirect.desc = NewHandleClear(kValueMaxLen);
	if (FillKeyValueUsingResID(sRedirect, sDescription, gControls->cfg->redirect.desc, cfgText))
	{			
		gControls->cfg->redirect.subpath = NewHandleClear(kValueMaxLen);
		FillKeyValueUsingName(cSection, cSubpath, gControls->cfg->redirect.subpath, cfgText);
	}
	
	/* save bits msg */
	gControls->cfg->saveBitsMsg = NewHandleClear(kValueMaxLen);
	FillKeyValueUsingResID(sTermDlg, sMsg1, gControls->cfg->saveBitsMsg, cfgText);
	
	if (cSection)
		DisposePtr((Ptr)cSection);
	if (cDesc)
		DisposePtr((Ptr)cDesc);
	if (cSubpath)
		DisposePtr((Ptr)cSubpath);
		
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

OSErr
PopulateMiscKeys(char *cfgText)
{
	OSErr err = noErr;
	int i;
	Str255	psection;
	Ptr		csection;
	char 	*section, *idx;
	
	/* RunAppX section */
	gControls->cfg->numRunApps = 0;
	GetIndString(psection, rParseKeys, sRunApp);
	csection = PascalToC(psection);

	for (i = 0; i < kMaxRunApps; i++)
	{
		section = (char*) malloc((strlen(csection) * sizeof(char)) + 3); /* added 3 for idx nums and null termination */
		if (!section)
			return eParseFailed;
		memset( section, 0, ((strlen(csection) * sizeof(char)) + 3) );
		idx = ltoa(i);
		strcpy(section, csection);
		strcat(section, idx);
		strcat(section, "\0");
		gControls->cfg->apps[i].targetApp = NewHandleClear(kValueMaxLen);
		if (!FillKeyValueSecNameKeyID(rParseKeys, section, sTargetApp, gControls->cfg->apps[i].targetApp, cfgText))
		{
			if (section)
				free (section);
			if (idx)
				free(idx);
			break;
		}
		gControls->cfg->apps[i].targetDoc = NewHandleClear(kValueMaxLen);
		if (!FillKeyValueSecNameKeyID(rParseKeys, section, sTargetDoc, gControls->cfg->apps[i].targetDoc, cfgText))
			gControls->cfg->apps[i].targetDoc = NULL;	// no optional doc supplied
			
		gControls->cfg->numRunApps++;
		if (section)
			free (section);
		if (idx)
			free (idx);
	}
	if (csection)
		DisposePtr(csection);
	
	/* LegacyCheckX section */
	gControls->cfg->numLegacyChecks = 0;
	GetIndString(psection, rParseKeys, sLegacyCheck);
	csection = PascalToC(psection);
	
	for (i = 0; i < kMaxLegacyChecks; i++)
	{
		section = (char *) malloc((strlen(csection) * sizeof(char)) + 3); /* added 3 for idx nums and null termination */
		if (!section)
			return eParseFailed;
		memset( section, 0, ((strlen(csection) * sizeof(char)) + 3) );
		idx = ltoa(i);
		strcpy(section, csection);
		strcat(section, idx);
		strcat(section, "\0");
		
		gControls->cfg->checks[i].filename = NewHandleClear(kValueMaxLen);
		if (!FillKeyValueSecNameKeyID(rParseKeys, section, sFilename, gControls->cfg->checks[i].filename, cfgText))
		{
			if (section)
				free (section);
			if (idx)
				free (idx);
			break;
		}
		
		gControls->cfg->checks[i].subfolder = NewHandleClear(kValueMaxLen);
		FillKeyValueSecNameKeyID(rParseKeys, section, sSubfolder, gControls->cfg->checks[i].subfolder, cfgText);
		
		/* if no version, we'll detect if the file is there and throw up the warning dlg */
		gControls->cfg->checks[i].version = NewHandleClear(kValueMaxLen);
		FillKeyValueSecNameKeyID(rParseKeys, section, sVersion, gControls->cfg->checks[i].version, cfgText);
		
		gControls->cfg->checks[i].message = NewHandleClear(kValueMaxLen);
		if (!FillKeyValueSecNameKeyID(rParseKeys, section, sMessage, gControls->cfg->checks[i].message, cfgText))
		{
			if (section)
				free (section);
			if (idx)
				free (idx);
			break;
		}
		gControls->cfg->numLegacyChecks++;
		if (section)
			free (section);
		if (idx)
			free (idx);
	}
	if (csection)
		DisposePtr(csection);
	
	return err;
}

#pragma mark - 

OSErr
MapDependees()
{
	OSErr	err = noErr;
	int 	i, j, compIdx;
	
	for (i=0; i<gControls->cfg->numComps; i++)
	{
		// init all deps to off
		for (j=0; j<kMaxComponents; j++)
		{
			gControls->cfg->comp[i].dep[j] = kDependeeOff;
		}
		
		// loop through turning on deps
		for(j=0; j<gControls->cfg->comp[i].numDeps; j++)
		{
			compIdx = GetComponentIndex(gControls->cfg->comp[i].depName[j]);
			if (compIdx != kInvalidCompIdx)
			{
				gControls->cfg->comp[i].dep[compIdx] = kDependeeOn;
				
				// we deal with making it selected and mucking with the ref count
				// in the components win code (see ComponentsWin.c:{SetOptInfo(), ResolveDependees()}
			}
		}
	}
	
	return err;
}

Boolean
RandomSelect(long percent)
{
	Boolean bSelect = false;
	int arbitrary = 0;
	
	srand(time(NULL));
	arbitrary = rand();
	arbitrary %= 100;
	
	if (arbitrary <= percent)
		bSelect = true;
		
	return bSelect;
}

short
GetComponentIndex(Handle compName)
{
	int i;
	short compIdx = kInvalidCompIdx;
	
	HLock(compName);
	for (i=0; i<gControls->cfg->numComps; i++)
	{
		HLock(gControls->cfg->comp[i].shortDesc);
		if (0==strcmp(*gControls->cfg->comp[i].shortDesc, *compName))
		{
			compIdx = i;			
			HUnlock(gControls->cfg->comp[i].shortDesc);
			break;
		}
		HUnlock(gControls->cfg->comp[i].shortDesc);
	}
	HUnlock(compName);
	
	return compIdx;
}

#pragma mark -
	
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
FillKeyValueSecNameKeyID(short strListID, char *sectionName, short keyID, Handle dest, char *cfgText)
{
	/* Fill key-value pair using section name str and key res id */
	
	unsigned char	pkey[kKeyMaxLen];
	char		  	*key;
	Boolean 		bFound = false;
	
	GetIndString(pkey, strListID, keyID);
	key = PascalToC(pkey);
	
	if (FillKeyValueUsingName(sectionName, key, dest, cfgText))
		bFound = true;
	
	if (key)
		DisposePtr(key);
		
	return bFound;
}

Boolean
FillKeyValueUsingName(char *sectionName, char *keyName, Handle dest, char *cfgText)
{
	/* Fill key-value pair using the pascal string section name and key name */
	
	char		*value;
	Boolean		bFound = false;
	
	value = (char*) NewPtrClear(kValueMaxLen);
	if (!value)
	{
        ErrorHandler(eMem, nil);
		return false;
	}
			
	if (FindKeyValue(cfgText, sectionName, keyName, value))
	{
		long	len = strlen(value);		
		OSErr	err;
		
		SetHandleSize(dest, len + 1);
		err = MemError();
		if (err != noErr)
		{
            ErrorHandler(err, nil);
			return false;
		}
		
		HLock(dest);
		strcpy(*dest, value);
		HUnlock(dest);
		bFound = true;
	}
	if (value)
	    DisposePtr(value);
	return bFound;
}

#pragma mark -

Boolean
FindKeyValue(const char *cfg, const char *inSectionName, const char *inKey, char *outValue)
{
	char 	*sectionName, *section, *key, *cfgPtr[1], *sectionPtr[1];

	*cfgPtr = (char*) cfg;
	
	sectionName = 	(char *) NewPtrClear( kSNameMaxLen );
	section = 		(char *) NewPtrClear( kSectionMaxLen ); 
	key = 			(char *) NewPtrClear( kKeyMaxLen );
	if (!sectionName || !section || !key)
	{
        ErrorHandler(eMem, nil);
		return false;
	}	
    
	/* find next section   [cfgPtr moved past next section per iteration] */
    while(GetNextSection(cfgPtr, sectionName, section))
	{ 
        if (strncmp(sectionName, inSectionName, strlen(inSectionName)) == 0)	
		{
			*sectionPtr = section;
			
			/* find next key   [sectionPtr moved past next key per iteration] */
            while(GetNextKeyVal(sectionPtr, key, outValue))
			{
				if (*key && strncmp(key, inKey, strlen(inKey)) == 0)
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
		if (*txt == ';') /* comment encountered */
		{
			while ((*txt != MAC_EOL) && (*txt != WIN_EOL)) /* ignore rest of line */
				txt++;
		}
		
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
find_contents:
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
	    
    /* handle case where '[' is in key or value (i.e., for i18n)*/
    if (*txt != MY_EOF)
    {
        if (*txt == START_SECTION && 
            !(txt == *ioTxt || *(txt-1) == MAC_EOL || *(txt-1) == WIN_EOL))
        {
    		if( kSectionMaxLen-1 >= cnt++)	/* prevent from falling of end of outSection buffer */				
    		{
    			*sbuf = *txt;
    			sbuf++; txt++;
    		}
    		else
    			txt++;
            goto find_contents;
        }
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
			sbuf++;

	if (*sbuf == MY_EOF) /* no more keys */
		return false;
	
	if (*sbuf == ';') /* comment encountered */
	{
		while ((*sbuf != MAC_EOL) && (*sbuf != WIN_EOL)) /*ignore rest of line */
			sbuf++;
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

	if (*outKey == NULL)
		exists = false;
		
	return exists;
}

#pragma mark -

/*
 * Makes a copy of the C string, converts the copy to a Pascal string,
 * and returns the Pascal string copy
 */
unsigned char *CToPascal(char *str)
{
	long len;
	char* cpy;

	len = strlen(str);
	cpy = (char*)NewPtrClear(len+1);
	if (!cpy)
		return 0;
    strncpy(&cpy[1], str, len);
    if (len > 255) 
        cpy[0] = 255;
    else
        cpy[0] = len;
	return((unsigned char *)cpy);
}

char *PascalToC(unsigned char *str)
{
	register unsigned char *p,*q,*end;
	unsigned char * cpy; 
	
	cpy = (unsigned char*)NewPtrClear( ((long)*str+1) ); 
	if (!cpy)
		return 0;
	strncpy((char*)cpy, (char*) str, (long)*str+1);
	end = cpy + *cpy;
	q = (p=cpy) + 1;
	while (p < end) *p++ = *q++;
	*p = '\0';
	return((char *)cpy);
}

void CopyPascalStrToC(ConstStr255Param srcPString, char* dest)
{
    BlockMoveData(&srcPString[1], dest, srcPString[0]);
    dest[srcPString[0]] = '\0';
}

/* Get strings from install.ini */
Boolean GetResourcedString(Str255 skey, int nList, int nIndex)
{
	if( nList == rInstList && nIndex < instKeysNum + 1 )
	{
		pstrcpy(skey, gStrings->iKey[nIndex]);
		return true;
	}
	else if( nList == rErrorList && nIndex < instErrsNum + 1 )
	{
		pstrcpy(skey, gStrings->iErr[nIndex]);
		return true;
	}
	else if( nList == rInstMenuList && nIndex < instMenuNum + 1 )
	{
		pstrcpy(skey, gStrings->iMenu[nIndex]);
		return true;
	}
	else
		return false;
}
