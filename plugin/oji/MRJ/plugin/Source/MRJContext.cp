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
	MRJContext.cp
	
	Manages Java content using the MacOS Runtime for Java.
	
	by Patrick C. Beard.
 */

#include <string.h>
#include <stdlib.h>
#include <Appearance.h>
#include <Gestalt.h>

#include "MRJSession.h"
#include "MRJContext.h"
#include "MRJPlugin.h"
#include "MRJPage.h"
#include "AsyncMessage.h"
#include "TopLevelFrame.h"
#include "EmbeddedFrame.h"
#include "LocalPort.h"
#include "StringUtils.h"

#include "nsIPluginManager2.h"
#include "nsIPluginInstancePeer.h"
#include "nsIPluginTagInfo2.h"
#include "nsIJVMPluginTagInfo.h"

// Commonly used interface IDs.

static NS_DEFINE_IID(kIPluginTagInfoIID, NS_IPLUGINTAGINFO_IID);
static NS_DEFINE_IID(kIPluginTagInfo2IID, NS_IPLUGINTAGINFO2_IID);

extern nsIPluginManager* thePluginManager;
extern nsIPluginManager2* thePluginManager2;

static OSStatus JMTextToStr255(JMTextRef textRef, Str255 str);
static void blinkRgn(RgnHandle rgn);

void LocalPort::Enter()
{
	::GetPort(&fOldPort);
	if (fPort != fOldPort)
		::SetPort(fPort);
	fOldOrigin = topLeft(fPort->portRect);
	if (fOldOrigin.h != fOrigin.h || fOldOrigin.v != fOrigin.v)
		::SetOrigin(fOrigin.h, fOrigin.v);
}

void LocalPort::Exit()
{
	if (fOldOrigin.h != fOrigin.h || fOldOrigin.v != fOrigin.v)
		::SetOrigin(fOldOrigin.h, fOldOrigin.v);
	if (fOldPort != fPort)
		::SetPort(fOldPort);
}

MRJContext::MRJContext(MRJSession* session, MRJPluginInstance* instance)
	:	mPluginInstance(instance), mSession(session), mSessionRef(session->getSessionRef()), mPeer(NULL),
		mLocator(NULL), mContext(NULL), mViewer(NULL), mViewerFrame(NULL), mIsActive(false),
		mPluginWindow(NULL), mPluginClipping(NULL),	mPluginPort(NULL), mCodeBase(NULL), mDocumentBase(NULL), mPage(NULL)
{
	instance->GetPeer(&mPeer);
	
	// we cache attributes of the window, and periodically notice when they change.
    mCache.window = NULL; 	/* Platform specific window handle */
    mCache.x = -1;			/* Position of top left corner relative */
    mCache.y = -1;			/*	to a netscape page.					*/
    mCache.width = 0;		/* Maximum window size */
    mCache.height = 0;
    ::SetRect((Rect*)&mCache.clipRect, 0, 0, 0, 0);
    						/* Clipping rectangle in port coordinates */
							/* Used by MAC only.			  */
    mCache.type = nsPluginWindowType_Drawable;
    						/* Is this a window or a drawable? */

	mPluginClipping = ::NewRgn();
	mPluginPort = getEmptyPort();
}

MRJContext::~MRJContext()
{
	if (mLocator != NULL) {
		JMDisposeAppletLocator(mLocator);
		mLocator = NULL;
	}

	if (mViewer != NULL) {
		::JMDisposeAppletViewer(mViewer);
		mViewer = NULL;
	}

#if 0
	// hack:  see if this allows the applet viewer to terminate gracefully.
	for (int i = 0; i < 100; i++)
		::JMIdle(mSessionRef, kDefaultJMTime);
#endif

	if (mContext != NULL) {
		// hack:  release any frames that we still see in the AWT context, before tossing it.
		releaseFrames();

		::JMDisposeAWTContext(mContext);
		mContext = NULL;
	}
	
	if (mPeer != NULL) {
		mPeer->Release();
		mPeer = NULL;
	}
	
	if (mPage != NULL) {
		mPage->Release();
		mPage = NULL;
	}

	if (mPluginClipping != NULL) {
		::DisposeRgn(mPluginClipping);
		mPluginClipping = NULL;
	}
	
	if (mCodeBase != NULL) {
		delete[] mCodeBase;
		mCodeBase = NULL;
	}
	
	if (mDocumentBase != NULL) {
		delete[] mDocumentBase;
		mDocumentBase = NULL;
	}
}

static char* slashify(char* url)
{
	int len = ::strlen(url);
	if (url[len - 1] != '/') {
		char* newurl = new char[len + 2];
		::strcpy(newurl, url);
		newurl[len] = '/';
		newurl[len + 1] = '\0';
		delete[] url;
		url = newurl;
	}
	return url;
}

