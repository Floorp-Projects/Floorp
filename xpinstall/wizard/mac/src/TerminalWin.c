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
 *   Terminal Window
 *-----------------------------------------------------------*/

static Boolean bDownloadedRedirect = false;

void 
ShowTerminalWin(void)
{
	Str255		next, back;
	Handle		rectH;
	Rect		viewRect;
	short		reserr;
	MenuHandle 			popupMenu;
	PopupPrivateData ** pvtDataHdl;
	unsigned char *		currMenuItem;
	short 				i, vRefNum;
	long 				cwdDirID, dirID;
	Boolean				isDir = false;
	Str255				pModulesDir;
	OSErr 				err = noErr;
	GrafPtr		oldPort;
	GetPort(&oldPort);

	if (gWPtr != NULL)
	{
		SetPort(gWPtr);
	
		gCurrWin = kTerminalID; 
		/* gControls->tw = (TermWin*) NewPtrClear(sizeof(TermWin)); */
	
		GetResourcedString(next, rInstList, sInstallBtn);
		GetResourcedString(back, rInstList, sBackBtn);
	
		// malloc and get control
		rectH = Get1Resource('RECT', rStartMsgBox);
		reserr = ResError();
		if (reserr == noErr && rectH != NULL)
		{
			viewRect = (Rect) **((Rect **)rectH);
			ReleaseResource(rectH);
        }
		else
		{
			ErrorHandler(reserr, nil);
			return;
		}
		
		gControls->tw->siteSelector = NULL;
		gControls->tw->saveBitsCheckbox = NULL;
		
		gControls->tw->startMsgBox = viewRect;
	
		gControls->tw->startMsg = TENew(&viewRect, &viewRect);
        if (gControls->tw->startMsg == NULL)
        {
        	ErrorHandler(eMem, nil);
        	return;
        }
    
		// save bits after download and install
	
		/* get the "Installer Modules" relative subdir */
		ERR_CHECK(GetCWD(&cwdDirID, &vRefNum));
		GetIndString(pModulesDir, rStringList, sInstModules);
		GetDirectoryID(vRefNum, cwdDirID, pModulesDir, &dirID, &isDir);
		if (isDir)
		{
			if (!ExistArchives(vRefNum, dirID))  // going to download
			{			    
        		if (gControls->cfg->numSites > 0)
        		{
                    // download settings groupbox
                    Str255 dlSettingsGBTitle;
                    gControls->tw->dlSettingsGB = GetNewControl(rDLSettingsGB, gWPtr);
                    if (gControls->tw->dlSettingsGB)
                    {
                        GetResourcedString(dlSettingsGBTitle, rInstList, sDLSettings);
                        SetControlTitle(gControls->tw->dlSettingsGB, dlSettingsGBTitle);
                        ShowControl(gControls->tw->dlSettingsGB);
                    }
                    
                    // site selector label
                    Str255 siteSelMsgStr;
                    gControls->tw->siteSelMsg = GetNewControl(rSiteSelMsg, gWPtr);
                    if (gControls->tw->siteSelMsg)
                    {
                        GetResourcedString(siteSelMsgStr, rInstList, sSiteSelMsg);
                        SetControlData(gControls->tw->siteSelMsg, kControlNoPart, 
                            kControlStaticTextTextTag, siteSelMsgStr[0], (Ptr)&siteSelMsgStr[1]); 
                        ShowControl(gControls->tw->siteSelMsg);
                    }
                    
    		        // site selector
        			gControls->tw->siteSelector = GetNewControl( rSiteSelector, gWPtr );
        			if (!gControls->tw->siteSelector)
        			{
        				ErrorHandler(eMem, nil);
        				return;
        			}
        			
        			// populate popup button menus
        			HLock((Handle)gControls->tw->siteSelector);
        			pvtDataHdl = (PopupPrivateData **) (*(gControls->tw->siteSelector))->contrlData;
        			HLock((Handle)pvtDataHdl);
        			popupMenu = (MenuHandle) (**pvtDataHdl).mHandle;
        			for (i=0; i<gControls->cfg->numSites; i++)
        			{
        				HLock(gControls->cfg->site[i].desc);
        				currMenuItem = CToPascal(*gControls->cfg->site[i].desc);		
        				HUnlock(gControls->cfg->site[i].desc);
        				InsertMenuItem( popupMenu, currMenuItem, i );
        			}
        			HUnlock((Handle)pvtDataHdl);
        			HUnlock((Handle)gControls->tw->siteSelector);
        			SetControlMaximum(gControls->tw->siteSelector, gControls->cfg->numSites);
        			SetControlValue(gControls->tw->siteSelector, gControls->opt->siteChoice);
        			ShowControl(gControls->tw->siteSelector);
        		}
		
				// show check box and message
				gControls->tw->saveBitsCheckbox = GetNewControl( rSaveCheckbox, gWPtr );
				if (!gControls->tw->saveBitsCheckbox)
				{
					ErrorHandler(eMem, nil);
					return;
				}
				if (gControls->opt->saveBits)
				    SetControlValue(gControls->tw->saveBitsCheckbox, 1);
				ShowControl(gControls->tw->saveBitsCheckbox);
				
				// get rect for save bits message
				rectH = Get1Resource('RECT', rSaveBitsMsgBox);
				reserr = ResError();
				if (reserr == noErr && rectH != NULL)
				{
					 gControls->tw->saveBitsMsgBox  = (Rect) **((Rect **)rectH);
					 DisposeHandle(rectH);
			    }
				else
				{
					ErrorHandler(reserr, nil);
					return;
				}
				
				// get text edit record for save bits message
				gControls->tw->saveBitsMsg = TENew(&gControls->tw->saveBitsMsgBox,
												   &gControls->tw->saveBitsMsgBox );
			    if (gControls->tw->saveBitsMsg == NULL)
			    {
			    	ErrorHandler(eMem, nil);
			    	return;
			    }
			    HLock(gControls->cfg->saveBitsMsg);
				TESetText(*gControls->cfg->saveBitsMsg, strlen(*gControls->cfg->saveBitsMsg),
					gControls->tw->saveBitsMsg);
				HUnlock(gControls->cfg->saveBitsMsg);
				
				// show save bits msg
				TEUpdate(&gControls->tw->saveBitsMsgBox, gControls->tw->saveBitsMsg);
				
				// proxy settings button
				gControls->tw->proxySettingsBtn = GetNewControl(rProxySettgBtn, gWPtr);
				if (!gControls->tw->proxySettingsBtn)
				{
					ErrorHandler(eMem, nil);
					return;
				}
				Str255 proxySettingsTitle;
				GetResourcedString(proxySettingsTitle, rInstList, sProxySettings);
				SetControlTitle(gControls->tw->proxySettingsBtn, proxySettingsTitle);
				ShowControl(gControls->tw->proxySettingsBtn);
            }
		}
				
		// populate control
		HLock(gControls->cfg->startMsg);
		TESetText(*gControls->cfg->startMsg, strlen(*gControls->cfg->startMsg), 
					gControls->tw->startMsg);
		HUnlock(gControls->cfg->startMsg);
	
		// show controls
		TEUpdate(&viewRect, gControls->tw->startMsg);
		ShowNavButtons( back, next );
	}
	
	SetPort(oldPort);
}

