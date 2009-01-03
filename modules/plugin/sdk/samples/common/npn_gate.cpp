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

// Implementation of Netscape entry points (NPN_*)

#include "npplat.h"

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

  if (navMinorVers >= NPVERS_HAS_NOTIFICATION)
		rv = (*NPNFuncs.geturlnotify)(instance, url, target, notifyData);
	else
		rv = NPERR_INCOMPATIBLE_VERSION_ERROR;

  return rv;
}

NPError NPN_GetURL(NPP instance, const char *url, const char *target)
{
  return (*NPNFuncs.geturl)(instance, url, target);
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file, void* notifyData)
{
	int navMinorVers = NPNFuncs.version & 0xFF;
  NPError rv = NPERR_NO_ERROR;

	if (navMinorVers >= NPVERS_HAS_NOTIFICATION)
		rv = (*NPNFuncs.posturlnotify)(instance, url, window, len, buf, file, notifyData);
	else
		rv = NPERR_INCOMPATIBLE_VERSION_ERROR;

  return rv;
}

NPError NPN_PostURL(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file)
{
  return (*NPNFuncs.posturl)(instance, url, window, len, buf, file);
} 

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
  return (*NPNFuncs.requestread)(stream, rangeList);
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
	int navMinorVersion = NPNFuncs.version & 0xFF;

  NPError rv = NPERR_NO_ERROR;

	if (navMinorVersion >= NPVERS_HAS_STREAMOUTPUT)
		rv = (*NPNFuncs.newstream)(instance, type, target, stream);
	else
		rv = NPERR_INCOMPATIBLE_VERSION_ERROR;

  return rv;
}

int32_t NPN_Write(NPP instance, NPStream *stream, int32_t len, void *buffer)
{
	int navMinorVersion = NPNFuncs.version & 0xFF;
  int32_t rv = 0;

  if (navMinorVersion >= NPVERS_HAS_STREAMOUTPUT)
		rv = (*NPNFuncs.write)(instance, stream, len, buffer);
	else
		rv = -1;

  return rv;
}

NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
	int navMinorVersion = NPNFuncs.version & 0xFF;
  NPError rv = NPERR_NO_ERROR;

  if (navMinorVersion >= NPVERS_HAS_STREAMOUTPUT)
		rv = (*NPNFuncs.destroystream)(instance, stream, reason);
	else
		rv = NPERR_INCOMPATIBLE_VERSION_ERROR;

  return rv;
}

void NPN_Status(NPP instance, const char *message)
{
  (*NPNFuncs.status)(instance, message);
}

const char* NPN_UserAgent(NPP instance)
{
  return (*NPNFuncs.uagent)(instance);
}

void* NPN_MemAlloc(uint32_t size)
{
  return (*NPNFuncs.memalloc)(size);
}

void NPN_MemFree(void* ptr)
{
  (*NPNFuncs.memfree)(ptr);
}

uint32_t NPN_MemFlush(uint32_t size)
{
  return (*NPNFuncs.memflush)(size);
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
  (*NPNFuncs.reloadplugins)(reloadPages);
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
  return (*NPNFuncs.getvalue)(instance, variable, value);
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
  return (*NPNFuncs.setvalue)(instance, variable, value);
}

void NPN_InvalidateRect(NPP instance, NPRect *invalidRect)
{
  (*NPNFuncs.invalidaterect)(instance, invalidRect);
}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
  (*NPNFuncs.invalidateregion)(instance, invalidRegion);
}

void NPN_ForceRedraw(NPP instance)
{
  (*NPNFuncs.forceredraw)(instance);
}