void MRJContext::processAppletTag()
{
	// create an applet locator.
	OSStatus status = noErr;
	JMLocatorInfoBlock info;
	info.fVersion = kJMVersion;
	info.fBaseURL = NULL;
	info.fAppletCode = NULL;
	info.fWidth = info.fHeight = 100;
	info.fOptionalParameterCount = 0;
	info.fParams = NULL;
	
	struct {
		PRUint16 fCount;
		const char* const* fNames;
		const char* const* fValues;
	} attributes = { 0, NULL, NULL }, parameters = { 0, NULL, NULL };

	MRJPageAttributes pageAttributes = { 0, NULL, NULL, false };

	// obtain the applet's attributes & parameters.
	nsIPluginTagInfo* tagInfo = NULL;
	if (mPeer->QueryInterface(kIPluginTagInfoIID, &tagInfo) == NS_OK) {
		tagInfo->GetAttributes(attributes.fCount, attributes.fNames, attributes.fValues);
		nsIPluginTagInfo2* tagInfo2 = NULL;
		if (tagInfo->QueryInterface(kIPluginTagInfo2IID, &tagInfo2) == NS_OK) {
			tagInfo2->GetParameters(parameters.fCount, parameters.fNames, parameters.fValues);

			// get the URL of the HTML document	containing the applet.
			const char* documentBase = NULL;
			if (tagInfo2->GetDocumentBase(&documentBase) == NS_OK && documentBase != NULL) {
				// record the document base, in case 
				setDocumentBase(documentBase);

				// establish a page context for this applet to run in.
				nsIJVMPluginTagInfo* jvmTagInfo = NULL;
				if (mPeer->QueryInterface(nsIJVMPluginTagInfo::GetIID(), &jvmTagInfo) == NS_OK) {
					PRUint32 documentID;
					const char* codeBase;
					const char* archive;
					PRBool mayScript;
					if (tagInfo2->GetUniqueID(&documentID) == NS_OK &&
						jvmTagInfo->GetCodeBase(&codeBase) == NS_OK &&
						jvmTagInfo->GetArchive(&archive) == NS_OK &&
						jvmTagInfo->GetMayScript(&mayScript) == NS_OK) {
						pageAttributes.documentID = documentID;
						pageAttributes.codeBase = codeBase;
						pageAttributes.archive = archive;
						pageAttributes.mayScript = mayScript;
					}
					jvmTagInfo->Release();
				}
			}
			tagInfo2->Release();
		}
		tagInfo->Release();
	}
	
	// compute the URL of the directory containing this applet's HTML page.
	char* baseURL = ::strdup(getDocumentBase());
	if (baseURL != NULL) {
		// trim the trailing document name.
		char* lastSlash = ::strrchr(baseURL, '/');
		if (lastSlash != NULL)
			lastSlash[1] = '\0';
	}
	
	// assume that all arguments might be optional.
	int optionalArgIndex = 0;
	info.fParams = new JMLIBOptionalParams[attributes.fCount + parameters.fCount];
	if (info.fParams == NULL)
		goto done;

	// process APPLET/EMBED tag attributes.
	for (int i = 0; i < attributes.fCount; i++) {
		const char* name = attributes.fNames[i];
		const char* value = attributes.fValues[i];
		if (name != NULL && value != NULL) {
			if (strcasecmp(name, "CODEBASE") == 0) {
				if (pageAttributes.codeBase == NULL)
					pageAttributes.codeBase = value;
			} else
			if (strcasecmp(name, "CODE") == 0) {
				status = ::JMNewTextRef(mSessionRef, &info.fAppletCode, kTextEncodingMacRoman, value, strlen(value));
			} else 
			if (strcasecmp(name, "WIDTH") == 0) {
				info.fWidth = ::atoi(value);
			} else 
			if (strcasecmp(name, "HEIGHT") == 0) {
				info.fHeight = ::atoi(value);
			} else {
				// assume it's an optional argument.
				JMLIBOptionalParams* param = &info.fParams[optionalArgIndex++];
				status = ::JMNewTextRef(mSessionRef, &param->fParamName, kTextEncodingMacRoman, name, strlen(name));
				status = ::JMNewTextRef(mSessionRef, &param->fParamValue, kTextEncodingMacRoman, value, strlen(value));
			}
		}
	}
	
	// set up the clipping region based on width & height.
	::SetRectRgn(mPluginClipping, 0, 0, info.fWidth, info.fHeight);
	
	// process parameters.
	for (int i = 0; i < parameters.fCount; i++) {
		const char* name = parameters.fNames[i];
		const char* value = parameters.fValues[i];
		if (name != NULL && value != NULL) {
			// assume it's an optional argument.
			JMLIBOptionalParams* param = &info.fParams[optionalArgIndex++];
			status = ::JMNewTextRef(mSessionRef, &param->fParamName, kTextEncodingMacRoman, name, strlen(name));
			status = ::JMNewTextRef(mSessionRef, &param->fParamValue, kTextEncodingMacRoman, value, strlen(value));
		}
	}
	info.fOptionalParameterCount = optionalArgIndex;
	
	// Compute the locator's baseURL field by concatenating the CODEBASE (if any specified).
	if (baseURL != NULL) {
		if (pageAttributes.codeBase != NULL) {
			// if the codebase is an absolute URL, use it, otherwise, concatenate with document's URL.
			if (::strchr(pageAttributes.codeBase, ':') != NULL) {
				delete[] baseURL;
				baseURL = ::strdup(pageAttributes.codeBase);
			} else {
				if (pageAttributes.codeBase[0] == '/') {
					// if codeBase starts with '/', need to prepend the URL's protocol://hostname part.
					char* fullURL = new char[::strlen(baseURL) + ::strlen(pageAttributes.codeBase) + 1];
					// search for start of host name. what about file URL's?
					const char* hostNameStart = strchr(baseURL, '/');
					while (*hostNameStart == '/') ++hostNameStart;
					const char* hostNameEnd = strchr(hostNameStart, '/') + 1;
					::strncpy(fullURL, baseURL, hostNameEnd - baseURL);
					fullURL[hostNameEnd - baseURL] = '\0';
					::strcat(fullURL, pageAttributes.codeBase + 1);
					delete[] baseURL;
					baseURL = fullURL;
				} else {
					// assume the codeBase is relative to the document's URL.
					int baseLength = ::strlen(baseURL);
					int fullLength = baseLength + ::strlen(pageAttributes.codeBase);
					char* fullURL = new char[fullLength + 1];
					::strcpy(fullURL, baseURL);
					::strcat(fullURL + baseLength, pageAttributes.codeBase);
					delete[] baseURL;
					baseURL = fullURL;
				}
			}
		}
	} else if (pageAttributes.codeBase != NULL) {
		// the CODEBASE must be an absolute URL in this case.
		baseURL = ::strdup(pageAttributes.codeBase);
	}

	if (baseURL != NULL) {
		// make sure the URL always ends with a slash.
		baseURL = ::slashify(baseURL);
		status = ::JMNewTextRef(mSessionRef, &info.fBaseURL, kTextEncodingMacRoman, baseURL, ::strlen(baseURL));

		// baseURL is now the *TRUE* codeBase
		pageAttributes.codeBase = baseURL;
		mPage = findPage(pageAttributes);
		
		// keep the codeBase around for later.
		setCodeBase(baseURL);
		delete[] baseURL;

		status = ::JMNewAppletLocatorFromInfo(&mLocator, mSessionRef, &info, NULL);
	} else {
		::DebugStr("\pNeed a baseURL (CODEBASE)!");
	}

done:
	if (info.fBaseURL != NULL)
		::JMDisposeTextRef(info.fBaseURL);
	if (info.fAppletCode != NULL)
		::JMDisposeTextRef(info.fAppletCode);
	if (info.fParams != NULL) {
		for (int i = 0; i < optionalArgIndex; i++) {
			JMLIBOptionalParams* param = &info.fParams[i];
			::JMDisposeTextRef(param->fParamName);
			::JMDisposeTextRef(param->fParamValue);
		}
		delete info.fParams;
	}
}