short
GetRectFromRes(Rect *outRect, short inResID)
{
    Handle rectH;
    short reserr;
    
    if (!outRect)
        return eParam;
        
	// get rect for save bits message
	rectH = Get1Resource('RECT', inResID);
	reserr = ResError();
	if (reserr == noErr && rectH != NULL)
	{
		 *outRect  = (Rect) **((Rect **)rectH);
		 DisposeHandle(rectH);
    }
	else
	{
		ErrorHandler(reserr, nil);
		return reserr;
	}
	
	return 0;
}

void
UpdateTerminalWin(void)
{
	GrafPtr	oldPort;
	int i;
	Rect	instMsgRect;
	GetPort(&oldPort);
	SetPort(gWPtr);
	
	if (!gInstallStarted)
    	TEUpdate(&gControls->tw->startMsgBox, gControls->tw->startMsg);

    if (gControls->tw->dlProgressBar)
    {
        for (i = 0; i < kNumDLFields; ++i)
        {
            if (gControls->tw->dlLabels[i])
            {
                ShowControl(gControls->tw->dlLabels[i]);
            }
        }

        // XXX   TO DO
        // update the dl TEs
    }
	else if (gControls->tw->allProgressMsg)
	{
		HLock((Handle)gControls->tw->allProgressMsg);
		SetRect(&instMsgRect, (*gControls->tw->allProgressMsg)->viewRect.left,
				(*gControls->tw->allProgressMsg)->viewRect.top,
				(*gControls->tw->allProgressMsg)->viewRect.right,
				(*gControls->tw->allProgressMsg)->viewRect.bottom );
		HUnlock((Handle)gControls->tw->allProgressMsg);
		
		TEUpdate(&instMsgRect, gControls->tw->allProgressMsg);
	}
	else
	{ 
	    if (gControls->tw->saveBitsMsg)
	    {
	        TEUpdate(&gControls->tw->saveBitsMsgBox, gControls->tw->saveBitsMsg);
	    }
	}
	
	SetPort(oldPort);
}

