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
 *
 */

#include "xp.h"

#include "npapi.h"
#include "npupp.h"

#include "logger.h"

extern Logger * logger;
extern NPNetscapeFuncs NPNFuncs;

void NPN_Version(int* plugin_major, int* plugin_minor, int* netscape_major, int* netscape_minor)
{
  if(logger)
    logger->logCall(action_npn_version, (DWORD)plugin_major, (DWORD)plugin_minor, (DWORD)netscape_major, (DWORD)netscape_minor);

  *plugin_major   = NP_VERSION_MAJOR;
  *plugin_minor   = NP_VERSION_MINOR;
  *netscape_major = HIBYTE(NPNFuncs.version);
  *netscape_minor = LOBYTE(NPNFuncs.version);

  if(logger)
    logger->logReturn();
}

NPError NPN_GetURLNotify(NPP instance, const char *url, const char *target, void* notifyData)
{
	int navMinorVers = NPNFuncs.version & 0xFF;
	
  NPError rv = NPERR_NO_ERROR;

  if(logger)
    logger->logCall(action_npn_get_url_notify, (DWORD)instance, (DWORD)url, (DWORD)target, (DWORD)notifyData);

  if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
		rv = NPNFuncs.geturlnotify(instance, url, target, notifyData);
	else
		rv = NPERR_INCOMPATIBLE_VERSION_ERROR;

  if(logger)
    logger->logReturn();

  return rv;
}

NPError NPN_GetURL(NPP instance, const char *url, const char *target)
{
  if(logger)
    logger->logCall(action_npn_get_url, (DWORD)instance, (DWORD)url, (DWORD)target);

  NPError rv = NPNFuncs.geturl(instance, url, target);

  if(logger)
    logger->logReturn();

  return rv;
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file, void* notifyData)
{
	int navMinorVers = NPNFuncs.version & 0xFF;

  NPError rv = NPERR_NO_ERROR;

  if(logger)
    logger->logCall(action_npn_post_url_notify, (DWORD)instance, (DWORD)url, (DWORD)window, (DWORD)len, (DWORD)buf, (DWORD)file, (DWORD)notifyData);

	if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
		rv = NPNFuncs.posturlnotify(instance, url, window, len, buf, file, notifyData);
	else
		rv = NPERR_INCOMPATIBLE_VERSION_ERROR;

  if(logger)
    logger->logReturn();

  return rv;
}

NPError NPN_PostURL(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file)
{
  if(logger)
    logger->logCall(action_npn_post_url, (DWORD)instance, (DWORD)url, (DWORD)window, (DWORD)len, (DWORD)buf, (DWORD)file);

  NPError rv = NPNFuncs.posturl(instance, url, window, len, buf, file);

  if(logger)
    logger->logReturn();

  return rv;
} 

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
  if(logger)
    logger->logCall(action_npn_request_read, (DWORD)stream, (DWORD)rangeList);

  NPError rv = NPNFuncs.requestread(stream, rangeList);

  if(logger)
    logger->logReturn();

  return rv;
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
	int navMinorVersion = NPNFuncs.version & 0xFF;

  NPError rv = NPERR_NO_ERROR;

  if(logger)
    logger->logCall(action_npn_new_stream, (DWORD)instance, (DWORD)type, (DWORD)target, (DWORD)stream);

	if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT )
		rv = NPNFuncs.newstream(instance, type, target, stream);
	else
		rv = NPERR_INCOMPATIBLE_VERSION_ERROR;

  if(logger)
    logger->logReturn();

  return rv;
}

int32 NPN_Write(NPP instance, NPStream *stream, int32 len, void *buffer)
{
	int navMinorVersion = NPNFuncs.version & 0xFF;

  int32 rv = 0;

  if(logger)
    logger->logCall(action_npn_write, (DWORD)instance, (DWORD)stream, (DWORD)len, (DWORD)buffer);

  if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT )
		rv = NPNFuncs.write(instance, stream, len, buffer);
	else
		rv = -1;

  if(logger)
    logger->logReturn();

  return rv;
}

NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
	int navMinorVersion = NPNFuncs.version & 0xFF;

  NPError rv = NPERR_NO_ERROR;

  if(logger)
    logger->logCall(action_npn_destroy_stream, (DWORD)instance, (DWORD)stream, (DWORD)reason);

  if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT )
		rv = NPNFuncs.destroystream(instance, stream, reason);
	else
		rv = NPERR_INCOMPATIBLE_VERSION_ERROR;

  if(logger)
    logger->logReturn();

  return rv;
}

void NPN_Status(NPP instance, const char *message)
{
  if(logger)
    logger->logCall(action_npn_status, (DWORD)instance, (DWORD)message);

  NPNFuncs.status(instance, message);
}

const char* NPN_UserAgent(NPP instance)
{
  const char * rv = NULL;

  if(logger)
    logger->logCall(action_npn_user_agent, (DWORD)instance);

  rv = NPNFuncs.uagent(instance);

  if(logger)
    logger->logReturn();

  return rv;
}

void* NPN_MemAlloc(uint32 size)
{
  void * rv = NULL;
  
  if(logger)
    logger->logCall(action_npn_mem_alloc, (DWORD)size);

  rv = NPNFuncs.memalloc(size);

  if(logger)
    logger->logReturn();

  return rv;
}

void NPN_MemFree(void* ptr)
{
  if(logger)
    logger->logCall(action_npn_mem_free, (DWORD)ptr);

  NPNFuncs.memfree(ptr);
}

uint32 NPN_MemFlush(uint32 size)
{
  if(logger)
    logger->logCall(action_npn_mem_flush, (DWORD)size);

  uint32 rv = NPNFuncs.memflush(size);

  if(logger)
    logger->logReturn();

  return rv;
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
  if(logger)
    logger->logCall(action_npn_reload_plugins, (DWORD)reloadPages);

  NPNFuncs.reloadplugins(reloadPages);
}

JRIEnv* NPN_GetJavaEnv(void)
{
  JRIEnv * rv = NULL;

  if(logger)
    logger->logCall(action_npn_get_java_env);

	rv = NPNFuncs.getJavaEnv();

  if(logger)
    logger->logReturn();

  return rv;
}

jref NPN_GetJavaPeer(NPP instance)
{
  jref rv;

  if(logger)
    logger->logCall(action_npn_get_java_peer, (DWORD)instance);

	rv = NPNFuncs.getJavaPeer(instance);

  if(logger)
    logger->logReturn();

  return rv;
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
  NPError rv = NPERR_NO_ERROR;

  if(logger)
    logger->logCall(action_npn_get_value, (DWORD)instance, (DWORD)variable, (DWORD)value);

  rv = NPNFuncs.getvalue(instance, variable, value);

  if(logger)
    logger->logReturn(action_npn_get_value);

  return rv;
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
  NPError rv = NPERR_NO_ERROR;

  if(logger)
    logger->logCall(action_npn_set_value, (DWORD)instance, (DWORD)variable, (DWORD)value);

  rv = NPNFuncs.setvalue(instance, variable, value);

  if(logger)
    logger->logReturn();

  return rv;
}

void NPN_InvalidateRect(NPP instance, NPRect *invalidRect)
{
  if(logger)
    logger->logCall(action_npn_invalidate_rect, (DWORD)instance, (DWORD)invalidRect);

  NPNFuncs.invalidaterect(instance, invalidRect);
}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
  if(logger)
    logger->logCall(action_npn_invalidate_region, (DWORD)instance, (DWORD)invalidRegion);

  NPNFuncs.invalidateregion(instance, invalidRegion);
}

void NPN_ForceRedraw(NPP instance)
{
  if(logger)
    logger->logCall(action_npn_force_redraw, (DWORD)instance);

  NPNFuncs.forceredraw(instance);
}
