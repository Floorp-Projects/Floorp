/*
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

/*
	MRJPlugin.cpp
	
	XP COM Plugin Implementation.
	
	by Patrick C. Beard.
 */

#include "MRJPlugin.h"
#include "MRJSession.h"
#include "MRJContext.h"
#include "MRJFrame.h"
#include "MRJConsole.h"

#include "nsIMalloc.h"
#include "nsRepository.h"
#include "nsIJVMManager.h"
#include "nsIPluginManager2.h"
#include "nsIPluginInstancePeer.h"
#include "nsIWindowlessPlugInstPeer.h"
#include "LiveConnectNativeMethods.h"
#include "CSecureJNI2.h"

extern nsIPluginManager* thePluginManager;		// now in badaptor.cpp.
extern nsIPlugin* thePlugin;

nsIPluginManager2* thePluginManager2 = NULL;
nsIMalloc* theMemoryAllocator = NULL;

// Common interface IDs.

static NS_DEFINE_IID(kIPluginIID, NS_IPLUGIN_IID);
static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIPluginManager2IID, NS_IPLUGINMANAGER2_IID);
static NS_DEFINE_IID(kIMallocIID, NS_IMALLOC_IID);
static NS_DEFINE_IID(kIJVMManagerIID, NS_IJVMMANAGER_IID);
static NS_DEFINE_IID(kIThreadManagerIID, NS_ITHREADMANAGER_IID);
static NS_DEFINE_IID(kIRunnableIID, NS_IRUNNABLE_IID);
static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID);
static NS_DEFINE_IID(kIJVMPluginInstanceIID, NS_IJVMPLUGININSTANCE_IID);
static NS_DEFINE_IID(kIWindowlessPluginInstancePeerIID, NS_IWINDOWLESSPLUGININSTANCEPEER_IID);

#pragma export on

/* NS_METHOD NP_CreatePlugin(NPIPluginManager* manager, NPIPlugin* *result)
{
	thePluginManager = manager;
	*result = new MRJPlugin(manager);
	return nsPluginError_NoError;
} */

nsresult NSGetFactory(const nsCID &classID, nsIFactory **aFactory)
{
	if (classID.Equals(kIPluginIID)) {
		static MRJPlugin thePluginSingleton;
		*aFactory = &thePluginSingleton;
		thePlugin = &thePluginSingleton;
		thePlugin->AddRef();
		// *aFactory = new MRJPlugin();
		return NS_OK;
	}
	return NS_NOINTERFACE;
}

#pragma export off

extern "C" void cfm_NSShutdownPlugin(void);

void cfm_NSShutdownPlugin()
{
	if (thePlugin != NULL) {
		nsrefcnt refs = thePlugin->Release();
		while (refs > 0 && thePlugin != NULL)
			refs = thePlugin->Release();
	}
}

//
// The MEAT of the plugin.
//

#pragma mark *** MRJPlugin ***

nsID MRJPlugin::sInterfaceIDs[] = { NS_IPLUGIN_IID, NS_IJVMPLUGIN_IID };

MRJPlugin::MRJPlugin()
	:	SupportsMixin((nsIJVMPlugin*)this, sInterfaceIDs, sizeof(sInterfaceIDs) / sizeof(nsID)),
		mManager(NULL), mThreadManager(NULL), mSession(NULL), mConsole(NULL), mIsEnabled(false), mPluginThreadID(0)
{
}

MRJPlugin::~MRJPlugin()
{
	// make sure the plugin is no longer visible.
	::thePlugin = NULL;

	// Release the console.
	if (mConsole != NULL) {
		mConsole->Release();
		mConsole = NULL;
	}

	// tear down the MRJ session, if it exists.
	if (mSession != NULL) {
		delete mSession;
		mSession = NULL;
	}

	// Release the manager?
	if (mManager != NULL) {
		mManager->Release();
		mManager = NULL;
	}
	
	if (mThreadManager != NULL) {
		mThreadManager->Release();
		mThreadManager = NULL;
	}
}