static MRJFrame* getFrame(JMFrameRef ref)
{
	MRJFrame* frame = NULL;
	if (ref != NULL)
		::JMGetFrameData(ref, (JMClientData*)&frame);
	return frame;
}

static void frameSetSize(JMFrameRef ref, const Rect* newSize)
{
	MRJFrame* frame = getFrame(ref);
	if (frame != NULL)
		frame->setSize(newSize);
}

static void frameInvalRect(JMFrameRef ref, const Rect* invalidRect)
{
	MRJFrame* frame = getFrame(ref);
	if (frame != NULL)
		frame->invalRect(invalidRect);

#if 0
	// do we know for certain that this frame's port is a window? Assume it is.
	MRJContext* thisContext = NULL;
	OSStatus status = ::JMGetFrameData(frame, (JMClientData*)&thisContext);
	if (status == noErr && thisContext != NULL)
		::InvalRect(r);
#endif

#if 0
	// since this comes in on an arbitrary thread, we can't very well go calling
	// back into the browser to do this, it crashes.
	MRJContext* thisContext;
	OSStatus status = ::JMGetFrameData(frame, (JMClientData*)&thisContext);
	if (status == noErr) {
		// since this comes in on an arbitrary thread, there's no telling what
		// state the world is in.
		NPRect invalidRect = { r->top, r->left, r->bottom, r->right };
		::NPN_InvalidateRect(thisContext->fInstance, &invalidRect);
	}
#endif
}

static void frameShowHide(JMFrameRef ref, Boolean visible)
{
	MRJFrame* frame = getFrame(ref);
	if (frame != NULL)
		frame->showHide(visible);
}

static void frameSetTitle(JMFrameRef ref, JMTextRef titleRef)
{
	Str255 title;
	JMTextToStr255(titleRef, title);
	MRJFrame* frame = getFrame(ref);
	if (frame != NULL)
		frame->setTitle(title);
}

static void frameCheckUpdate(JMFrameRef ref)
{
	MRJFrame* frame = getFrame(ref);
	if (frame != NULL)
		frame->checkUpdate();
}

static void frameReorderFrame(JMFrameRef ref, ReorderRequest request)
{
	MRJFrame* frame = getFrame(ref);
	if (frame != NULL)
		frame->reorder(request);
}

static void frameSetResizeable(JMFrameRef ref, Boolean resizeable) 
{
	MRJFrame* frame = getFrame(ref);
	if (frame != NULL)
		frame->setResizeable(resizeable);
}

static void frameGetFrameInsets(JMFrameRef frame, Rect *insets)
{
	// MRJFrame* frame = getFrame(ref);
	// if (frame != NULL)
	//	frame->getFrameInsets(insets);
	insets->top = insets->left = insets->bottom = insets->right = 0;
}

static void frameNextFocus(JMFrameRef frame, Boolean forward)
{
	// MRJFrame* frame = getFrame(ref);
	// if (frame != NULL)
	//	frame->nextFocus(insets);
}

