//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//
// npmac.cpp
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

#include <string.h>

#include <Carbon/Carbon.h>

#include "npapi.h"
#include "npfunctions.h"

//
// Define PLUGIN_TRACE to 1 to have the wrapper functions emit
// DebugStr messages whenever they are called.
//
//#define PLUGIN_TRACE 1

#if PLUGIN_TRACE
#define PLUGINDEBUGSTR(msg)		::DebugStr(msg)
#else
#define PLUGINDEBUGSTR(msg)    ((void) 0)
#endif


#ifdef __ppc__

// glue for mapping outgoing Macho function pointers to TVectors
struct TFPtoTVGlue{
    void* glue[2];
};

struct pluginFuncsGlueTable {
    TFPtoTVGlue     newp;
    TFPtoTVGlue     destroy;
    TFPtoTVGlue     setwindow;
    TFPtoTVGlue     newstream;
    TFPtoTVGlue     destroystream;
    TFPtoTVGlue     asfile;
    TFPtoTVGlue     writeready;
    TFPtoTVGlue     write;
    TFPtoTVGlue     print;
    TFPtoTVGlue     event;
    TFPtoTVGlue     urlnotify;
    TFPtoTVGlue     getvalue;
    TFPtoTVGlue     setvalue;

    TFPtoTVGlue     shutdown;
} gPluginFuncsGlueTable;

static inline void* SetupFPtoTVGlue(TFPtoTVGlue* functionGlue, void* fp)
{
    functionGlue->glue[0] = fp;
    functionGlue->glue[1] = 0;
    return functionGlue;
}

#define PLUGIN_TO_HOST_GLUE(name, fp) (SetupFPtoTVGlue(&gPluginFuncsGlueTable.name, (void*)fp))

// glue for mapping netscape TVectors to Macho function pointers
struct TTVtoFPGlue {
    uint32_t glue[6];
};

struct netscapeFuncsGlueTable {
    TTVtoFPGlue             geturl;
    TTVtoFPGlue             posturl;
    TTVtoFPGlue             requestread;
    TTVtoFPGlue             newstream;
    TTVtoFPGlue             write;
    TTVtoFPGlue             destroystream;
    TTVtoFPGlue             status;
    TTVtoFPGlue             uagent;
    TTVtoFPGlue             memalloc;
    TTVtoFPGlue             memfree;
    TTVtoFPGlue             memflush;
    TTVtoFPGlue             reloadplugins;
    TTVtoFPGlue             getJavaEnv;
    TTVtoFPGlue             getJavaPeer;
    TTVtoFPGlue             geturlnotify;
    TTVtoFPGlue             posturlnotify;
    TTVtoFPGlue             getvalue;
    TTVtoFPGlue             setvalue;
    TTVtoFPGlue             invalidaterect;
    TTVtoFPGlue             invalidateregion;
    TTVtoFPGlue             forceredraw;
    TTVtoFPGlue             pushpopupsenabledstate;
    TTVtoFPGlue             poppopupsenabledstate;
} gNetscapeFuncsGlueTable;

static void* SetupTVtoFPGlue(TTVtoFPGlue* functionGlue, void* tvp)
{
    static const TTVtoFPGlue glueTemplate = { 0x3D800000, 0x618C0000, 0x800C0000, 0x804C0004, 0x7C0903A6, 0x4E800420 };

    memcpy(functionGlue, &glueTemplate, sizeof(TTVtoFPGlue));
    functionGlue->glue[0] |= ((UInt32)tvp >> 16);
    functionGlue->glue[1] |= ((UInt32)tvp & 0xFFFF);
    ::MakeDataExecutable(functionGlue, sizeof(TTVtoFPGlue));
    return functionGlue;
}

#define HOST_TO_PLUGIN_GLUE(name, fp) (SetupTVtoFPGlue(&gNetscapeFuncsGlueTable.name, (void*)fp))

#else

#define PLUGIN_TO_HOST_GLUE(name, fp) (fp)
#define HOST_TO_PLUGIN_GLUE(name, fp) (fp)

#endif /* __ppc__ */


#pragma mark -


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//
// Globals
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

short			gResFile;			// Refnum of the pluginÕs resource file
NPNetscapeFuncs	gNetscapeFuncs;		// Function table for procs in Netscape called by plugin

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//
// Wrapper functions for all calls from the plugin to Netscape.
// These functions let the plugin developer just call the APIs
// as documented and defined in npapi.h.
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::


void NPN_Version(int* plugin_major, int* plugin_minor, int* netscape_major, int* netscape_minor)
{
	*plugin_major = NP_VERSION_MAJOR;
	*plugin_minor = NP_VERSION_MINOR;
	*netscape_major = gNetscapeFuncs.version >> 8;		// Major version is in high byte
	*netscape_minor = gNetscapeFuncs.version & 0xFF;	// Minor version is in low byte
}