// PCB:  Metrowerks pre-processor bug? The following macro won't compile.
// NS_IMPL_ISUPPORTS(MRJPlugin, NP_IPLUGIN_IID)

NS_METHOD MRJPlugin::QueryInterface(const nsIID& aIID, void** instancePtr)
{
	if (aIID.Equals(kIRunnableIID)) {
		*instancePtr = (void*) (nsIRunnable*) this;
		return NS_OK;
	}
	nsresult result = queryInterface(aIID, instancePtr);
	if (result == NS_NOINTERFACE) {
		result = mConsole->queryInterface(aIID, instancePtr);
	}
	return result;
}

nsrefcnt MRJPlugin::AddRef(void) { return addRef(); }
nsrefcnt MRJPlugin::Release(void) { return release(); }

NS_METHOD MRJPlugin::CreateInstance(nsISupports *aOuter, const nsIID& aIID, void **aResult)
{
	if (aIID.Equals(kIPluginInstanceIID) || aIID.Equals(kIJVMPluginInstanceIID)) {
		if (StartupJVM(NULL) == NS_OK) {
			MRJPluginInstance* instance = new MRJPluginInstance(this);
			instance->AddRef();		// if not us, then who will take this burden?
			*aResult = instance;
			return NS_OK;
		}
		return NS_ERROR_FAILURE;
	}
	return NS_NOINTERFACE;
}

NS_METHOD MRJPlugin::Initialize(nsISupports* browserInterfaces)
{
	nsresult result = NS_OK;
	
	// try to get a plugin manager.
	if (thePluginManager == NULL) {
		result = browserInterfaces->QueryInterface(kIPluginManagerIID, &thePluginManager);
		if (result != NS_OK || thePluginManager == NULL)
			return NS_ERROR_FAILURE;
	}

	// see if the enhanced plugin manager exists.
	if (thePluginManager2 == NULL) {
		result = browserInterfaces->QueryInterface(kIPluginManager2IID, &thePluginManager2);
		if (result != NS_OK)
			thePluginManager2 = NULL;
	}

	// see if IMalloc exists.
	if (theMemoryAllocator == NULL) {
		result = browserInterfaces->QueryInterface(kIMallocIID, &theMemoryAllocator);
		if (result != NS_OK)
			theMemoryAllocator = NULL;
	}
	
	// try to get a JVM manager. we have to be able to run without one.
	result = browserInterfaces->QueryInterface(kIJVMManagerIID, &mManager);
	if (result != NS_OK)
		mManager = NULL;
	
	// try to get a Thread manager.
	result = browserInterfaces->QueryInterface(kIThreadManagerIID, &mThreadManager);
	if (result != NS_OK)
		mThreadManager = NULL;

	if (mThreadManager != NULL)
		mThreadManager->GetCurrentThread(&mPluginThreadID);

	// create a console, only if we can register windows.
	if (thePluginManager2 != NULL) {
		mConsole = new MRJConsole(this);
		mConsole->AddRef();
	}

	return NS_OK;
}

NS_METHOD MRJPlugin::GetMIMEDescription(const char* *result)
{
	*result = NPJVM_MIME_TYPE;
	return NS_OK;
}

MRJSession* MRJPlugin::getSession()
{
	StartupJVM(NULL);
	return mSession;
}

nsIJVMManager* MRJPlugin::getManager()
{
	return mManager;
}

nsIThreadManager* MRJPlugin::getThreadManager()
{
	return mThreadManager;
}