static void frameRequestFocus(JMFrameRef frame)
{
	// MRJFrame* frame = getFrame(ref);
	// if (frame != NULL)
	//	frame->requestFocus(insets);
}

class AppletViewerFrame : public MRJFrame {
public:
	AppletViewerFrame(JMFrameRef frameRef, MRJContext* context) : MRJFrame(frameRef), mContext(context) {}
	
	virtual void invalRect(const Rect* invalidRect);
	
	virtual void idle(SInt16 modifiers);
	virtual void update();
	virtual void click(const EventRecord* event);

protected:
	virtual GrafPtr getPort();

private:
	MRJContext* mContext;
};

void AppletViewerFrame::invalRect(const Rect* invalidRect)
{
	::InvalRect(invalidRect);
}

void AppletViewerFrame::idle(SInt16 modifiers)
{
	mContext->idle(modifiers);
}

void AppletViewerFrame::update()
{
	mContext->drawApplet();
}

void AppletViewerFrame::click(const EventRecord* event)
{
	mContext->click(event, this);
}

GrafPtr AppletViewerFrame::getPort()
{
	return mContext->getPort();	
}

JMFrameCallbacks theFrameCallbacks = {
	kJMVersion,						/* should be set to kJMVersion */
	&frameSetSize,
	&frameInvalRect,
	&frameShowHide,
	&frameSetTitle,
	&frameCheckUpdate,
	&frameReorderFrame,
	&frameSetResizeable,
	&frameGetFrameInsets,
	&frameNextFocus,
	&frameRequestFocus,
};

OSStatus MRJContext::requestFrame(JMAWTContextRef contextRef, JMFrameRef frameRef, JMFrameKind kind,
									const Rect* initialBounds, Boolean resizeable, JMFrameCallbacks* cb)
{
	// set up the viewer frame's callbacks.
	BlockMoveData(&theFrameCallbacks, cb, sizeof(theFrameCallbacks));
	// *cb = callbacks;

	MRJContext* thisContext = NULL;
	OSStatus status = ::JMGetAWTContextData(contextRef, (JMClientData*)&thisContext);
	return thisContext->createFrame(frameRef, kind, initialBounds, resizeable);
}

OSStatus MRJContext::releaseFrame(JMAWTContextRef contextRef, JMFrameRef frameRef)
{
	MRJContext* thisContext = NULL;
	OSStatus status = ::JMGetAWTContextData(contextRef, (JMClientData*)&thisContext);
	MRJFrame* thisFrame = NULL;
	status = ::JMGetFrameData(frameRef, (JMClientData*)&thisFrame);
	if (thisFrame != NULL) {
		status = ::JMSetFrameData(frameRef, NULL);
		if (thisContext->mViewerFrame == frameRef) {
			thisContext->mViewerFrame = NULL;
		}
		delete thisFrame;
	}
	return status;
}

SInt16 MRJContext::getUniqueMenuID(JMAWTContextRef contextRef, Boolean isSubmenu)
{
	MRJContext* thisContext = NULL;
	OSStatus status = ::JMGetAWTContextData(contextRef, (JMClientData*)&thisContext);
	return thisContext->allocateMenuID(isSubmenu);
}

static Boolean appearanceManagerExists()
{
	long response = 0;
	return (Gestalt(gestaltAppearanceAttr, &response) == noErr && (response & (1 << gestaltAppearanceExists)));
}

static OSStatus JMTextToStr255(JMTextRef textRef, Str255 str)
{
	UInt32 length = 0;
	OSStatus status = JMGetTextBytes(textRef, kTextEncodingMacRoman, &str[1], sizeof(Str255) - 1, &length);
	if (status == noErr)
		str[0] = (unsigned char)(status == noErr ? length : 0);
	return status;
}

void MRJContext::exceptionOccurred(JMAWTContextRef context, JMTextRef exceptionName, JMTextRef exceptionMsg, JMTextRef stackTrace)
{
	// why not display this using the Appearance Manager's wizzy new alert?
	if (appearanceManagerExists()) {
		OSStatus status;
		Str255 error, explanation;
		status = ::JMTextToStr255(exceptionName, error);
		status = ::JMTextToStr255(exceptionMsg, explanation);
		
		SInt16 itemHit = 0;
		OSErr result = ::StandardAlert(kAlertPlainAlert, error, explanation, NULL, &itemHit);
	}
}

Boolean MRJContext::createContext()
{
	JMAWTContextCallbacks callbacks = {
		kJMVersion,						/* should be set to kJMVersion */
		&requestFrame, 					/* a new frame is being created. */
		&releaseFrame,					/* an existing frame is being destroyed. */
		&getUniqueMenuID, 				/* a new menu will be created with this id. */
		&exceptionOccurred,				/* just some notification that some recent operation caused an exception.  You can't do anything really from here. */
	};
	if (mPage != NULL)
		return mPage->createContext(&mContext, &callbacks, this);
	else
		return (::JMNewAWTContext(&mContext, mSessionRef, &callbacks, this) == noErr);
}

JMAWTContextRef MRJContext::getContextRef()
{
	return mContext;
}