NPError NPN_GetURLNotify(NPP instance, const char* url, const char* window, void* notifyData)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
	{
		err = (*gNetscapeFuncs.geturlnotify)(instance, url, window, notifyData);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

NPError NPN_GetURL(NPP instance, const char* url, const char* window)
{
	return (*gNetscapeFuncs.geturl)(instance, url, window);
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file, void* notifyData)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
	{
		err = (*gNetscapeFuncs.posturlnotify)(instance, url, window, len, buf, file, notifyData);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

NPError NPN_PostURL(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file)
{
	return (*gNetscapeFuncs.posturl)(instance, url, window, len, buf, file);
}

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
	return (*gNetscapeFuncs.requestread)(stream, rangeList);
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* window, NPStream** stream)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_STREAMOUTPUT )
	{
		err = (*gNetscapeFuncs.newstream)(instance, type, window, stream);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

int32_t NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_STREAMOUTPUT )
	{
		err = (*gNetscapeFuncs.write)(instance, stream, len, buffer);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

NPError	NPN_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_STREAMOUTPUT )
	{
		err = (*gNetscapeFuncs.destroystream)(instance, stream, reason);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

void NPN_Status(NPP instance, const char* message)
{
	(*gNetscapeFuncs.status)(instance, message);
}

const char* NPN_UserAgent(NPP instance)
{
	return (*gNetscapeFuncs.uagent)(instance);
}

void* NPN_MemAlloc(uint32_t size)
{
	return (*gNetscapeFuncs.memalloc)(size);
}

void NPN_MemFree(void* ptr)
{
	(*gNetscapeFuncs.memfree)(ptr);
}

uint32_t NPN_MemFlush(uint32_t size)
{
	return (*gNetscapeFuncs.memflush)(size);
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
	(*gNetscapeFuncs.reloadplugins)(reloadPages);
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
	return (*gNetscapeFuncs.getvalue)(instance, variable, value);
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
	return (*gNetscapeFuncs.setvalue)(instance, variable, value);
}

void NPN_InvalidateRect(NPP instance, NPRect *rect)
{
	(*gNetscapeFuncs.invalidaterect)(instance, rect);
}

void NPN_InvalidateRegion(NPP instance, NPRegion region)
{
	(*gNetscapeFuncs.invalidateregion)(instance, region);
}

void NPN_ForceRedraw(NPP instance)
{
	(*gNetscapeFuncs.forceredraw)(instance);
}

void NPN_PushPopupsEnabledState(NPP instance, NPBool enabled)
{
	(*gNetscapeFuncs.pushpopupsenabledstate)(instance, enabled);
}

void NPN_PopPopupsEnabledState(NPP instance)
{
	(*gNetscapeFuncs.poppopupsenabledstate)(instance);
}

#pragma mark -

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//
// Wrapper functions for all calls from Netscape to the plugin.
// These functions let the plugin developer just create the APIs
// as documented and defined in npapi.h, without needing to 
// install those functions in the function table or worry about
// setting up globals for 68K plugins.
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

NPError 	Private_Initialize(void);
void 		Private_Shutdown(void);
NPError		Private_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved);
NPError 	Private_Destroy(NPP instance, NPSavedData** save);
NPError		Private_SetWindow(NPP instance, NPWindow* window);
NPError		Private_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype);
NPError		Private_DestroyStream(NPP instance, NPStream* stream, NPError reason);
int32_t		Private_WriteReady(NPP instance, NPStream* stream);
int32_t		Private_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer);
void		  Private_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void	  	Private_Print(NPP instance, NPPrint* platformPrint);
int16_t 	Private_HandleEvent(NPP instance, void* event);
void      Private_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData);


NPError Private_Initialize(void)
{
	PLUGINDEBUGSTR("\pInitialize;g;");
	return NPP_Initialize();
}

void Private_Shutdown(void)
{
	PLUGINDEBUGSTR("\pShutdown;g;");
	NPP_Shutdown();
}


NPError	Private_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
	PLUGINDEBUGSTR("\pNew;g;");
	return NPP_New(pluginType, instance, mode, argc, argn, argv, saved);
}

NPError Private_Destroy(NPP instance, NPSavedData** save)
{
	PLUGINDEBUGSTR("\pDestroy;g;");
	return NPP_Destroy(instance, save);
}

NPError Private_SetWindow(NPP instance, NPWindow* window)
{
	PLUGINDEBUGSTR("\pSetWindow;g;");
	return NPP_SetWindow(instance, window);
}

NPError Private_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
	PLUGINDEBUGSTR("\pNewStream;g;");
	return NPP_NewStream(instance, type, stream, seekable, stype);
}