void 
InTerminalContent(EventRecord* evt, WindowPtr wCurrPtr)
{	
	Point 			localPt;
	Rect			r;
	ControlPartCode	part;
	ControlHandle	currCntl;
	short 			checkboxVal;
	GrafPtr			oldPort;
	GetPort(&oldPort);
	
	SetPort(wCurrPtr);
	localPt = evt->where;
	GlobalToLocal( &localPt);
	
	if (gControls->tw->siteSelector)
	{
		HLock((Handle)gControls->tw->siteSelector);
		r = (**(gControls->tw->siteSelector)).contrlRect;
		HUnlock((Handle)gControls->tw->siteSelector);
		if (PtInRect(localPt, &r))
		{
			part = FindControl(localPt, gWPtr, &currCntl);
			part = TrackControl(currCntl, localPt, (ControlActionUPP) -1);
			gControls->opt->siteChoice = GetControlValue(currCntl);
			return;
		}		
	}

	if (gControls->tw->saveBitsCheckbox)
	{
		HLock((Handle)gControls->tw->saveBitsCheckbox);
		r = (**(gControls->tw->saveBitsCheckbox)).contrlRect;
		HUnlock((Handle)gControls->tw->saveBitsCheckbox);
		if (PtInRect(localPt, &r))
		{
			part = FindControl(localPt, gWPtr, &currCntl);
			part = TrackControl(currCntl, localPt, (ControlActionUPP) -1);
			checkboxVal = GetControlValue(currCntl);
			SetControlValue(currCntl, 1 - checkboxVal);
			if (checkboxVal)  // was selected so now toggling off
				gControls->opt->saveBits = false;
			else			  // was not selected so now toggling on
				gControls->opt->saveBits = true;
			return;
		}
	}

    if (gControls->tw->proxySettingsBtn)
    {
        HLock((Handle)gControls->tw->proxySettingsBtn);
        r = (**(gControls->tw->proxySettingsBtn)).contrlRect;
        HUnlock((Handle)gControls->tw->proxySettingsBtn);
        if (PtInRect(localPt, &r))
        {
            part = TrackControl(gControls->tw->proxySettingsBtn, evt->where, NULL);
            if (part != 0)
                OpenProxySettings();
            return;
        }
    }
					
	HLock((Handle)gControls->backB);
	SetRect(&r, (**(gControls->backB)).contrlRect.left,
				(**(gControls->backB)).contrlRect.top,
				(**(gControls->backB)).contrlRect.right,
				(**(gControls->backB)).contrlRect.bottom);
	HUnlock((Handle)gControls->backB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->backB, evt->where, NULL);
		if (part)
		{
		    /* before install has started */
		    if (gControls->state == eInstallNotStarted)
		    {
    			KillControls(gWPtr);
    			if (&gControls->tw->startMsgBox)
    			{
    				EraseRect(&gControls->tw->startMsgBox);
    				InvalRect(&gControls->tw->startMsgBox);
    			}
    			else
    			{
    				ErrorHandler(eParam, nil);	
    				return;
    			}
    			ClearSaveBitsMsg();
    			
    			/* treat last setup type  selection as custom */
    			if (gControls->opt->instChoice == gControls->cfg->numSetupTypes)
    			{
    				if (gControls->cfg->bAdditionsExist)
    					ShowAdditionsWin();
    				else
    					ShowComponentsWin();
    			}
    			else
    				ShowSetupTypeWin();
    			return;
            }
            
            /* pause button pressed */
            else if (gControls->state == eDownloading || gControls->state == eResuming)
            {
                SetPausedState();
            }
		}
	}
			
	HLock((Handle)gControls->nextB);		
	SetRect(&r, (**(gControls->nextB)).contrlRect.left,
				(**(gControls->nextB)).contrlRect.top,
				(**(gControls->nextB)).contrlRect.right,
				(**(gControls->nextB)).contrlRect.bottom);	
	HUnlock((Handle)gControls->nextB);
	if (PtInRect( localPt, &r))
	{
		part = TrackControl(gControls->nextB, evt->where, NULL);
		if (part)
		{
            BeginInstall();
		}
	}
	
	SetPort(oldPort);
}

