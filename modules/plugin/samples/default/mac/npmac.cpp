//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//
// npmac.cpp
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

#include <string.h>

#include <Carbon/Carbon.h>

#include "npapi.h"
#include "npupp.h"

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
    uint32 glue[6];
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
// as documented and defined in npapi.h, without needing to know
// about the function table and call macros in npupp.h.
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
		err = CallNPN_GetURLNotifyProc(gNetscapeFuncs.geturlnotify, instance, url, window, notifyData);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

NPError NPN_GetURL(NPP instance, const char* url, const char* window)
{
	return CallNPN_GetURLProc(gNetscapeFuncs.geturl, instance, url, window);
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file, void* notifyData)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
	{
		err = CallNPN_PostURLNotifyProc(gNetscapeFuncs.posturlnotify, instance, url, 
														window, len, buf, file, notifyData);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

NPError NPN_PostURL(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file)
{
	return CallNPN_PostURLProc(gNetscapeFuncs.posturl, instance, url, window, len, buf, file);
}

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
	return CallNPN_RequestReadProc(gNetscapeFuncs.requestread, stream, rangeList);
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* window, NPStream** stream)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_STREAMOUTPUT )
	{
		err = CallNPN_NewStreamProc(gNetscapeFuncs.newstream, instance, type, window, stream);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

int32 NPN_Write(NPP instance, NPStream* stream, int32 len, void* buffer)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_STREAMOUTPUT )
	{
		err = CallNPN_WriteProc(gNetscapeFuncs.write, instance, stream, len, buffer);
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
		err = CallNPN_DestroyStreamProc(gNetscapeFuncs.destroystream, instance, stream, reason);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

void NPN_Status(NPP instance, const char* message)
{
	CallNPN_StatusProc(gNetscapeFuncs.status, instance, message);
}

const char* NPN_UserAgent(NPP instance)
{
	return CallNPN_UserAgentProc(gNetscapeFuncs.uagent, instance);
}

void* NPN_MemAlloc(uint32 size)
{
	return CallNPN_MemAllocProc(gNetscapeFuncs.memalloc, size);
}

void NPN_MemFree(void* ptr)
{
	CallNPN_MemFreeProc(gNetscapeFuncs.memfree, ptr);
}

uint32 NPN_MemFlush(uint32 size)
{
	return CallNPN_MemFlushProc(gNetscapeFuncs.memflush, size);
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
	CallNPN_ReloadPluginsProc(gNetscapeFuncs.reloadplugins, reloadPages);
}

#ifdef OJI
JRIEnv* NPN_GetJavaEnv(void)
{
	return CallNPN_GetJavaEnvProc( gNetscapeFuncs.getJavaEnv );
}

jobject  NPN_GetJavaPeer(NPP instance)
{
	return CallNPN_GetJavaPeerProc( gNetscapeFuncs.getJavaPeer, instance );
}
#endif

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
	return CallNPN_GetValueProc( gNetscapeFuncs.getvalue, instance, variable, value);
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
	return CallNPN_SetValueProc( gNetscapeFuncs.setvalue, instance, variable, value);
}

void NPN_InvalidateRect(NPP instance, NPRect *rect)
{
	CallNPN_InvalidateRectProc( gNetscapeFuncs.invalidaterect, instance, rect);
}

void NPN_InvalidateRegion(NPP instance, NPRegion region)
{
	CallNPN_InvalidateRegionProc( gNetscapeFuncs.invalidateregion, instance, region);
}

void NPN_ForceRedraw(NPP instance)
{
	CallNPN_ForceRedrawProc( gNetscapeFuncs.forceredraw, instance);
}

void NPN_PushPopupsEnabledState(NPP instance, NPBool enabled)
{
	CallNPN_PushPopupsEnabledStateProc( gNetscapeFuncs.pushpopupsenabledstate, instance, enabled);
}

void NPN_PopPopupsEnabledState(NPP instance)
{
	CallNPN_PopPopupsEnabledStateProc( gNetscapeFuncs.poppopupsenabledstate, instance);
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
NPError		Private_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
NPError 	Private_Destroy(NPP instance, NPSavedData** save);
NPError		Private_SetWindow(NPP instance, NPWindow* window);
NPError		Private_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
NPError		Private_DestroyStream(NPP instance, NPStream* stream, NPError reason);
int32		Private_WriteReady(NPP instance, NPStream* stream);
int32		Private_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer);
void		Private_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void		Private_Print(NPP instance, NPPrint* platformPrint);
int16 		Private_HandleEvent(NPP instance, void* event);
void        Private_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData);
jobject		Private_GetJavaClass(void);


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