NS_METHOD MRJPlugin::StartupJVM(nsJVMInitArgs* initArgs)
{
	if (mSession == NULL) {
		// start a session with MRJ.
		mSession = new MRJSession();
		if (mSession->getStatus() != noErr) {
			// how can we signal an error?
			delete mSession;
			mSession = NULL;
			return NS_ERROR_FAILURE;
		}
		// Apply the initialization args.
		if (initArgs != NULL && initArgs->version >= nsJVMInitArgs_Version) {
			const char* classPathAdditions = initArgs->classpathAdditions;
			if (classPathAdditions != NULL) {
				// what format will this be in? UNIX paths, separated by ':' characters.
				char* paths = new char[1 + strlen(classPathAdditions)];
				if (paths != NULL) {
					strcpy(paths, classPathAdditions);
					char* path = strtok(paths, ":");
					while (path != NULL) {
						static char urlPrefix[] = { "file://" };
						char* fileURL = new char[sizeof(urlPrefix) + strlen(path)];
						if (fileURL != NULL) {
							strcat(strcpy(fileURL, urlPrefix), path);
							mSession->addURLToClassPath(fileURL);
							delete[] fileURL;
						}
						path = strtok(NULL, ":");
					}
					delete[] paths;
				}
			}
		}

		InitLiveConnectSupport(this);

#if 0
		// start our idle thread.
		if (mThreadManager != NULL) {
			PRUint32 threadID;
			mThreadManager->CreateThread(&threadID, this);
		}
#endif

		mIsEnabled = true;
	}
	return NS_OK;
}

NS_METHOD MRJPlugin::ShutdownJVM(PRBool fullShutdown)
{
	if (fullShutdown) {
		if (mSession != NULL) {
			delete mSession;
			mSession = NULL;
		}
	}
	return NS_OK;
}

NS_METHOD MRJPlugin::AddToClassPath(const char* dirPath)
{
	if (mSession != NULL) {
		mSession->addToClassPath(dirPath);
		return NS_OK;
	}
	return NS_JVM_ERROR_WRONG_CLASSES;
}

NS_METHOD MRJPlugin::GetClassPath(const char* *result)
{
	char* classPath = mSession->getProperty("java.class.path");
	*result = classPath;
	return (classPath != NULL ? NS_OK : NS_ERROR_FAILURE);
}

NS_METHOD MRJPlugin::GetJavaWrapper(JNIEnv* env, jint jsobj, jobject *jobj)
{
	// use jsobj as key into a table.
	// if not in the table, then create a new netscape.javascript.JSObject that references this.
	*jobj = Wrap_JSObject(env, jsobj);
	return NS_OK;
}

NS_METHOD MRJPlugin::GetJavaVM(JavaVM* *result)
{
	*result = NULL;
	if (StartupJVM(NULL) == NS_OK) {
		*result = mSession->getJavaVM();
		return NS_OK;
	}
	return NS_ERROR_FAILURE;
}

nsrefcnt MRJPlugin::GetJNIEnv(JNIEnv* *result)
{
	JNIEnv* env = NULL;
	if (StartupJVM(NULL) == NS_OK) {
#if 1
		env = mSession->getCurrentEnv();
#else
		JDK1_1AttachArgs args;
		JavaVM* vm = mSession->getJavaVM();
		jint result = vm->AttachCurrentThread(&env, &args);
		if (result != 0)
			env = NULL;
#endif
	}
	*result = env;
	return 1;
}

nsrefcnt MRJPlugin::ReleaseJNIEnv(JNIEnv* env)
{
	return 0;
}

NS_METHOD MRJPlugin::CreateSecureEnv(JNIEnv* proxyEnv, nsISecureJNI2* *outSecureEnv)
{
	// Need to spawn a new JVM communication thread here.
	NS_DEFINE_IID(kISecureJNI2IID, NS_ISECUREJNI2_IID);
	return CSecureJNI2::Create(NULL, this, proxyEnv, kISecureJNI2IID, (void**)outSecureEnv);
}

NS_METHOD MRJPlugin::SpendTime(PRUint32 timeMillis)
{
	nsresult result = NS_OK;
	// Only do this if there aren't any plugin instances.
	// if (true || MRJPluginInstance::getInstances() == NULL) {
	result = StartupJVM(NULL);
	if (result == NS_OK)
		mSession->idle(timeMillis);
	// }
	return result;
}