void MRJContext::showDocument(JMAppletViewerRef viewer, JMTextRef urlString, JMTextRef windowName)
{
	MRJContext* thisContext;
	OSStatus status = ::JMGetAppletViewerData(viewer, (JMClientData*)&thisContext);
	if (status == noErr) {
		Handle urlHandle = ::JMTextToMacOSCStringHandle(urlString);
		Handle windowHandle = ::JMTextToMacOSCStringHandle(windowName);
		if (urlHandle != NULL && windowHandle != NULL) {
			::HLock(urlHandle); ::HLock(windowHandle);
			const char* url = *urlHandle;
			const char* target = *windowHandle;
			thisContext->showURL(url, target);
		}
		if (urlHandle != NULL)
			::DisposeHandle(urlHandle);
		if (windowHandle != NULL)
			::DisposeHandle(windowHandle);
	}
}

void MRJContext::setStatusMessage(JMAppletViewerRef viewer, JMTextRef statusMsg)
{
	MRJContext* thisContext;
	OSStatus status = ::JMGetAppletViewerData(viewer, (JMClientData*)&thisContext);
	if (status == noErr) {
		Handle messageHandle = ::JMTextToMacOSCStringHandle(statusMsg);
		if (messageHandle != NULL) {
			::HLock(messageHandle);
			const char* message = *messageHandle;
			thisContext->showStatus(message);
			::DisposeHandle(messageHandle);
		}
	}
}

/*
	FIXME: this code should really be called from a true browser thread, so should put message in a queue, and let
	idle time processing handle it. Otherwise, the browser will get very confused.
 */

static nsresult getURL(nsISupports* peer, const char* url, const char* target)
{
	nsresult result = NS_OK;
	if (thePluginManager != NULL) {
		result = thePluginManager->GetURL(peer, url, target);
	}
	return result;
}

void AsyncMessage::send(Boolean async)
{
	// submit the message, and wait for the message to be executed asynchronously.
	mSession->sendMessage(this, async);
}

class GetURLMessage : public AsyncMessage {
	MRJPluginInstance* mPluginInstance;
	char* mURL;
	char* mTarget;
public:
	GetURLMessage(MRJPluginInstance* pluginInstance, const char* url, const char* target);
	~GetURLMessage();

	virtual void execute();
};

GetURLMessage::GetURLMessage(MRJPluginInstance* pluginInstance, const char* url, const char* target)
	:	AsyncMessage(pluginInstance->getSession()),
		mPluginInstance(pluginInstance), mURL(::strdup(url)), mTarget(::strdup(target))
{
	NS_ADDREF(mPluginInstance);
}

GetURLMessage::~GetURLMessage()
{
	if (mURL != NULL)
		delete[] mURL;
	if (mTarget != NULL)
		delete[] mTarget;

	NS_RELEASE(mPluginInstance);
}

void GetURLMessage::execute()
{
	// get the URL.
	nsIPluginInstance* pluginInstance = mPluginInstance;
	nsresult result = thePluginManager->GetURL(pluginInstance, mURL, mTarget);
	delete this;
}

void MRJContext::showURL(const char* url, const char* target)
{
	if (thePluginManager != NULL) {
#if 0
		GetURLMessage* message = new GetURLMessage(mPluginInstance, url, target);
		message->send(true);
#else
		nsIPluginInstance* pluginInstance = mPluginInstance;
		thePluginManager->GetURL(pluginInstance, url, target);
#endif
	}
}

void MRJContext::showStatus(const char* message)
{
	ensureValidPort();
	
	if (mPeer != NULL)
		mPeer->ShowStatus(message);
}

static SInt16 nextMenuId = 20000;
static SInt16 nextMenuPopupId = 200;

SInt16 MRJContext::allocateMenuID(Boolean isSubmenu)
{
	// FIXME:  can use more centralized approach to managing menu IDs which can be shared across all contexts.
	if (thePluginManager2 != NULL) {
		PRInt16 menuID = -1;
		nsIEventHandler* eventHandler = mPluginInstance;
		thePluginManager2->AllocateMenuID(eventHandler, isSubmenu, &menuID);
		return menuID;
	} else
		return (isSubmenu ? nextMenuPopupId++ : nextMenuId++);
}

OSStatus MRJContext::createFrame(JMFrameRef frameRef, JMFrameKind kind, const Rect* initialBounds, Boolean resizeable)
{
	OSStatus status = memFullErr;
	MRJFrame* frame = NULL;
	
	// The first frame created will always be for the applet viewer.
	if (mViewerFrame == NULL) {
		// bind this newly created frame to this context, and vice versa.
		mViewerFrame = frameRef;
		frame = new AppletViewerFrame(frameRef, this);

		// make sure the frame's clipping is up-to-date.
		setVisibility();

#if 0		
		// ensure the frame is active.
		WindowRef appletWindow = getPort();
		Boolean isHilited = IsWindowHilited(appletWindow);
		frame->activate(isHilited);
#endif
	} else if (thePluginManager2 != NULL) {
		// Can only do this safely if we are using the new API.
		frame = new TopLevelFrame(mPluginInstance, frameRef, kind, initialBounds, resizeable);
	} else {
		// Try to create a frame that lives in a newly created browser window.
		frame = new EmbeddedFrame(mPluginInstance, frameRef, kind, initialBounds, resizeable);
	}

	if (frame != NULL)
		status = ::JMSetFrameData(frameRef, frame);
	
	return status;
}

Boolean MRJContext::appletLoaded()
{
	return (mViewer != NULL);
}

