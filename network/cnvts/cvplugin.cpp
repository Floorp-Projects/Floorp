/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#include "xp.h"
#include "np.h"
#include "prmem.h"
#include "plstr.h"
#include "nsINetPlugin.h"
#include "nsINetPluginInstance.h"
#include "nsIComponentManager.h"
#include "nsNetConverterStream.h"


static NS_DEFINE_CID(kINetPluginCID, NS_INET_PLUGIN_CID);

static NS_DEFINE_IID(kINetPluginInstanceIID, NS_INETPLUGININSTANCE_IID);
static NS_DEFINE_IID(kINetOStreamIID, NS_INETOSTREAM_IID);


PRIVATE void
plugin_stream_complete(NET_StreamClass *stream)
{
	nsINetOStream *instance_stream = (nsINetOStream *)stream->data_object;

	instance_stream->Complete();
}


PRIVATE void
plugin_stream_abort(NET_StreamClass *stream, int status)
{
	nsINetOStream *instance_stream = (nsINetOStream *)stream->data_object;

	instance_stream->Abort(status);
}


PRIVATE int
plugin_stream_write(NET_StreamClass *stream, const char* str, int32 len)
{
	nsINetOStream *instance_stream = (nsINetOStream *)stream->data_object;
	nsresult res;
	int32 written;

	res = instance_stream->Write(str, (unsigned int)len, (unsigned int *)&written);
	if (res == NS_OK)
	{
		return written;
	}
	return -1;
}


PRIVATE unsigned int
plugin_stream_write_ready(NET_StreamClass *stream)
{
	nsINetOStream *instance_stream = (nsINetOStream *)stream->data_object;
	nsresult res;
	unsigned int ready;

	res = instance_stream->WriteReady(&ready);
	if (res == NS_OK)
	{
		return ready;
	}
	return 0;
}


typedef struct UsedConverter {
	void *data_id;
	char *mime_type;
	struct UsedConverter *next;
} UsedConverter;
UsedConverter *RegisteredConverters = NULL;

PRIVATE PRBool
register_converter(char *mime_type, void *data_id)
{
	UsedConverter *converter = RegisteredConverters;

	if ((mime_type == NULL)||(data_id == NULL))
	{
		return PR_FALSE;
	}

	while (converter != NULL)
	{
		if ((converter->data_id == data_id)&&
		    (PL_strcmp(converter->mime_type, mime_type) == 0))
		{
			return PR_FALSE;
		}
		converter = converter->next;
	}
	converter = PR_NEWZAP(UsedConverter);
	converter->mime_type = PL_strdup(mime_type);
	converter->data_id = data_id;
	converter->next = RegisteredConverters;
	RegisteredConverters = converter;
	return PR_TRUE;
}


PRIVATE void
unregister_converter(char *mime_type, void *data_id)
{
	UsedConverter *converter = RegisteredConverters;
	UsedConverter *last_converter = NULL;

	if ((mime_type == NULL)||(data_id == NULL))
	{
		return;
	}
	while (converter != NULL)
	{
		if ((converter->data_id == data_id)&&
		    (PL_strcmp(converter->mime_type, mime_type) == 0))
		{
			if (last_converter == NULL)
			{
				RegisteredConverters = converter->next;
			}
			else
			{
				last_converter->next = converter->next;
			}
			PR_FREEIF(converter->mime_type);
			PR_FREEIF(converter);
			return;
		}
		last_converter = converter;
		converter = converter->next;
	}
}


extern "C" NET_StreamClass * NET_PluginStream(int fmt, void* data_obj, URL_Struct* URL_s, MWContext* w);

extern "C" NET_StreamClass *
NET_PluginStream(int fmt, void* data_obj, URL_Struct* URL_s, MWContext* w)
{
	PRBool can_use, error = PR_FALSE;
	char *mime_type;
	const char *mime_type_out;
	NET_StreamClass *new_stream = NULL;
	NET_StreamClass *next_stream = NULL;
	nsINetPluginInstance *plugin_inst;
	nsINetOStream *instance_stream;
	nsINetOStream *out_stream;
	nsNetConverterStream *converter_stream;
	nsCID classID = {0};

	if (URL_s->transfer_encoding != NULL)
	{
		mime_type = XP_STRDUP(URL_s->transfer_encoding);
		PR_FREEIF(URL_s->transfer_encoding);
	}
	else if (URL_s->content_encoding != NULL)
	{
		mime_type = XP_STRDUP(URL_s->content_encoding);
		PR_FREEIF(URL_s->content_encoding);
	}
	else
	{
		mime_type = XP_STRDUP(URL_s->content_type);
	}

	can_use = register_converter(mime_type, (void *)URL_s);
	if (can_use == PR_FALSE)
	{
		PR_FREEIF(mime_type);
		return NULL;
	}

	do {
		if (nsComponentManager::ProgIDToCLSID(mime_type, &classID) != NS_OK)
		{
			error = PR_TRUE;
			break;
		}

		nsComponentManager::CreateInstance(classID, (nsISupports *)nsnull, kINetPluginInstanceIID, (void **)&plugin_inst);

		if (plugin_inst == NULL)
		{
			error = PR_TRUE;
			break;
		}

		plugin_inst->GetMIMEOutput(&mime_type_out, URL_s->address);

		if (mime_type_out != NULL)
		{
			PR_FREEIF(URL_s->content_type);
			URL_s->content_type = XP_STRDUP(mime_type_out);
		}
		next_stream = NET_StreamBuilder(fmt, URL_s, w);

		converter_stream = new nsNetConverterStream();
		if (converter_stream == NULL)
		{
			error = PR_TRUE;
			break;
		}

		converter_stream->Initialize((void *)next_stream);

		if (NS_OK != converter_stream->QueryInterface(kINetOStreamIID, (void **)&out_stream))
		{
			delete converter_stream;
			error = PR_TRUE;
			break;
		}

		plugin_inst->Initialize(out_stream, URL_s->address);

		if (NS_OK != plugin_inst->QueryInterface(kINetOStreamIID, (void **)&instance_stream))
		{
			delete converter_stream;
			error = PR_TRUE;
			break;
		}
	} while (0);

	if (error)
	{
		unregister_converter(mime_type, (void *)URL_s);
		PR_FREEIF(mime_type);
		return NULL;
	}

	new_stream = NET_NewStream("PluginStream", plugin_stream_write, plugin_stream_complete, plugin_stream_abort, plugin_stream_write_ready, (void *)instance_stream, w);

	unregister_converter(mime_type, (void *)URL_s);
	PR_FREEIF(mime_type);
	return new_stream;
}