NPError	Private_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved)
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

NPError Private_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype)
{
	PLUGINDEBUGSTR("\pNewStream;g;");
	return NPP_NewStream(instance, type, stream, seekable, stype);
}

int32 Private_WriteReady(NPP instance, NPStream* stream)
{
	PLUGINDEBUGSTR("\pWriteReady;g;");
	return NPP_WriteReady(instance, stream);
}

int32 Private_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer)
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

int16 Private_HandleEvent(NPP instance, void* event)
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

#ifdef OJI
jobject Private_GetJavaClass(void)
{
	PLUGINDEBUGSTR("\pGetJavaClass;g;");

    jobject clazz = NPP_GetJavaClass();
    if (clazz)
    {
		JRIEnv* env = NPN_GetJavaEnv();
		return (jobject)JRI_NewGlobalRef(env, clazz);
    }
    return NULL;
}
#endif

void SetUpQD(void);
void SetUpQD(void)
{
	//
	// Memorize the pluginÕs resource file 
	// refnum for later use.
	//
	gResFile = CurResFile();
}


int main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs, NPP_ShutdownUPP* unloadUpp);

DEFINE_API_C(int) main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs, NPP_ShutdownUPP* unloadUpp)
{
	PLUGINDEBUGSTR("\pmain");

	NPError err = NPERR_NO_ERROR;
	
	//
	// Ensure that everything Netscape passed us is valid!
	//
	if ((nsTable == NULL) || (pluginFuncs == NULL) || (unloadUpp == NULL))
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
		gNetscapeFuncs.posturl          = (NPN_PostURLUPP)HOST_TO_PLUGIN_GLUE(posturl, nsTable->posturl);
		gNetscapeFuncs.geturl           = (NPN_GetURLUPP)HOST_TO_PLUGIN_GLUE(geturl, nsTable->geturl);
		gNetscapeFuncs.requestread      = (NPN_RequestReadUPP)HOST_TO_PLUGIN_GLUE(requestread, nsTable->requestread);
		gNetscapeFuncs.newstream        = (NPN_NewStreamUPP)HOST_TO_PLUGIN_GLUE(newstream, nsTable->newstream);
		gNetscapeFuncs.write            = (NPN_WriteUPP)HOST_TO_PLUGIN_GLUE(write, nsTable->write);
		gNetscapeFuncs.destroystream    = (NPN_DestroyStreamUPP)HOST_TO_PLUGIN_GLUE(destroystream, nsTable->destroystream);
		gNetscapeFuncs.status           = (NPN_StatusUPP)HOST_TO_PLUGIN_GLUE(status, nsTable->status);
		gNetscapeFuncs.uagent           = (NPN_UserAgentUPP)HOST_TO_PLUGIN_GLUE(uagent, nsTable->uagent);
		gNetscapeFuncs.memalloc         = (NPN_MemAllocUPP)HOST_TO_PLUGIN_GLUE(memalloc, nsTable->memalloc);
		gNetscapeFuncs.memfree          = (NPN_MemFreeUPP)HOST_TO_PLUGIN_GLUE(memfree, nsTable->memfree);
		gNetscapeFuncs.memflush         = (NPN_MemFlushUPP)HOST_TO_PLUGIN_GLUE(memflush, nsTable->memflush);
		gNetscapeFuncs.reloadplugins    = (NPN_ReloadPluginsUPP)HOST_TO_PLUGIN_GLUE(reloadplugins, nsTable->reloadplugins);
		if( navMinorVers >= NPVERS_HAS_LIVECONNECT )
		{
			gNetscapeFuncs.getJavaEnv   = (NPN_GetJavaEnvUPP)HOST_TO_PLUGIN_GLUE(getJavaEnv, nsTable->getJavaEnv);
			gNetscapeFuncs.getJavaPeer  = (NPN_GetJavaPeerUPP)HOST_TO_PLUGIN_GLUE(getJavaPeer, nsTable->getJavaPeer);
		}
		if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
		{	
			gNetscapeFuncs.geturlnotify 	= (NPN_GetURLNotifyUPP)HOST_TO_PLUGIN_GLUE(geturlnotify, nsTable->geturlnotify);
			gNetscapeFuncs.posturlnotify 	= (NPN_PostURLNotifyUPP)HOST_TO_PLUGIN_GLUE(posturlnotify, nsTable->posturlnotify);
		}
		gNetscapeFuncs.getvalue         = (NPN_GetValueUPP)HOST_TO_PLUGIN_GLUE(getvalue, nsTable->getvalue);
		gNetscapeFuncs.setvalue         = (NPN_SetValueUPP)HOST_TO_PLUGIN_GLUE(setvalue, nsTable->setvalue);
		gNetscapeFuncs.invalidaterect   = (NPN_InvalidateRectUPP)HOST_TO_PLUGIN_GLUE(invalidaterect, nsTable->invalidaterect);
		gNetscapeFuncs.invalidateregion = (NPN_InvalidateRegionUPP)HOST_TO_PLUGIN_GLUE(invalidateregion, nsTable->invalidateregion);
		gNetscapeFuncs.forceredraw      = (NPN_ForceRedrawUPP)HOST_TO_PLUGIN_GLUE(forceredraw, nsTable->forceredraw);
		gNetscapeFuncs.pushpopupsenabledstate = (NPN_PushPopupsEnabledStateUPP)HOST_TO_PLUGIN_GLUE(pushpopupsenabledstate, nsTable->pushpopupsenabledstate);
		gNetscapeFuncs.poppopupsenabledstate  = (NPN_PopPopupsEnabledStateUPP)HOST_TO_PLUGIN_GLUE(poppopupsenabledstate, nsTable->poppopupsenabledstate);
		
		//
		// Set up the plugin function table that Netscape will use to
		// call us.  Netscape needs to know about our version and size
		// and have a UniversalProcPointer for every function we implement.
		//
		pluginFuncs->version        = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
		pluginFuncs->size           = sizeof(NPPluginFuncs);
		pluginFuncs->newp           = NewNPP_NewProc(PLUGIN_TO_HOST_GLUE(newp, Private_New));
		pluginFuncs->destroy        = NewNPP_DestroyProc(PLUGIN_TO_HOST_GLUE(destroy, Private_Destroy));
		pluginFuncs->setwindow      = NewNPP_SetWindowProc(PLUGIN_TO_HOST_GLUE(setwindow, Private_SetWindow));
		pluginFuncs->newstream      = NewNPP_NewStreamProc(PLUGIN_TO_HOST_GLUE(newstream, Private_NewStream));
		pluginFuncs->destroystream  = NewNPP_DestroyStreamProc(PLUGIN_TO_HOST_GLUE(destroystream, Private_DestroyStream));
		pluginFuncs->asfile         = NewNPP_StreamAsFileProc(PLUGIN_TO_HOST_GLUE(asfile, Private_StreamAsFile));
		pluginFuncs->writeready     = NewNPP_WriteReadyProc(PLUGIN_TO_HOST_GLUE(writeready, Private_WriteReady));
		pluginFuncs->write          = NewNPP_WriteProc(PLUGIN_TO_HOST_GLUE(write, Private_Write));
		pluginFuncs->print          = NewNPP_PrintProc(PLUGIN_TO_HOST_GLUE(print, Private_Print));
		pluginFuncs->event          = NewNPP_HandleEventProc(PLUGIN_TO_HOST_GLUE(event, Private_HandleEvent));	
		if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
		{	
			pluginFuncs->urlnotify = NewNPP_URLNotifyProc(PLUGIN_TO_HOST_GLUE(urlnotify, Private_URLNotify));			
		}
#ifdef OJI
		if( navMinorVers >= NPVERS_HAS_LIVECONNECT )
		{
			pluginFuncs->javaClass	= (JRIGlobalRef) Private_GetJavaClass();
		}
#else
                pluginFuncs->javaClass = NULL;
#endif
		*unloadUpp = NewNPP_ShutdownProc(PLUGIN_TO_HOST_GLUE(shutdown, Private_Shutdown));

		SetUpQD();
		err = Private_Initialize();
	}
	
	return err;
}