Boolean MRJContext::loadApplet()
{
	static JMAppletSecurity security = {
		kJMVersion,						/* should be set to kJMVersion */
		eAppletHostAccess,				/* can this applet access network resources */
		eLocalAppletAccess,				/* can this applet access network resources */
		true,							/* restrict access to system packages (com.apple.*, sun.*, netscape.*) also found in the property "mrj.security.system.access" */
		true,							/* restrict classes from loading system packages (com.apple.*, sun.*, netscape.*) also found in the property "mrj.security.system.define" */
		true,							/* restrict access to application packages found in the property "mrj.security.application.access" */
		true,							/* restrict access to application packages found in the property "mrj.security.application.access" */
	};
	static JMAppletViewerCallbacks callbacks = {
		kJMVersion,						/* should be set to kJMVersion */
		&showDocument,					/* go to a url, optionally in a new window */
		&setStatusMessage,				/* applet changed status message */
	};
	OSStatus status = ::JMNewAppletViewer(&mViewer, mContext, mLocator, 0,
								 			&security, &callbacks, this);
	if (status == noErr) {
		status = ::JMSetAppletViewerData(mViewer, JMClientData(this));
		status = ::JMReloadApplet(mViewer);
	}
	
	if (status == noErr) {
		// for grins, force the applet to load right away, so we can report any error we might encounter eagerly.
		// jobject appletObject = getApplet();
	}
	
	return (status == noErr);
}

void MRJContext::suspendApplet()
{
	if (mViewer != NULL)
		::JMSuspendApplet(mViewer);
}

void MRJContext::resumeApplet()
{
	if (mViewer != NULL)
		::JMResumeApplet(mViewer);
}

jobject MRJContext::getApplet()
{
	if (appletLoaded() && &::JMGetAppletJNIObject != NULL) {
		JNIEnv* env = ::JMGetCurrentEnv(mSessionRef);
		jobject appletObject = ::JMGetAppletJNIObject(mViewer, env);
		if (appletObject == NULL) {
			// Give MRJ some time to try to comply. 100 calls to JMIdle should be enough.
			// 10 seconds should be enough. Could ask the user...
			UInt32 deadline = ::TickCount() + 600;
			while (appletObject == NULL && ::TickCount() < deadline) {
				mSession->idle(kDefaultJMTime);
				appletObject = ::JMGetAppletJNIObject(mViewer, env);
			}
			if (appletObject == NULL) {
				// assume the applet's hosed.
				::JMDisposeAppletViewer(mViewer);
				mViewer = NULL;
				// Let the user know the applet failed to load.
				if (appearanceManagerExists()) {
					SInt16 itemHit = 0;
					Str255 appletURL = { "\pUnknown APPLET URL" };
					if (mCodeBase != NULL) {
						appletURL[0] = ::strlen(mCodeBase);
						::BlockMoveData(mCodeBase, appletURL + 1, appletURL[0]);
					}
					::StandardAlert(kAlertPlainAlert, "\pApplet failed to load from URL:", appletURL, NULL, &itemHit);
				}
			}
		}
		return appletObject;
	}
	return NULL;
}

/**
 * At various times during development, it is useful to see what bits are supposed to be drawn by
 * the applet. Defining DEBUG_CLIPPING as 1 turns on this behavior.
 */
#define DEBUG_CLIPPING 0

void MRJContext::drawApplet()
{
	// We assume the proper coordinate system for the frame has
	// already been set up.
	if (appletLoaded()) {
#if DEBUG_CLIPPING
		nsPluginPort* npPort = (nsPluginPort*) mCache.window;
		GrafPtr framePort = GrafPtr(npPort->port);
		RgnHandle oldClip = NewRgn();
		if (oldClip != NULL) {
			CopyRgn(framePort->clipRgn, oldClip);
			SetClip(mPluginClipping);
			InvertRgn(mPluginClipping);
			SetClip(oldClip);
			DisposeRgn(oldClip);
		}
#endif

		// ::JMFrameUpdate(mViewerFrame, framePort->visRgn);
		// OSStatus status = ::JMFrameUpdate(mViewerFrame, framePort->clipRgn);
		OSStatus status = ::JMFrameUpdate(mViewerFrame, mPluginClipping);
	}
}

void MRJContext::activate(Boolean active)
{
	if (mViewerFrame != NULL) {
		::JMFrameActivate(mViewerFrame, active);
		mIsActive = active;
	} else {
		mIsActive = false;
	}
}

void MRJContext::resume(Boolean inFront)
{
	if (mViewerFrame != NULL) {
		::JMFrameResume(mViewerFrame, inFront);
	}
}

void MRJContext::click(const EventRecord* event, MRJFrame* appletFrame)
{
	// inspectWindow();

	nsPluginPort* npPort = (nsPluginPort*) mCache.window;
	
	// make the plugin's port current, and move its origin to (0, 0).
	LocalPort port(GrafPtr(npPort->port));
	port.Enter();

	// will we always be called in the right coordinate system?
	Point localWhere = event->where;
	::GlobalToLocal(&localWhere);
	nsPluginRect& clipRect = mCache.clipRect;
	Rect bounds = { clipRect.top, clipRect.left, clipRect.bottom, clipRect.right };
	if (PtInRect(localWhere, &bounds)) {
		localToFrame(&localWhere);
		appletFrame->click(event, localWhere);
	}
	
	// restore the plugin port's origin, and restore the current port.
	port.Exit();
}