void 
BeginInstall(void)
{
    ThreadID tid;

    /* starting download first time or resume download */
    if (gControls->state == eInstallNotStarted || gControls->state == ePaused)
    {
        if (gControls->state == eInstallNotStarted)
        {
            SetupPauseResumeButtons();
            ClearDownloadSettings();
            gInstallStarted = true;
        }
        else if (gControls->state == ePaused)
            SetResumedState();

        SpawnSDThread(Install, &tid);
    }
}

void
my_c2pstrcpy(const char *aCStr, Str255 aPStr)
{
    if (!aCStr)
        return;
    
    memcpy(&aPStr[1], aCStr, strlen(aCStr) > 255 ? 255 : strlen(aCStr));
    aPStr[0] = strlen(aCStr);
}

#define rProxyHostItem 3
#define rProxyPortItem 4
#define rProxyUserItem 5
#define rProxyPswdItem 6
#define rProxyStrOK			1
#define rProxyStrCancel		2
#define rProxyStrHostLab	7
#define rProxyStrPortLab	8
#define rProxyStrUserLab	9
#define rProxyStrPswdLab	10
 
void
OpenProxySettings(void)
{
    short itemHit = 999;
    short itemType;
    Handle item;
    Rect itemBox;
    Str255 itemText, pswdBuf, blindPswdText;
    DialogPtr psDlg;
    Boolean bDefault = true;
    
    /* show dialog */
    psDlg = GetNewDialog(rDlgProxySettg, NULL, (WindowPtr) -1);

    /* show dialog title, button and lable from install.ini */
    GetResourcedString(itemText, rInstList, sProxyDlg);
    SetWTitle(psDlg, itemText);
    GetDialogItem(psDlg, rProxyStrOK, &itemType, &item, &itemBox);
    GetResourcedString(itemText, rInstList, sOKBtn);
    SetControlTitle((ControlRecord **)item, itemText);
    SetControlData((ControlRecord **)item, kControlNoPart, 
            kControlPushButtonDefaultTag, sizeof(bDefault),(Ptr) &bDefault);
    GetDialogItem(psDlg, rProxyStrCancel, &itemType, &item, &itemBox);
    GetResourcedString(itemText, rInstList, sCancel);
    SetControlTitle((ControlRecord **)item, itemText);
    GetDialogItem(psDlg, rProxyStrHostLab, &itemType, &item, &itemBox);
    GetResourcedString(itemText, rInstList, sProxyHost);
    SetDialogItemText(item, itemText);
    GetDialogItem(psDlg, rProxyStrPortLab, &itemType, &item, &itemBox);
    GetResourcedString(itemText, rInstList, sProxyPort);
    SetDialogItemText(item, itemText);
    GetDialogItem(psDlg, rProxyStrUserLab, &itemType, &item, &itemBox);
    GetResourcedString(itemText, rInstList, sProxyUsername);
    SetDialogItemText(item, itemText);
    GetDialogItem(psDlg, rProxyStrPswdLab, &itemType, &item, &itemBox);
    GetResourcedString(itemText, rInstList, sProxyPassword);
    SetDialogItemText(item, itemText);

    /* pre-populate text fields */
    if (gControls->opt->proxyHost)
    {
        my_c2pstrcpy(gControls->opt->proxyHost, itemText);
        GetDialogItem(psDlg, rProxyHostItem, &itemType, &item, &itemBox);
        SetDialogItemText(item, itemText);
    }
    
    if (gControls->opt->proxyPort)
    {
        my_c2pstrcpy(gControls->opt->proxyPort, itemText);
        GetDialogItem(psDlg, rProxyPortItem, &itemType, &item, &itemBox);
        SetDialogItemText(item, itemText);    
    }
    
    if (gControls->opt->proxyUsername)
    {
        my_c2pstrcpy(gControls->opt->proxyUsername, itemText);
        GetDialogItem(psDlg, rProxyUserItem, &itemType, &item, &itemBox);
        SetDialogItemText(item, itemText);    
    }
    
    if (gControls->opt->proxyPassword)
    {
        int pswdLen = strlen(gControls->opt->proxyPassword);
        memset(&blindPswdText[1], '¥', pswdLen);
        blindPswdText[0] = pswdLen;
        GetDialogItem(psDlg, rProxyPswdItem, &itemType, &item, &itemBox);
        SetDialogItemText(item, blindPswdText);    
    }
    
    if (gControls->opt->proxyPassword)
        my_c2pstrcpy(gControls->opt->proxyPassword, pswdBuf);
    else
        pswdBuf[0] = 0;
    do
    {
        ModalDialog(NULL, &itemHit);
        
        /* special handling for "blind" password field */
        if (itemHit == rProxyPswdItem)
        {
            GetDialogItem(psDlg, rProxyPswdItem, &itemType, &item, &itemBox);
            GetDialogItemText(item, itemText);
            
            /* char deleted ? */
            if (itemText[0] < pswdBuf[0])
            {
                /* truncate password buffer */
                pswdBuf[0] = itemText[0];
            }
            else
            {
                /* store new char in password buffer */
                pswdBuf[itemText[0]] = itemText[itemText[0]];
                pswdBuf[0] = itemText[0];
            }
            
            memset(&blindPswdText[1], '¥', pswdBuf[0]);
            blindPswdText[0] = itemText[0];
            
            SetDialogItemText(item, blindPswdText);
        }
    } while(itemHit != 1 && itemHit != 2);
    
    /* if OK was hit then take changed settings */
    if (itemHit == 1)
    {
        GetDialogItem(psDlg, rProxyHostItem, &itemType, &item, &itemBox);
        GetDialogItemText(item, itemText);
        if (itemText[0] > 0)
        {
            if (gControls->opt->proxyHost)
                free(gControls->opt->proxyHost);
            gControls->opt->proxyHost = (char *) malloc(itemText[0] + 1);
            strncpy(gControls->opt->proxyHost, (const char *)&itemText[1], itemText[0]);
            *(gControls->opt->proxyHost + itemText[0]) = 0;
        }
        
        GetDialogItem(psDlg, rProxyPortItem, &itemType, &item, &itemBox);
        GetDialogItemText(item, itemText);
        if (itemText[0] > 0)
        {
            if (gControls->opt->proxyPort)
                free(gControls->opt->proxyPort);        
            gControls->opt->proxyPort = (char *) malloc(itemText[0] + 1);
            strncpy(gControls->opt->proxyPort, (const char *)&itemText[1], itemText[0]);
            *(gControls->opt->proxyPort + itemText[0]) = 0;
        }
            
        GetDialogItem(psDlg, rProxyUserItem, &itemType, &item, &itemBox);
        GetDialogItemText(item, itemText);
        if (itemText[0] > 0)
        {
            if (gControls->opt->proxyUsername)
                free(gControls->opt->proxyUsername);        
            gControls->opt->proxyUsername = (char *) malloc(itemText[0] + 1);
            strncpy(gControls->opt->proxyUsername, (const char *)&itemText[1], itemText[0]);
            *(gControls->opt->proxyUsername + itemText[0]) = 0;
        }
            
        if (pswdBuf[0] > 0)
        {
            if (gControls->opt->proxyPassword)
                free(gControls->opt->proxyPassword);        
            gControls->opt->proxyPassword = (char *) malloc(pswdBuf[0] + 1);
            strncpy(gControls->opt->proxyPassword, (const char *)&pswdBuf[1], pswdBuf[0]);
            *(gControls->opt->proxyPassword + pswdBuf[0]) = 0;
        }
    }
        
    DisposeDialog(psDlg);
}

