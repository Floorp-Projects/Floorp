/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "nspr.h"
#include "nsRepository.h"
#include "nsINetPlugin.h"
#include "nsINetPluginInstance.h"
#include "plstr.h"
#include "nscore.h"
#include "magicCID.h"


static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kINetPluginInstanceIID, NS_INETPLUGININSTANCE_IID);


static NS_DEFINE_CID(kINetPluginCID, NS_INET_PLUGIN_CID);
static NS_DEFINE_CID(kINetPluginEBMCID, NS_INET_PLUGIN_EBM_CID);
static NS_DEFINE_CID(kINetPluginHTMLCID, NS_INET_PLUGIN_HTML_CID);
static NS_DEFINE_CID(kINetPluginPLAINCID, NS_INET_PLUGIN_PLAIN_CID);
static NS_DEFINE_CID(kINetPluginTARCID, NS_INET_PLUGIN_TAR_CID);


extern "C" const nsCID &
MAGIC_FindCID(const char *mime_type)
{
	if (PL_strcmp(mime_type, "image/x-ebm") == 0)
	{
		return kINetPluginEBMCID;
	}
	else if (PL_strcmp(mime_type, "application/x-tar") == 0)
	{
		return kINetPluginTARCID;
	}
	else if (PL_strcmp(mime_type, "text/html") == 0)
	{
		return kINetPluginHTMLCID;
	}
	else if (PL_strcmp(mime_type, "text/plain") == 0)
	{
		return kINetPluginPLAINCID;
	}
	else
	{
		return kINetPluginCID;
	}
}


extern "C" void *
MAGIC_FindAndLoadDLL(const nsIID& aIID, const char *index_str)
{
	char *path;
	PRLibrary *the_dll;
	nsFactoryProc get_factory;
	nsIFactory *factory;
	nsINetPluginInstance *instance;

	path = (char *)PR_Malloc(8192);
	if (path == NULL)
	{
		return nsnull;
	}
	if (PL_strcmp(index_str, "image/x-ebm") == 0)
	{
		PL_strcpy(path, "/home/ebina/plugins/");
		PL_strcat(path, "ebm.so");
	}
	else if (PL_strcmp(index_str, "application/x-tar") == 0)
	{
		PL_strcpy(path, "/home/ebina/plugins/");
		PL_strcat(path, "tar.so");
	}
	else if (PL_strcmp(index_str, "text/html") == 0)
	{
		PL_strcpy(path, "/home/ebina/plugins/");
		PL_strcat(path, "html.so");
	}
	else if (PL_strcmp(index_str, "text/plain") == 0)
	{
		PL_strcpy(path, "/home/ebina/plugins/");
		PL_strcat(path, "plain.so");
	}
	else
	{
		return NULL;
	}

	the_dll = PR_LoadLibrary(path);
	if (the_dll == NULL)
	{
		return nsnull;
	}

	get_factory = (nsFactoryProc)PR_FindSymbol(the_dll, "NSGetFactory");
	if (nsnull == get_factory)
	{
		return nsnull;
	}

	if (NS_OK == (get_factory)(kIFactoryIID, (nsISupports *)nsnull, &factory))
	{
		factory->CreateInstance(nsnull, kINetPluginInstanceIID, (void **)&instance);
		NS_RELEASE(factory);
	}
	return (void *)instance;
}