void MRJContext::keyPress(long message, short modifiers)
{
	if (mViewerFrame != NULL) {
		::JMFrameKey(mViewerFrame, message & charCodeMask,
					(message & keyCodeMask) >> 8, modifiers);
	}
}

void MRJContext::keyRelease(long message, short modifiers)
{
	if (mViewerFrame != NULL) {
		::JMFrameKeyRelease(mViewerFrame, message & charCodeMask,
					(message & keyCodeMask) >> 8, modifiers);
	}
}

void MRJContext::idle(short modifiers)
{
	// inspectWindow();

	// Put the port in to proper window coordinates.
	nsPluginPort* npPort = (nsPluginPort*) mCache.window;
	LocalPort port(GrafPtr(npPort->port));
	port.Enter();
	
	Point pt;
	::GetMouse(&pt);
	localToFrame(&pt);
	::JMFrameMouseOver(mViewerFrame, pt, modifiers);
	
	port.Exit();
}

// interim routine for compatibility between OLD plugin interface and new.

// FIXME:  when the window goes away, npWindow/pluginWindow is passed in as NULL.
// should tell the AWTContext that the window has gone away. could use an offscreen
// or empty clipped grafport.

void MRJContext::setWindow(nsPluginWindow* pluginWindow)
{
	// don't do anything if the AWTContext hasn't been created yet.
	if (mContext != NULL && pluginWindow != NULL) {
		if (pluginWindow->height != 0 && pluginWindow->width != 0) {
			mPluginWindow = pluginWindow;
			mCache.window = NULL;

			// establish the GrafPort the plugin will draw in.
			mPluginPort = pluginWindow->window->port;

			// set up the clipping region based on width & height.
			::SetRectRgn(mPluginClipping, 0, 0, pluginWindow->width, pluginWindow->height);

			if (! appletLoaded())
				loadApplet();

			setVisibility();
		}
	} else {
		// tell MRJ the window has gone away.
		mPluginWindow = NULL;
		
		// use a single, 0x0, empty port for all future drawing.
		mPluginPort = getEmptyPort();
		
		// perhaps we should set the port to something quite innocuous, no? say a 0x0, empty port?
		::SetEmptyRgn(mPluginClipping);
		setVisibility();
	}
}

static Boolean equalRect(const nsPluginRect* r1, const nsPluginRect* r2)
{
	SInt32* r1p = (SInt32*)r1;
	SInt32* r2p = (SInt32*)r2;
	return (r1p[0] == r2p[0] && r1p[1] == r2p[1]);
}

Boolean MRJContext::inspectWindow()
{
	// don't bother looking, if the applet viewer frame doesn't exist yet.
	if (mViewerFrame == NULL)
		return false;

	Boolean recomputeVisibility = false;
	
	if (mPluginWindow != NULL) {
		// Use new plugin data structures.
		if (mCache.window == NULL || mCache.x != mPluginWindow->x || mCache.y != mPluginWindow->y || !equalRect(&mCache.clipRect, &mPluginWindow->clipRect)) {
			// transfer over values to the window cache.
			recomputeVisibility = true;
		}
	}
	
	if (recomputeVisibility)
		setVisibility();
	
	return recomputeVisibility;
}

Boolean MRJContext::inspectClipping()
{
	// this is called on update events to make sure the clipping region is in sync with the browser's.
	GrafPtr pluginPort = getPort();
	if (pluginPort != NULL) {
		if (true || !::EqualRgn(pluginPort->clipRgn, mPluginClipping)) {
			::CopyRgn(pluginPort->clipRgn, mPluginClipping);
			setVisibility();
			return true;
		}
	}
	return false;
}

void MRJContext::setClipping(RgnHandle clipRgn)
{
	::CopyRgn(clipRgn, mPluginClipping);
	setVisibility();
}

MRJFrame* MRJContext::findFrame(WindowRef window)
{
	MRJFrame* frame = NULL;
	
	// locates the frame corresponding to this window.
	if (window == NULL || (CGrafPtr(window) == mPluginPort) && mViewerFrame != NULL) {
		frame = getFrame(mViewerFrame);
	} else {
		// Scan the available frames for this context, and see if any of them correspond to this window.
		UInt32 frameCount;
		OSStatus status = ::JMCountAWTContextFrames(mContext, &frameCount);
		if (status == noErr) {
			for (UInt32 frameIndex = 1; frameIndex < frameCount; frameIndex++) {
				JMFrameRef frameRef;
				status = ::JMGetAWTContextFrame(mContext, frameIndex, &frameRef);
				frame = getFrame(frameRef);
				TopLevelFrame* tlFrame = dynamic_cast<TopLevelFrame*>(frame);
				if (tlFrame != NULL && tlFrame->getWindow() == window)
					break;
			}
		}
	}
	
	return frame;
}

GrafPtr MRJContext::getPort()
{
#if 0
	if (mPluginWindow != NULL) {
		nsPluginPort* npPort = (nsPluginPort*) mPluginWindow->window;
		return GrafPtr(npPort->port);
	}
	return NULL;
#endif

	return GrafPtr(mPluginPort);
}

void MRJContext::localToFrame(Point* pt)
{
	if (mPluginWindow != NULL) {
		// transform mouse to frame coordinates.
		nsPluginPort* npPort = (nsPluginPort*) mPluginWindow->window;
		pt->v += npPort->porty;
		pt->h += npPort->portx;
	}
}