NS_METHOD MRJPlugin::Run()
{
	while (mSession != NULL) {
		mSession->idle();
		mThreadManager->Sleep();
	}
	return NS_OK;
}

MRJPluginInstance* MRJPlugin::getPluginInstance(jobject applet)
{
	JNIEnv* env = mSession->getCurrentEnv();
	MRJPluginInstance* instance = MRJPluginInstance::getInstances();
	while (instance != NULL) {
		jobject object = NULL;
		if (instance->GetJavaObject(&object) == NS_OK && env->IsSameObject(applet, object)) {
			instance->AddRef();
			return instance;
		}
		instance = instance->getNextInstance();
	}
	return NULL;
}

MRJPluginInstance* MRJPlugin::getPluginInstance(JNIEnv* jenv)
{
	// Apple will provide an API that maps a JNIEnv to an JMAWTContextRef. We can map this to the MRJContext/Applet/Instance.
	MRJPluginInstance* instance = MRJPluginInstance::getInstances();
	if (&::JMJNIToAWTContext != NULL) {
		JMAWTContextRef contextRef = ::JMJNIToAWTContext(mSession->getSessionRef(), jenv);
		if (contextRef != NULL) {
			while (instance != NULL) {
				if (instance->getContext()->getContextRef() == contextRef) {
					instance->AddRef();
					return instance;
				}
				instance = instance->getNextInstance();
			}
		}
	} else {
		if (instance != NULL) {
			instance->AddRef();
			return instance;
		}
	}
	return NULL;
}

Boolean MRJPlugin::inPluginThread()
{
	PRUint32 currentThreadID = -1;
	if (mThreadManager != NULL)
		mThreadManager->GetCurrentThread(&currentThreadID);
	return (mPluginThreadID == currentThreadID);
}

#pragma mark *** MRJPluginInstance ***

NS_METHOD MRJPluginInstance::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
	return queryInterface(aIID, aInstancePtr);
}

nsrefcnt MRJPluginInstance::AddRef() { return addRef(); }
nsrefcnt MRJPluginInstance::Release() { return release(); }

nsID MRJPluginInstance::sInterfaceIDs[] = { NS_IPLUGININSTANCE_IID, NS_IJVMPLUGININSTANCE_IID };

MRJPluginInstance::MRJPluginInstance(MRJPlugin* plugin)
	:	SupportsMixin(this, sInterfaceIDs, sizeof(sInterfaceIDs) / sizeof(nsID)),
		mPeer(NULL), mWindowlessPeer(NULL),
		mPlugin(plugin), mSession(plugin->getSession()),
		mContext(NULL), mApplet(NULL),
		mNext(NULL)
{
	// add this instance to the instance list.
	pushInstance();

	// Tell the plugin we are retaining a reference.
	mPlugin->AddRef();
}

MRJPluginInstance::~MRJPluginInstance()
{
	// Remove this instance from the global list.
	popInstance();

	if (mContext != NULL) {
		delete mContext;
		mContext = NULL;
	}

	if (mPlugin != NULL) {
		mPlugin->Release();
		mPlugin = NULL;
	}

	if (mWindowlessPeer != NULL) {
		mWindowlessPeer->Release();
		mWindowlessPeer = NULL;
	}

	if (mPeer != NULL) {
		mPeer->Release();
		mPeer = NULL;
	}

	if (mApplet != NULL) {
		JNIEnv* env = mSession->getCurrentEnv();
		env->DeleteGlobalRef(mApplet);
		mApplet = NULL;
	}
}

NS_METHOD MRJPluginInstance::Initialize(nsIPluginInstancePeer* peer)
{
	// Tell the peer we are retaining a reference.
	mPeer = peer;
	mPeer->AddRef();

	// See if we have a windowless peer.
	nsresult result = mPeer->QueryInterface(kIWindowlessPluginInstancePeerIID, &mWindowlessPeer);
	if (result != NS_OK) mWindowlessPeer = NULL;

	// create a context for the applet we will run.
	mContext = new MRJContext(mSession, this);

	mContext->processAppletTag();
	mContext->createContext();

	return NS_OK;
}