int32_t Private_WriteReady(NPP instance, NPStream* stream)
{
	PLUGINDEBUGSTR("\pWriteReady;g;");
	return NPP_WriteReady(instance, stream);
}

int32_t Private_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer)
{
	PLUGINDEBUGSTR("\pWrite;g;");
	return NPP_Write(instance, stream, offset, len, buffer);
}

void Private_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
	PLUGINDEBUGSTR("\pStreamAsFile;g;");
	NPP_StreamAsFile(instance, stream, fname);
}


NPError Private_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
	PLUGINDEBUGSTR("\pDestroyStream;g;");
	return NPP_DestroyStream(instance, stream, reason);
}

int16_t Private_HandleEvent(NPP instance, void* event)
{
	PLUGINDEBUGSTR("\pHandleEvent;g;");
	return NPP_HandleEvent(instance, event);
}

void Private_Print(NPP instance, NPPrint* platformPrint)
{
	PLUGINDEBUGSTR("\pPrint;g;");
	NPP_Print(instance, platformPrint);
}

void Private_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
	PLUGINDEBUGSTR("\pURLNotify;g;");
	NPP_URLNotify(instance, url, reason, notifyData);
}

void SetUpQD(void);
void SetUpQD(void)
{
	//
	// Memorize the pluginÕs resource file 
	// refnum for later use.
	//
	gResFile = CurResFile();
}


int main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs, NPP_ShutdownProcPtr* unloadProcPtr);