Boolean
SpawnSDThread(ThreadEntryProcPtr threadProc, ThreadID *tid)
{
	OSErr			err;
	
	err = NewThread(kCooperativeThread, (ThreadEntryProcPtr)threadProc, (void*) nil, 
					0, kCreateIfNeeded, (void**)nil, tid);
				//  ^---- 0 means gimme the default stack size from Thread Manager
	if (err == noErr)
		YieldToThread(*tid); /* force ctx switch */
	else
	{
		return false;
	}
	
	return true;
}

void
ClearDownloadSettings(void)
{
    GrafPtr oldPort;
    
    GetPort(&oldPort);
    if (gWPtr)
    {
        SetPort(gWPtr);
        
        if (gControls->tw->dlSettingsGB)
            DisposeControl(gControls->tw->dlSettingsGB);
            
    	if (gControls->tw->startMsg)
            EraseRect(&gControls->tw->startMsgBox);
            
    	if (gControls->tw->siteSelector)
    		DisposeControl(gControls->tw->siteSelector);	  
        if (gControls->tw->siteSelMsg)
            DisposeControl(gControls->tw->siteSelMsg);
                    	
        if (gControls->tw->saveBitsMsg)
            EraseRect(&gControls->tw->saveBitsMsgBox);
        if (gControls->tw->saveBitsCheckbox)
            DisposeControl(gControls->tw->saveBitsCheckbox);
            
        if (gControls->tw->proxySettingsBtn)
            DisposeControl(gControls->tw->proxySettingsBtn);
    }
    
    SetPort(oldPort);            
}

