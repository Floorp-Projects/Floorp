/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

////////////////////////////////////////////////////////////
//
// Implementation of Netscape entry points (NPN_*)
//
#include "npapi.h"
#include "npupp.h"

extern NPNetscapeFuncs NPNFuncs;

void NPN_Version(int* plugin_major, int* plugin_minor, int* netscape_major, int* netscape_minor)
{
  *plugin_major   = NP_VERSION_MAJOR;
  *plugin_minor   = NP_VERSION_MINOR;
  *netscape_major = HIBYTE(NPNFuncs.version);
  *netscape_minor = LOBYTE(NPNFuncs.version);
}

NPError NPN_GetURLNotify(NPP instance, const char *url, const char *target, void* notifyData)
{
	int navMinorVers = NPNFuncs.version & 0xFF;
  NPError rv = NPERR_NO_ERROR;

  if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
		rv = NPNFuncs.geturlnotify(instance, url, target, notifyData);
	else
		rv = NPERR_INCOMPATIBLE_VERSION_ERROR;

  return rv;
}

NPError NPN_GetURL(NPP instance, const char *url, const char *target)
{
  NPError rv = NPNFuncs.geturl(instance, url, target);
  return rv;
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file, void* notifyData)
{
	int navMinorVers = NPNFuncs.version & 0xFF;
  NPError rv = NPERR_NO_ERROR;

	if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
		rv = NPNFuncs.posturlnotify(instance, url, window, len, buf, file, notifyData);
	else
		rv = NPERR_INCOMPATIBLE_VERSION_ERROR;

  return rv;
}

NPError NPN_PostURL(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file)
{
  NPError rv = NPNFuncs.posturl(instance, url, window, len, buf, file);
  return rv;
} 

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
  NPError rv = NPNFuncs.requestread(stream, rangeList);
  return rv;
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
	int navMinorVersion = NPNFuncs.version & 0xFF;

  NPError rv = NPERR_NO_ERROR;

	if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT )
		rv = NPNFuncs.newstream(instance, type, target, stream);
	else
		rv = NPERR_INCOMPATIBLE_VERSION_ERROR;

  return rv;
}

int32 NPN_Write(NPP instance, NPStream *stream, int32 len, void *buffer)
{
	int navMinorVersion = NPNFuncs.version & 0xFF;
  int32 rv = 0;

  if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT )
		rv = NPNFuncs.write(instance, stream, len, buffer);
	else
		rv = -1;

  return rv;
}

NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
	int navMinorVersion = NPNFuncs.version & 0xFF;
  NPError rv = NPERR_NO_ERROR;

  if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT )
		rv = NPNFuncs.destroystream(instance, stream, reason);
	else
		rv = NPERR_INCOMPATIBLE_VERSION_ERROR;

  return rv;
}

void NPN_Status(NPP instance, const char *message)
{
  NPNFuncs.status(instance, message);
}

const char* NPN_UserAgent(NPP instance)
{
  const char * rv = NULL;
  rv = NPNFuncs.uagent(instance);
  return rv;
}

void* NPN_MemAlloc(uint32 size)
{
  void * rv = NULL;
  rv = NPNFuncs.memalloc(size);
  return rv;
}

void NPN_MemFree(void* ptr)
{
  NPNFuncs.memfree(ptr);
}

uint32 NPN_MemFlush(uint32 size)
{
  uint32 rv = NPNFuncs.memflush(size);
  return rv;
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
  NPNFuncs.reloadplugins(reloadPages);
}

JRIEnv* NPN_GetJavaEnv(void)
{
  JRIEnv * rv = NULL;
	rv = NPNFuncs.getJavaEnv();
  return rv;
}

jref NPN_GetJavaPeer(NPP instance)
{
  jref rv;
	rv = NPNFuncs.getJavaPeer(instance);
  return rv;
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
  NPError rv = NPNFuncs.getvalue(instance, variable, value);
  return rv;
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
  NPError rv = NPNFuncs.setvalue(instance, variable, value);
  return rv;
}

void NPN_InvalidateRect(NPP instance, NPRect *invalidRect)
{
  NPNFuncs.invalidaterect(instance, invalidRect);
}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
  NPNFuncs.invalidateregion(instance, invalidRegion);
}

void NPN_ForceRedraw(NPP instance)
{
  NPNFuncs.forceredraw(instance);
}