DEFINE_API_C(int) main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs, NPP_ShutdownProcPtr* unloadProcPtr)
{
	PLUGINDEBUGSTR("\pmain");

	NPError err = NPERR_NO_ERROR;
	
	//
	// Ensure that everything Netscape passed us is valid!
	//
	if ((nsTable == NULL) || (pluginFuncs == NULL) || (unloadProcPtr == NULL))
		err = NPERR_INVALID_FUNCTABLE_ERROR;
	
	//
	// Check the ÒmajorÓ version passed in NetscapeÕs function table.
	// We wonÕt load if the major version is newer than what we expect.
	// Also check that the function tables passed in are big enough for
	// all the functions we need (they could be bigger, if Netscape added
	// new APIs, but thatÕs OK with us -- weÕll just ignore them).
	//
	if (err == NPERR_NO_ERROR)
	{
		if ((nsTable->version >> 8) > NP_VERSION_MAJOR)		// Major version is in high byte
			err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
		
	
	if (err == NPERR_NO_ERROR)
	{
		//
		// Copy all the fields of NetscapeÕs function table into our
		// copy so we can call back into Netscape later.  Note that
		// we need to copy the fields one by one, rather than assigning
		// the whole structure, because the Netscape function table
		// could actually be bigger than what we expect.
		//
		
		int navMinorVers = nsTable->version & 0xFF;

		gNetscapeFuncs.version          = nsTable->version;
		gNetscapeFuncs.size             = nsTable->size;
		gNetscapeFuncs.posturl          = (NPN_PostURLProcPtr)HOST_TO_PLUGIN_GLUE(posturl, nsTable->posturl);
		gNetscapeFuncs.geturl           = (NPN_GetURLProcPtr)HOST_TO_PLUGIN_GLUE(geturl, nsTable->geturl);
		gNetscapeFuncs.requestread      = (NPN_RequestReadProcPtr)HOST_TO_PLUGIN_GLUE(requestread, nsTable->requestread);
		gNetscapeFuncs.newstream        = (NPN_NewStreamProcPtr)HOST_TO_PLUGIN_GLUE(newstream, nsTable->newstream);
		gNetscapeFuncs.write            = (NPN_WriteProcPtr)HOST_TO_PLUGIN_GLUE(write, nsTable->write);
		gNetscapeFuncs.destroystream    = (NPN_DestroyStreamProcPtr)HOST_TO_PLUGIN_GLUE(destroystream, nsTable->destroystream);
		gNetscapeFuncs.status           = (NPN_StatusProcPtr)HOST_TO_PLUGIN_GLUE(status, nsTable->status);
		gNetscapeFuncs.uagent           = (NPN_UserAgentProcPtr)HOST_TO_PLUGIN_GLUE(uagent, nsTable->uagent);
		gNetscapeFuncs.memalloc         = (NPN_MemAllocProcPtr)HOST_TO_PLUGIN_GLUE(memalloc, nsTable->memalloc);
		gNetscapeFuncs.memfree          = (NPN_MemFreeProcPtr)HOST_TO_PLUGIN_GLUE(memfree, nsTable->memfree);
		gNetscapeFuncs.memflush         = (NPN_MemFlushProcPtr)HOST_TO_PLUGIN_GLUE(memflush, nsTable->memflush);
		gNetscapeFuncs.reloadplugins    = (NPN_ReloadPluginsProcPtr)HOST_TO_PLUGIN_GLUE(reloadplugins, nsTable->reloadplugins);
		if( navMinorVers >= NPVERS_HAS_LIVECONNECT )
		{
			gNetscapeFuncs.getJavaEnv     = NULL;
			gNetscapeFuncs.getJavaPeer    = NULL;
		}
		if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
		{	
			gNetscapeFuncs.geturlnotify 	= (NPN_GetURLNotifyProcPtr)HOST_TO_PLUGIN_GLUE(geturlnotify, nsTable->geturlnotify);
			gNetscapeFuncs.posturlnotify 	= (NPN_PostURLNotifyProcPtr)HOST_TO_PLUGIN_GLUE(posturlnotify, nsTable->posturlnotify);
		}
		gNetscapeFuncs.getvalue         = (NPN_GetValueProcPtr)HOST_TO_PLUGIN_GLUE(getvalue, nsTable->getvalue);
		gNetscapeFuncs.setvalue         = (NPN_SetValueProcPtr)HOST_TO_PLUGIN_GLUE(setvalue, nsTable->setvalue);
		gNetscapeFuncs.invalidaterect   = (NPN_InvalidateRectProcPtr)HOST_TO_PLUGIN_GLUE(invalidaterect, nsTable->invalidaterect);
		gNetscapeFuncs.invalidateregion = (NPN_InvalidateRegionProcPtr)HOST_TO_PLUGIN_GLUE(invalidateregion, nsTable->invalidateregion);
		gNetscapeFuncs.forceredraw      = (NPN_ForceRedrawProcPtr)HOST_TO_PLUGIN_GLUE(forceredraw, nsTable->forceredraw);
		gNetscapeFuncs.pushpopupsenabledstate = (NPN_PushPopupsEnabledStateProcPtr)HOST_TO_PLUGIN_GLUE(pushpopupsenabledstate, nsTable->pushpopupsenabledstate);
		gNetscapeFuncs.poppopupsenabledstate  = (NPN_PopPopupsEnabledStateProcPtr)HOST_TO_PLUGIN_GLUE(poppopupsenabledstate, nsTable->poppopupsenabledstate);
		
		//
		// Set up the plugin function table that Netscape will use to
		// call us.  Netscape needs to know about our version and size
		// and have a UniversalProcPointer for every function we implement.
		//
		pluginFuncs->version        = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
		pluginFuncs->size           = sizeof(NPPluginFuncs);
		pluginFuncs->newp           = (NPP_NewProcPtr)(PLUGIN_TO_HOST_GLUE(newp, Private_New));
		pluginFuncs->destroy        = (NPP_DestroyProcPtr)(PLUGIN_TO_HOST_GLUE(destroy, Private_Destroy));
		pluginFuncs->setwindow      = (NPP_SetWindowProcPtr)(PLUGIN_TO_HOST_GLUE(setwindow, Private_SetWindow));
		pluginFuncs->newstream      = (NPP_NewStreamProcPtr)(PLUGIN_TO_HOST_GLUE(newstream, Private_NewStream));
		pluginFuncs->destroystream  = (NPP_DestroyStreamProcPtr)(PLUGIN_TO_HOST_GLUE(destroystream, Private_DestroyStream));
		pluginFuncs->asfile         = (NPP_StreamAsFileProcPtr)(PLUGIN_TO_HOST_GLUE(asfile, Private_StreamAsFile));
		pluginFuncs->writeready     = (NPP_WriteReadyProcPtr)(PLUGIN_TO_HOST_GLUE(writeready, Private_WriteReady));
		pluginFuncs->write          = (NPP_WriteProcPtr)(PLUGIN_TO_HOST_GLUE(write, Private_Write));
		pluginFuncs->print          = (NPP_PrintProcPtr)(PLUGIN_TO_HOST_GLUE(print, Private_Print));
		pluginFuncs->event          = (NPP_HandleEventProcPtr)(PLUGIN_TO_HOST_GLUE(event, Private_HandleEvent));	
		if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
		{	
			pluginFuncs->urlnotify = (NPP_URLNotifyProcPtr)(PLUGIN_TO_HOST_GLUE(urlnotify, Private_URLNotify));			
		}
		if( navMinorVers >= NPVERS_HAS_LIVECONNECT )
		{
			pluginFuncs->javaClass	= NULL;
		}
		*unloadProcPtr = (NPP_ShutdownProcPtr)(PLUGIN_TO_HOST_GLUE(shutdown, Private_Shutdown));

		SetUpQD();
		err = Private_Initialize();
	}
	
	return err;
}