void
ClearSaveBitsMsg(void)
{
    if (gControls->tw->saveBitsMsg)
        EraseRect(&gControls->tw->saveBitsMsgBox);
}

void
EnableTerminalWin(void)
{
    if (gControls->state == eInstallNotStarted)
    {
	    EnableNavButtons();
    	
    	if (gControls->tw->siteSelector)
    		HiliteControl(gControls->tw->siteSelector, kEnableControl);
    	if (gControls->tw->saveBitsCheckbox)
    	    HiliteControl(gControls->tw->saveBitsCheckbox, kEnableControl);
        if (gControls->tw->proxySettingsBtn)
            HiliteControl(gControls->tw->proxySettingsBtn, kEnableControl);	    
    }
    else if (gControls->state == eDownloading || gControls->state == eResuming)
    {
        if (gControls->nextB)
            HiliteControl(gControls->nextB, kDisableControl);       
        if (gControls->backB)
            HiliteControl(gControls->backB, kEnableControl);    
        if (gControls->cancelB)
            HiliteControl(gControls->cancelB, kDisableControl);
    }
    else if (gControls->state == ePaused)
    {
        if (gControls->nextB)
            HiliteControl(gControls->nextB, kEnableControl);       
        if (gControls->backB)
            HiliteControl(gControls->backB, kDisableControl); 
        if (gControls->cancelB)
            HiliteControl(gControls->cancelB, kDisableControl);    
    } 
}

void
DisableTerminalWin(void)
{
	DisableNavButtons();

	if (gControls->tw->siteSelector)
		HiliteControl(gControls->tw->siteSelector, kDisableControl);
	if (gControls->tw->saveBitsCheckbox)
	    HiliteControl(gControls->tw->saveBitsCheckbox, kDisableControl);
    if (gControls->tw->proxySettingsBtn)
        HiliteControl(gControls->tw->proxySettingsBtn, kDisableControl);		    
}

void
SetupPauseResumeButtons(void)
{
    Str255 pPauseLabel, pResumeLabel;
    
    /* rename labels to pause/resume */
    if (gControls->backB)
    {
    	GetResourcedString(pPauseLabel, rInstList, sPauseBtn);
        SetControlTitle(gControls->backB, pPauseLabel); 
        ShowControl(gControls->backB);
    }
    
    if (gControls->nextB)
    {
	    GetResourcedString(pResumeLabel, rInstList, sResumeBtn);
        SetControlTitle(gControls->nextB, pResumeLabel); 
        ShowControl(gControls->nextB);    
    }
    
    /* disable cancel button */
    if (gControls->cancelB)
        HiliteControl(gControls->cancelB, kDisableControl);
        
    /* disable pause button  */
    if (gControls->nextB)
        HiliteControl(gControls->nextB, kDisableControl);   
    
    /* enable resume button */
    if (gControls->backB)
        HiliteControl(gControls->backB, kEnableControl);    

    gControls->state = eDownloading;
}

void
SetPausedState(void)
{   
    /*  disable resume button */
    if (gControls->backB)
        HiliteControl(gControls->backB, kDisableControl);
    
    /* enable pause button */
    if (gControls->nextB)
        HiliteControl(gControls->nextB, kEnableControl);     
          
    gControls->state = ePaused;
}

void
SetResumedState(void)
{    
    /* disable pause button  */
    if (gControls->nextB)
        HiliteControl(gControls->nextB, kDisableControl);   
    
    /* enable resume button */
    if (gControls->backB)
        HiliteControl(gControls->backB, kEnableControl);
        
    gControls->state = eResuming;
}

void
DisablePauseAndResume()
{
    /* disable pause button  */
    if (gControls->nextB)
        HiliteControl(gControls->nextB, kDisableControl);   
    
    /* disable resume button */
    if (gControls->backB)
        HiliteControl(gControls->backB, kDisableControl);

}