void MRJContext::ensureValidPort()
{
	if (mPluginWindow != NULL) {
		nsPluginPort* npPort = (nsPluginPort*) mPluginWindow->window;
		if (npPort == NULL)
			mPluginPort = getEmptyPort();
		::SetPort(GrafPtr(mPluginPort));
	}
}

static void blinkRgn(RgnHandle rgn)
{
	::InvertRgn(rgn);
	UInt32 ticks = ::TickCount();
	while (::TickCount() - ticks < 10) ;
	::InvertRgn(rgn);
}

void MRJContext::setVisibility()
{
	// always update the cached information.
	if (mPluginWindow != NULL)
		mCache = *mPluginWindow;
	
	if (mViewerFrame != NULL) {
		nsPluginWindow* npWindow = &mCache;
		
		// compute the frame's origin and clipping.
		
		// JManager wants the origin expressed in window coordinates.
#if 0		
		Point frameOrigin = { npWindow->y, npWindow->x };
#else
		nsPluginPort* npPort = mPluginWindow->window;
		Point frameOrigin = { -npPort->porty, -npPort->portx };
#endif
	
		// The clipping region is now maintained by a new browser event.
		OSStatus status = ::JMSetFrameVisibility(mViewerFrame, GrafPtr(mPluginPort),
												frameOrigin, mPluginClipping);
	}
}

void MRJContext::showFrames()
{
	UInt32 frameCount;
	OSStatus status = ::JMCountAWTContextFrames(mContext, &frameCount);
	if (status == noErr) {
		for (UInt32 frameIndex = 0; frameIndex < frameCount; frameIndex++) {
			JMFrameRef frameRef;
			status = ::JMGetAWTContextFrame(mContext, frameIndex, &frameRef);
			if (status == noErr) {
				::JMFrameShowHide(frameRef, true);
//				MRJFrame* frame = getFrame(frameRef);
//				if (frame != NULL)
//					frame->focusEvent(false);
			}
		}
	}
}

void MRJContext::hideFrames()
{
	UInt32 frameCount;
	OSStatus status = ::JMCountAWTContextFrames(mContext, &frameCount);
	if (status == noErr) {
		for (UInt32 frameIndex = 0; frameIndex < frameCount; frameIndex++) {
			JMFrameRef frameRef;
			status = ::JMGetAWTContextFrame(mContext, frameIndex, &frameRef);
			if (status == noErr) {
				// make sure the frame doesn't have the focus.
				MRJFrame* frame = getFrame(frameRef);
				if (frame != NULL)
					frame->focusEvent(false);
				::JMFrameShowHide(frameRef, false);
			}
		}
	}
}

/**
 * Ensure that any frames Java still has a reference to are no longer valid, so that we won't crash
 * after a plugin instance gets shut down. This is called by the destructor just in case, to avoid
 * some hard freeze crashes I've seen.
 */
void MRJContext::releaseFrames()
{
	UInt32 frameCount;
	OSStatus status = ::JMCountAWTContextFrames(mContext, &frameCount);
	if (status == noErr) {
		for (UInt32 frameIndex = 0; frameIndex < frameCount; frameIndex++) {
			JMFrameRef frameRef = NULL;
			status = ::JMGetAWTContextFrame(mContext, frameIndex, &frameRef);
			if (status == noErr) {
				frameShowHide(frameRef, false);
				releaseFrame(mContext, frameRef);
			}
		}
	}
}

void MRJContext::setCodeBase(const char* codeBase)
{
	if (mCodeBase != NULL)
		delete[] mCodeBase;
	if (codeBase != NULL)
		mCodeBase = ::strdup(codeBase);
}

const char* MRJContext::getCodeBase()
{
	return mCodeBase;
}

void MRJContext::setDocumentBase(const char* documentBase)
{
	if (mDocumentBase != NULL)
		mDocumentBase = NULL;
	if (documentBase != NULL)
		mDocumentBase = ::strdup(documentBase); 
}

const char* MRJContext::getDocumentBase()
{
	return mDocumentBase;
}

MRJPage* MRJContext::findPage(const MRJPageAttributes& attributes)
{
	MRJPage* page = MRJPage::getFirstPage();
	while (page != NULL) {
		if (attributes.documentID == page->getDocumentID() &&
			::strcasecmp(attributes.codeBase, page->getCodeBase()) == 0 &&
			::strcasecmp(attributes.archive, page->getArchive()) == 0 &&
			attributes.mayScript == page->getMayScript()) {
			page->AddRef();
			return page;
		}
		page = page->getNextPage();
	}
	
	// create a unique page for this URL.
	page = new MRJPage(mSession, attributes);
	page->AddRef();
	return page;
}

struct EmptyPort : public CGrafPort {
	EmptyPort() {
		GrafPtr oldPort;
		::GetPort(&oldPort);
		::OpenCPort(this);
		::PortSize(0, 0);
		::SetEmptyRgn(this->visRgn);
		::SetEmptyRgn(this->clipRgn);
		::SetPort(oldPort);
	}
	
	~EmptyPort() {
		::CloseCPort(this);
	}
};

CGrafPtr MRJContext::getEmptyPort()
{
	static EmptyPort emptyPort;
	return &emptyPort;
}
