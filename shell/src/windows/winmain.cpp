/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the Private language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
 
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include "nsRepository.h"
#include "nsShellInstance.h"
#include "nsApplicationManager.h"

#define SHELL_DLL "shell.dll"

extern nsIID kIXPCOMApplicationShellCID ;

static NS_DEFINE_IID(kIApplicationShellIID, NS_IAPPLICATIONSHELL_IID);
static NS_DEFINE_IID(kCApplicationShellIID, NS_IAPPLICATIONSHELL_CID);

static NS_DEFINE_IID(kIShellInstanceIID, NS_ISHELLINSTANCE_IID);
static NS_DEFINE_IID(kCShellInstanceCID, NS_SHELLINSTANCE_CID);

int PASCAL WinMain(HANDLE instance, HANDLE prevInstance, LPSTR cmdParam, int nCmdShow)
{
	nsresult result = NS_OK ;

	nsIShellInstance * pShellInstance ;
	nsIApplicationShell * pApplicationShell ;

    // Let get a ShellInstance for this Application instance
    NSRepository::RegisterFactory(kCShellInstanceCID, SHELL_DLL, PR_FALSE, PR_FALSE);

	result = NSRepository::CreateInstance(kCShellInstanceCID,
										  NULL,
										  kIShellInstanceIID,
										  (void **) &pShellInstance) ;

	if (result != NS_OK)
		return result ;

    // Let's instantiate the Application's Shell
    NS_RegisterApplicationShellFactory() ;

	result = NSRepository::CreateInstance(kIXPCOMApplicationShellCID,
										  NULL,
										  kIXPCOMApplicationShellCID,
										  (void **) &pApplicationShell) ;
		
	if (result != NS_OK)
		return result ;

    // Let the the State know who it's Application Instance is
    pShellInstance->SetNativeInstance((nsNativeApplicationInstance) instance);
    pShellInstance->SetApplicationShell(pApplicationShell);

    // Tell the application manager to store away the association so the
    // Application can look up its State
    nsApplicationManager::SetShellAssociation(pApplicationShell, pShellInstance);

    // Initialize the system
    pShellInstance->Init();
	pApplicationShell->Init();

    // Now, let actually start dispatching events.
	result = pApplicationShell->Run();

    // We're done, clean up
    nsApplicationManager::DeleteShellAssociation(pApplicationShell, pShellInstance);

    // book out of here
	return result;
}

void main(int argc, char **argv)
{
  WinMain(GetModuleHandle(NULL), NULL, 0, SW_SHOW);
}
