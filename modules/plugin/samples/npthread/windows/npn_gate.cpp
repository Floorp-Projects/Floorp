/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "npapi.h"
#include "npupp.h"
#include "dbg.h"

extern NPNetscapeFuncs NPNFuncs;

void NPN_Version(int* plugin_major, int* plugin_minor, int* netscape_major, int* netscape_minor)
{
  dbgOut1("wrapper: NPN_Version");

  *plugin_major   = NP_VERSION_MAJOR;
  *plugin_minor   = NP_VERSION_MINOR;
  *netscape_major = HIBYTE(NPNFuncs.version);
  *netscape_minor = LOBYTE(NPNFuncs.version);
}

NPError NPN_GetURLNotify(NPP instance, const char *url, const char *target, void* notifyData)
{
  dbgOut1("wrapper: NPN_GetURLNotify");

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
  dbgOut1("wrapper: NPN_GetURL");

  NPError rv = NPNFuncs.geturl(instance, url, target);
  return rv;
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file, void* notifyData)
{
  dbgOut1("wrapper: NPN_PostURLNotify");

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
  dbgOut1("wrapper: NPN_PostURL");

  NPError rv = NPNFuncs.posturl(instance, url, window, len, buf, file);
  return rv;
} 

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
  dbgOut1("wrapper: NPN_RequestRead");

  NPError rv = NPNFuncs.requestread(stream, rangeList);
  return rv;
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
  dbgOut1("wrapper: NPN_NewStream");

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
  dbgOut1("wrapper: NPN_Write");

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
  dbgOut1("wrapper: NPN_DestroyStream");

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
  dbgOut1("wrapper: NPN_Status");

  NPNFuncs.status(instance, message);
}

const char* NPN_UserAgent(NPP instance)
{
  dbgOut1("wrapper: NPN_UserAgent");

  const char * rv = NULL;
  rv = NPNFuncs.uagent(instance);
  return rv;
}

void* NPN_MemAlloc(uint32 size)
{
  //bgOut1("wrapper: NPN_MemAlloc");

  void * rv = NULL;
  rv = NPNFuncs.memalloc(size);
  return rv;
}

void NPN_MemFree(void* ptr)
{
  //dbgOut1("wrapper: NPN_MemFree");

  NPNFuncs.memfree(ptr);
}

uint32 NPN_MemFlush(uint32 size)
{
  dbgOut1("wrapper: NPN_MemFlush");

  uint32 rv = NPNFuncs.memflush(size);
  return rv;
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
  dbgOut1("wrapper: NPN_ReloadPlugins");

  NPNFuncs.reloadplugins(reloadPages);
}

JRIEnv* NPN_GetJavaEnv(void)
{
  dbgOut1("wrapper: NPN_GetJavaEnv");

  JRIEnv * rv = NULL;
	rv = NPNFuncs.getJavaEnv();
  return rv;
}

jref NPN_GetJavaPeer(NPP instance)
{
  dbgOut1("wrapper: NPN_GetJavaPeer");

  jref rv;
	rv = NPNFuncs.getJavaPeer(instance);
  return rv;
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
  dbgOut1("wrapper: NPN_GetValue");

  NPError rv = NPERR_NO_ERROR;
  rv = NPNFuncs.getvalue(instance, variable, value);
  return rv;
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
  dbgOut1("wrapper: NPN_SetValue");

  NPError rv = NPERR_NO_ERROR;
  rv = NPNFuncs.setvalue(instance, variable, value);
  return rv;
}

void NPN_InvalidateRect(NPP instance, NPRect *invalidRect)
{
  dbgOut1("wrapper: NPN_InvalidateRect");

  NPNFuncs.invalidaterect(instance, invalidRect);
}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
  dbgOut1("wrapper: NPN_InvalidateRegion");

  NPNFuncs.invalidateregion(instance, invalidRegion);
}

void NPN_ForceRedraw(NPP instance)
{
  dbgOut1("wrapper: ForceRedraw");

  NPNFuncs.forceredraw(instance);
}