NS_METHOD MRJPluginInstance::GetPeer(nsIPluginInstancePeer* *result)
{
	mPeer->AddRef();
	*result = mPeer;
	return NS_OK;
}

NS_METHOD MRJPluginInstance::Start()
{
	// Take this moment to show the applet's frames (if any).
	mContext->showFrames();
	
	mContext->resumeApplet();
	
	return NS_OK;
}

NS_METHOD MRJPluginInstance::Stop()
{
	// Take this moment to hide the applet's frames.
	mContext->hideFrames();

	mContext->suspendApplet();

	return NS_OK;
}

NS_METHOD MRJPluginInstance::Destroy()
{
	// Release();
	return NS_OK;
}

/** FIXME:  Need an intelligent way to track changes to the NPPluginWindow. */

NS_METHOD MRJPluginInstance::SetWindow(nsPluginWindow* pluginWindow)
{
	mContext->setWindow(pluginWindow);

   	return NS_OK;
}

NS_METHOD MRJPluginInstance::HandleEvent(nsPluginEvent* pluginEvent, PRBool* eventHandled)
{
	*eventHandled = PR_TRUE;

	if (pluginEvent != NULL) {
		EventRecord* event = pluginEvent->event;

		// Check for clipping changes.
		if (event->what == nsPluginEventType_ClippingChangedEvent) {
			mContext->setClipping(RgnHandle(event->message));
			return NS_OK;
		}
		
		// Check for coordinate system changes.
		// visibilityChanged = mContext->inspectWindow();
		// assume the browser will generate the correct update events?
		if (mContext->inspectWindow() && mWindowlessPeer != NULL)
			mWindowlessPeer->ForceRedraw();
		
		if (event->what == nullEvent) {
			// Give MRJ another quantum of time.
			mSession->idle(kDefaultJMTime);	// now SpendTime does this.
		} else {
			MRJFrame* frame = mContext->findFrame(WindowRef(pluginEvent->window));
			if (frame != NULL) {
				switch (event->what) {
				case nsPluginEventType_GetFocusEvent:
					frame->focusEvent(true);
					break;
				
				case nsPluginEventType_LoseFocusEvent:
					frame->focusEvent(false);
					break;

				case nsPluginEventType_AdjustCursorEvent:
					frame->idle(event->modifiers);
					break;
				
				case nsPluginEventType_MenuCommandEvent:
					frame->menuSelected(event->message, event->modifiers);
					break;
					
				default:
					*eventHandled = frame->handleEvent(event);
					break;
				}
			}
		}
	}
	
	return NS_OK;
}

NS_METHOD MRJPluginInstance::GetJavaObject(jobject *result)
{
	if (mApplet == NULL) {
		jobject applet = mContext->getApplet();
		JNIEnv* env = mSession->getCurrentEnv();
		mApplet = env->NewGlobalRef(applet);
	}
	*result = mApplet;
	return NS_OK;
}

// Accessing the list of instances.

static MRJPluginInstance* theInstances = NULL;

void MRJPluginInstance::pushInstance()
{
	mNext = theInstances;
	theInstances = this;
}

void MRJPluginInstance::popInstance()
{
	MRJPluginInstance** link = &theInstances;
	MRJPluginInstance* instance  = *link;
	while (instance != NULL) {
		if (instance == this) {
			*link = mNext;
			mNext = NULL;
			break;
		}
		link = &instance->mNext;
		instance = *link;
	}
}

MRJPluginInstance* MRJPluginInstance::getInstances()
{
	return theInstances;
}

MRJPluginInstance* MRJPluginInstance::getNextInstance()
{
	return mNext;
}

MRJContext* MRJPluginInstance::getContext()
{
	return mContext;
}

MRJSession* MRJPluginInstance::getSession()
{
	return mSession;
}
