/* ----- BEGIN LICENSE BLOCK -----
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
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
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
 * ----- END LICENSE BLOCK ----- */

#include <string.h>
#include <stdlib.h>
#include <Appearance.h>
#include <Gestalt.h>
#include <TextCommon.h>

#if TARGET_CARBON
#include <CarbonEvents.h>
#include <JavaControl.h>
#include <JavaApplet.h>

template<class RefType>
class cfref {
    RefType mRef;
public:
    cfref() : mRef(NULL) {}
    cfref(RefType ref) : mRef(ref) {}
    ~cfref() { if(mRef) ::CFRelease(mRef); }
    
    RefType operator=(RefType ref)
    {
        if (mRef && mRef != ref)
            ::CFRelease(mRef);
        return (mRef = ref);
    }
    
    operator RefType() { return mRef; }
    operator int() { return (mRef != NULL); }

private:
    // cfref(const cfref<RefType>& other) {}
};

#endif

#include "MRJSession.h"
#include "MRJContext.h"
#include "MRJPlugin.h"
#include "MRJPage.h"
#include "MRJMonitor.h"
#include "AsyncMessage.h"
#include "LocalPort.h"
#include "StringUtils.h"
#include "TimedMessage.h"
#if !TARGET_CARBON
#include "TopLevelFrame.h"
#include "EmbeddedFrame.h"
#endif

#include "nsIPluginManager2.h"
#include "nsIPluginInstancePeer.h"
#include "nsIJVMPluginTagInfo.h"
#include "MRJSecurityContext.h"

#include <string>

using namespace std;

extern nsIPluginManager* thePluginManager;
extern nsIPluginManager2* thePluginManager2;

static void blinkRgn(RgnHandle rgn);

void LocalPort::Enter()
{
    ::GetPort(&fOldPort);
    if (fPort != fOldPort)
        ::SetPort(fPort);
    Rect portRect;
    GetPortBounds(fPort, &portRect);
    fOldOrigin.h = portRect.left, fOldOrigin.v = portRect.top;
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

static RgnHandle NewEmptyRgn()
{
    RgnHandle region = ::NewRgn();
    if (region != NULL) ::SetEmptyRgn(region);
    return region;
}

MRJContext::MRJContext(MRJSession* session, MRJPluginInstance* instance)
    :   mPluginInstance(instance), mSession(session), mPeer(NULL),
#if !TARGET_CARBON
        mLocator(NULL), mContext(NULL), mViewer(NULL), mViewerFrame(NULL),
#endif
        mIsActive(false), mIsFocused(false), mIsVisible(false),
        mPluginClipping(NULL), mPluginWindow(NULL), mPluginPort(NULL),
        mDocumentBase(NULL), mAppletHTML(NULL), mPage(NULL), mSecurityContext(NULL)
#if TARGET_CARBON
        , mAppletFrame(NULL), mAppletObject(NULL), mAppletControl(NULL), mScrollCounter(0)
#endif
{
    instance->GetPeer(&mPeer);
    
    // we cache attributes of the window, and periodically notice when they change.
    mCachedOrigin.x = mCachedOrigin.y = -1;
    ::SetRect((Rect*)&mCachedClipRect, 0, 0, 0, 0);
    mPluginClipping =::NewEmptyRgn();
    mPluginPort = getEmptyPort();
}

MRJContext::~MRJContext()
{
#if !TARGET_CARBON
    if (mLocator != NULL) {
        JMDisposeAppletLocator(mLocator);
        mLocator = NULL;
    }

    if (mViewer != NULL) {
        ::JMDisposeAppletViewer(mViewer);
        mViewer = NULL;
    }

    // hack:  see if this allows the applet viewer to terminate gracefully.
    // Re-enable.  Else if opened a new window thru Java and then quit browser,
    // get "SP_WARN: Yow! Invalid canvas->visRgn:0000000" in 'MRJSubPorts'
    // several times while applet viewer is closing

    for (int i = 0; i < 100; i++)
        ::JMIdle(mSessionRef, kDefaultJMTime);

    if (mContext != NULL) {
        // hack:  release any frames that we still see in the AWT context, before tossing it.
        releaseFrames();

        ::JMDisposeAWTContext(mContext);
        mContext = NULL;
    }
#endif /* !TARGET_CARBON */

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
    
    if (mDocumentBase != NULL) {
        delete[] mDocumentBase;
        mDocumentBase = NULL;
    }
    
    if (mAppletHTML != NULL) {
        delete[] mAppletHTML;
        mAppletHTML = NULL;
    }
    
    if (mSecurityContext != NULL) {
        mSecurityContext->Release();
        mSecurityContext = NULL;
    }

#if TARGET_CARBON
    if (mSession) {
        JNIEnv* env = mSession->getCurrentEnv();
        if (mAppletObject != NULL) {
            env->DeleteGlobalRef(mAppletObject);
            mAppletObject = NULL;
        }
        if (mAppletControl != NULL) {
            ::DisposeControl(mAppletControl);
            mAppletControl = NULL;
        }
        if (mAppletFrame != NULL) {
            OSStatus status;
            
            // disconnect callbacks.
            status = ::RegisterStatusCallback(env, mAppletFrame, NULL, NULL);
            status = ::RegisterShowDocumentCallback(env, mAppletFrame, NULL, NULL);
            
            // destroy the applet.
            status = ::SetJavaAppletState(env, mAppletFrame, kAppletDestroy);

            // env->DeleteGlobalRef(mAppletFrame);
            mAppletFrame = NULL;
        }
    }
#endif
}

#if !TARGET_CARBON

JMAWTContextRef MRJContext::getContextRef()
{
    return mContext;
}

JMAppletViewerRef MRJContext::getViewerRef()
{
    return mViewer;
}

#endif

Boolean MRJContext::appletLoaded()
{
#if TARGET_CARBON
    return (mAppletFrame && mAppletControl);
#else
    return (mViewer != NULL);
#endif
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

static bool isAppletAttribute(const char* name)
{
    // this table must be kept in alphabetical order.
    static const char* kAppletAttributes[] = {
        "ALIGN", "ALT", "ARCHIVE",
        "CODE", "CODEBASE",
        "HEIGHT", "HSPACE",
        "MAYSCRIPT", "NAME", "OBJECT",
        "VSPACE", "WIDTH"
    };
    int length = sizeof(kAppletAttributes) / sizeof(char*);
    int minIndex = 0, maxIndex = length - 1;
    int index = maxIndex / 2;
    while (minIndex <= maxIndex) {
        int diff = strcasecmp(name, kAppletAttributes[index]);
        if (diff < 0) {
            maxIndex = (index - 1);
            index = (minIndex + maxIndex) / 2;
        } else if (diff > 0) {
            minIndex = (index + 1);
            index = (minIndex + maxIndex) / 2;
        } else {
            return true;
        }
    }
    return false;
}

static void addAttribute(string& attrs, const char* name, const char* value)
{
    attrs += " ";
    attrs += name;
    attrs += "=\"";
    attrs += value;
    attrs += "\"";
}

static void addParameter(string& params, const char* name, const char* value)
{
    params += "<PARAM NAME=\"";
    params += name;
    params += "\" VALUE=\"";
    params += value;
    params += "\">\n";
}

static void addAttributes(nsIPluginTagInfo* tagInfo, string& attributes)
{
    PRUint16 count;
    const char* const* names;
    const char* const* values;
    if (tagInfo->GetAttributes(count, names, values) == NS_OK) {
        for (PRUint16 i = 0; i < count; ++i)
            addAttribute(attributes, names[i], values[i]);
    }
} 

static void addParameters(nsIPluginTagInfo2* tagInfo2, string& parameters)
{
    PRUint16 count;
    const char* const* names;
    const char* const* values;
    if (tagInfo2->GetParameters(count, names, values) == NS_OK) {
        for (PRUint16 i = 0; i < count; ++i)
            addParameter(parameters, names[i], values[i]);
    }
}

static void addObjectAttributes(nsIPluginTagInfo* tagInfo, string& attributes)
{
    PRUint16 count;
    const char* const* names;
    const char* const* values;
    const char kClassID[] = "classid";
    const char kJavaPrefix[] = "java:";
    const size_t kJavaPrefixSize = sizeof(kJavaPrefix) - 1;
    if (tagInfo->GetAttributes(count, names, values) == NS_OK) {
        for (PRUint16 i = 0; i < count; ++i) {
            const char* name = names[i];
            const char* value = values[i];
            if (strcasecmp(name, "classid") == 0 && strncmp(value, kJavaPrefix, kJavaPrefixSize) == 0)
                addAttribute(attributes, "code", value + kJavaPrefixSize);
            else
                addAttribute(attributes, name, value);
        }
    }
}

static void addEmbedAttributes(nsIPluginTagInfo* tagInfo, string& attributes, string& parameters)
{
    PRUint16 count;
    const char* const* names;
    const char* const* values;
    const char kJavaPluginAttributePrefix[] = "java_";
    const size_t kJavaPluginAttributePrefixSize = sizeof(kJavaPluginAttributePrefix) - 1;
    if (tagInfo->GetAttributes(count, names, values) == NS_OK) {
        for (PRUint16 i = 0; i < count; ++i) {
            const char* name = names[i];
            const char* value = values[i];
            if (strncmp(name, kJavaPluginAttributePrefix, kJavaPluginAttributePrefixSize) == 0)
                name += kJavaPluginAttributePrefixSize;
            if (isAppletAttribute(name)) {
                addAttribute(attributes, name, value);
            } else {
                // assume it's a parameter.
                addParameter(parameters, name, value);
            }
        }
    }
}

static char* synthesizeAppletElement(nsIPluginTagInfo* tagInfo)
{
    // just synthesize an <APPLET> element out of whole cloth.
    // this may be used because of the way the applet is being
    // instantiated, or it may be used to work around bugs
    // in the shipping browser.
    string element("<APPLET");
    string attributes("");
    string parameters("");
    
    nsIPluginTagInfo2* tagInfo2 = NULL;
    if (tagInfo->QueryInterface(NS_GET_IID(nsIPluginTagInfo2), (void**)&tagInfo2) == NS_OK) {
        nsPluginTagType tagType = nsPluginTagType_Unknown;
        if (tagInfo2->GetTagType(&tagType) == NS_OK) {
            switch (tagType) {
            case nsPluginTagType_Applet:
                addAttributes(tagInfo2, attributes);
                addParameters(tagInfo2, parameters);
                break;
            case nsPluginTagType_Object:
                addObjectAttributes(tagInfo2, attributes);
                addParameters(tagInfo2, parameters);
                break;
            case nsPluginTagType_Embed:
                addEmbedAttributes(tagInfo2, attributes, parameters);
                break;
            }
        }
        NS_RELEASE(tagInfo2);
    } else {
        addEmbedAttributes(tagInfo, attributes, parameters);
    }   
    
    element += attributes;
    element += ">\n";
    element += parameters;
    element += "</APPLET>\n";
    
    return ::strdup(element.c_str());
}

#if !TARGET_CARBON
static void fetchCompleted(JMAppletLocatorRef ref, JMLocatorErrors status) {}
#endif

#if TARGET_CARBON

inline CFMutableDictionaryRef createStringDictionary(PRUint16 capacity)
{
    return CFDictionaryCreateMutable(NULL, capacity,
                                     &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
}

static CFStringRef createUppercaseString(const char* cstr, CFStringEncoding encoding)
{
    CFMutableStringRef str = CFStringCreateMutable(NULL, strlen(cstr));
    if (str) {
        CFStringAppendCString(str, cstr, encoding);
        CFStringUppercase(str, NULL);
    }
    return str;
}

static CFMutableDictionaryRef getAttributes(nsIPluginTagInfo* tagInfo)
{
    PRUint16 count;
    const char* const* names;
    const char* const* values;
    if (tagInfo->GetAttributes(count, names, values) == NS_OK) {
        CFMutableDictionaryRef attributes = createStringDictionary(count);
        if (attributes != NULL) {
            for (PRUint16 i = 0; i < count; ++i) {
                cfref<CFStringRef> name = createUppercaseString(names[i], kCFStringEncodingUTF8);
                cfref<CFStringRef> value = CFStringCreateWithCString(NULL, values[i], kCFStringEncodingUTF8);
                if (name && value)
                    CFDictionaryAddValue(attributes, name, value);
            }
            return attributes;
        }
    }
    return NULL;
} 

static CFMutableDictionaryRef getParameters(nsIPluginTagInfo2* tagInfo2)
{
    PRUint16 count;
    const char* const* names;
    const char* const* values;
    if (tagInfo2->GetParameters(count, names, values) == NS_OK) {
        CFMutableDictionaryRef parameters = createStringDictionary(count);
        if (parameters) {
            for (PRUint16 i = 0; i < count; ++i) {
                cfref<CFStringRef> name = createUppercaseString(names[i], kCFStringEncodingUTF8);
                cfref<CFStringRef> value = CFStringCreateWithCString(NULL, values[i], kCFStringEncodingUTF8);
                if (name && value)
                    CFDictionaryAddValue(parameters, name, value);
            }
            return parameters;
        }
    }
    return NULL;
}

#endif

void MRJContext::processAppletTag()
{
    // use the applet's HTML element to create a locator. this is required
    // in general, to specify a separate CODEBASE.

    nsIPluginTagInfo* tagInfo = NULL;
    if (mPeer->QueryInterface(NS_GET_IID(nsIPluginTagInfo), (void**)&tagInfo) == NS_OK) {
        nsIPluginTagInfo2* tagInfo2 = NULL;
        if (tagInfo->QueryInterface(NS_GET_IID(nsIPluginTagInfo2), (void**)&tagInfo2) == NS_OK) {
            nsPluginTagType tagType = nsPluginTagType_Unknown;
            if (tagInfo2->GetTagType(&tagType) == NS_OK) {
                // get the URL of the HTML document containing the applet, and the
                // fragment of HTML that defines this applet itself.
                const char* documentBase = NULL;
                if (tagInfo2->GetDocumentBase(&documentBase) == NS_OK)
                    setDocumentBase(documentBase);
                const char* appletHTML = NULL;
                if (tagInfo2->GetTagText(&appletHTML) == NS_OK)
                    setAppletHTML(appletHTML, tagType);
                else
                    mAppletHTML = synthesizeAppletElement(tagInfo);
            }

            // to support applet communication, put applets from the same document, codebase, and mayscript setting
            // in the same page.

            // establish a page context for this applet to run in.
            nsIJVMPluginTagInfo* jvmTagInfo = NULL;
            if (mPeer->QueryInterface(NS_GET_IID(nsIJVMPluginTagInfo), (void**)&jvmTagInfo) == NS_OK) {
                PRUint32 documentID;
                const char* codeBase;
                const char* archive;
                PRBool mayScript;
                if (tagInfo2->GetUniqueID(&documentID) != NS_OK) documentID = 0;
                if (jvmTagInfo->GetCodeBase(&codeBase) != NS_OK || codeBase == NULL) codeBase = "";
                if (jvmTagInfo->GetArchive(&archive) != NS_OK || archive == NULL) archive = "";
                if (jvmTagInfo->GetMayScript(&mayScript) != NS_OK) mayScript = PR_FALSE;
                MRJPageAttributes pageAttributes = { documentID, codeBase, archive, mayScript };
                mPage = findPage(pageAttributes);
                NS_RELEASE(jvmTagInfo);
            }

            NS_RELEASE(tagInfo2);
        } else {
            mAppletHTML = synthesizeAppletElement(tagInfo);
        }
        
        NS_RELEASE(tagInfo);
    }
       
    if (mDocumentBase != NULL && mAppletHTML != NULL) {
        // example that works.
        // const char* kBaseURL = "http://java.sun.com/applets/other/ImageLoop/index.html";
        // const char* kAppletHTML = "<applet codebase=\"classes\" code=ImageLoopItem width=55 height=68>\n"
        //                           "<param name=nimgs value=10>\n"
        //                           "<param name=img value=\"images\">\n"
        //                           "<param name=pause value=1000>\n"
        //                           "</applet>";

#if !TARGET_CARBON
        static JMAppletLocatorCallbacks callbacks = {
            kJMVersion,                     /* should be set to kJMVersion */
            &fetchCompleted,                /* called when the html has been completely fetched */
        };
        OSStatus status;
        JMTextRef urlRef = NULL, htmlRef = NULL;

        TextEncoding utf8 = CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicodeUTF8Format);

        status = ::JMNewTextRef(mSessionRef, &urlRef, utf8, mDocumentBase, strlen(mDocumentBase));
        if (status != noErr) goto done;

        status = ::JMNewTextRef(mSessionRef, &htmlRef, utf8, mAppletHTML, strlen(mAppletHTML));
        if (status != noErr) goto done;

        status = ::JMNewAppletLocator(&mLocator, mSessionRef, &callbacks,
                                      urlRef, htmlRef, JMClientData(this));
        
    done:
        if (urlRef != NULL)
            ::JMDisposeTextRef(urlRef);
        if (htmlRef != NULL)
            ::JMDisposeTextRef(htmlRef);        
#endif  /* !TARGET_CARBON */

    }
}

#if !TARGET_CARBON
static MRJFrame* getFrame(JMFrameRef ref)
{
    MRJFrame* frame = NULL;

    if (ref != NULL)
        ::JMGetFrameData(ref, (JMClientData*)&frame);
    
    return frame;
}
#endif

Boolean MRJContext::createContext()
{
    return true;
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
    :   AsyncMessage(pluginInstance->getSession()),
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

#if !TARGET_CARBON
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
        synchronizeClipping();
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

void MRJContext::setProxyInfoForURL(char * url, JMProxyType proxyType) 
{
    /*
     * We then call 'nsIPluginManager2::FindProxyForURL' which will return
     * proxy information which we can parse and set via JMSetProxyInfo.
     */ 
    char* proxy = NULL;
    nsresult rv = thePluginManager2->FindProxyForURL(url, &proxy);
    if (NS_SUCCEEDED(rv) && proxy != NULL) {
        /* See if a proxy was specified */
        if (strcmp("DIRECT", proxy) != 0) {
            JMProxyInfo proxyInfo;
            proxyInfo.useProxy = true;
            char* space = strchr(proxy, ' ');
            if (space != NULL) {
                char* host = space + 1;
                char* colon = ::strchr(host, ':');
                int length = (colon - host);
                if (length < sizeof(proxyInfo.proxyHost)) {
                    strncpy(proxyInfo.proxyHost, host, length);
                    proxyInfo.proxyPort = atoi(colon + 1);
                    ::JMSetProxyInfo(mSessionRef, proxyType, &proxyInfo);
                }
            }
        }
        
        delete[] proxy;
    }
}

#endif /* !TARGET_CARBON */

#if TARGET_CARBON

void* TimedMessage::operator new(size_t n)
{
    return (void*) NewPtr(n);
}

void TimedMessage::operator delete(void* ptr)
{
    DisposePtr(Ptr(ptr));
}

TimedMessage::TimedMessage()
    :   mTimerUPP(NULL)
{
    mTimerUPP = NewEventLoopTimerUPP(TimedMessageHandler);
}

TimedMessage::~TimedMessage()
{
     if (mTimerUPP) ::DisposeEventLoopTimerUPP(mTimerUPP);
}

OSStatus TimedMessage::send()
{
    EventLoopTimerRef timerRef;
    return ::InstallEventLoopTimer(::GetMainEventLoop(), 0, 0,
                                   mTimerUPP, this, &timerRef);
}

pascal void TimedMessage::TimedMessageHandler(EventLoopTimerRef inTimer, void *inUserData)
{
    // prevent this timer from ever firing again.
    RemoveEventLoopTimer(inTimer);
    TimedMessage* message = reinterpret_cast<TimedMessage*>(inUserData);
    message->execute();
    delete message;
}

static char* getCString(CFStringRef stringRef)
{
	// worst case length scenario would encode every character
    CFIndex len = (1 + ::CFStringGetLength(stringRef)) * 3;
    char* result = new char[len];
    if (result)
        CFStringGetCString(stringRef, result, len, kCFStringEncodingUTF8);
    return result;
}

class SetStatusMessage : public TimedMessage {
    nsIPluginInstance* mPluginInstance;
    CFStringRef mStatus;

public:
    SetStatusMessage(nsIPluginInstance* pluginInstance, CFStringRef statusString)
        : mPluginInstance(pluginInstance), mStatus(statusString)
    {
        NS_ADDREF(mPluginInstance);
        ::CFRetain(mStatus);
    }
    
    ~SetStatusMessage()
    {
        ::CFRelease(mStatus);
        NS_RELEASE(mPluginInstance);
    }
    
    virtual void execute();
};

void SetStatusMessage::execute()
{
    char* status = getCString(mStatus);
    if (status) {
        nsIPluginInstancePeer* peer;
        mPluginInstance->GetPeer(&peer);
        if (peer) {
            peer->ShowStatus(status);
            NS_RELEASE(peer);
        }
        delete[] status;
    }
}

static void setStatusCallback(jobject applet, CFStringRef statusMessage, void *inUserData)
{
    // use a timer on the main event loop to handle this?
    MRJContext* context = reinterpret_cast<MRJContext*>(inUserData);
    SetStatusMessage* message = new SetStatusMessage(context->getInstance(), statusMessage);
    if (message) {
        OSStatus status = message->send();
        if (status != noErr) delete message;
    }
}

class ShowDocumentMessage : public TimedMessage {
    MRJPluginInstance* mPluginInstance;
    CFURLRef mURL;
    CFStringRef mWindowName;

public:
    ShowDocumentMessage(MRJPluginInstance* pluginInstance, CFURLRef url, CFStringRef windowName)
        :   mPluginInstance(pluginInstance), mURL(url), mWindowName(windowName)
    {
        NS_ADDREF(mPluginInstance);
        ::CFRetain(mURL);
        ::CFRetain(mWindowName);
    }
    
    ~ShowDocumentMessage()
    {
        ::CFRelease(mWindowName);
        ::CFRelease(mURL);
        NS_RELEASE(mPluginInstance);
    }
    
    virtual void execute();
};

static OSErr FSpGetFullPath(const FSSpec *spec, short *fullPathLength, Handle *fullPath)
{
    OSErr       result;
    OSErr       realResult;
    FSSpec      tempSpec;
    CInfoPBRec  pb;
    
    *fullPathLength = 0;
    *fullPath = NULL;
    
    // Default to noErr
    realResult = noErr;
    
    /* Make a copy of the input FSSpec that can be modified */
    BlockMoveData(spec, &tempSpec, sizeof(FSSpec));
    
    if ( tempSpec.parID == fsRtParID )
    {
        /* The object is a volume */
        
        /* Add a colon to make it a full pathname */
        ++tempSpec.name[0];
        tempSpec.name[tempSpec.name[0]] = ':';
        
        /* We're done */
        result = PtrToHand(&tempSpec.name[1], fullPath, tempSpec.name[0]);
    }
    else
    {
        /* The object isn't a volume */
        
        /* Is the object a file or a directory? */
        pb.dirInfo.ioNamePtr = tempSpec.name;
        pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
        pb.dirInfo.ioDrDirID = tempSpec.parID;
        pb.dirInfo.ioFDirIndex = 0;
        result = PBGetCatInfoSync(&pb);
        // Allow file/directory name at end of path to not exist.
        realResult = result;
        if ( (result == noErr) || (result == fnfErr) )
        {
            /* if the object is a directory, append a colon so full pathname ends with colon */
            if ( (result == noErr) && (pb.hFileInfo.ioFlAttrib & ioDirMask) != 0 )
            {
                ++tempSpec.name[0];
                tempSpec.name[tempSpec.name[0]] = ':';
            }
            
            /* Put the object name in first */
            result = PtrToHand(&tempSpec.name[1], fullPath, tempSpec.name[0]);
            if ( result == noErr )
            {
                /* Get the ancestor directory names */
                pb.dirInfo.ioNamePtr = tempSpec.name;
                pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
                pb.dirInfo.ioDrParID = tempSpec.parID;
                do  /* loop until we have an error or find the root directory */
                {
                    pb.dirInfo.ioFDirIndex = -1;
                    pb.dirInfo.ioDrDirID = pb.dirInfo.ioDrParID;
                    result = PBGetCatInfoSync(&pb);
                    if ( result == noErr )
                    {
                        /* Append colon to directory name */
                        ++tempSpec.name[0];
                        tempSpec.name[tempSpec.name[0]] = ':';
                        
                        /* Add directory name to beginning of fullPath */
                        (void) Munger(*fullPath, 0, NULL, 0, &tempSpec.name[1], tempSpec.name[0]);
                        result = MemError();
                    }
                } while ( (result == noErr) && (pb.dirInfo.ioDrDirID != fsRtDirID) );
            }
        }
    }
    if ( result == noErr )
    {
        /* Return the length */
        *fullPathLength = GetHandleSize(*fullPath);
        result = realResult;    // return realResult in case it was fnfErr
    }
    else
    {
        /* Dispose of the handle and return NULL and zero length */
        if ( *fullPath != NULL )
        {
            DisposeHandle(*fullPath);
        }
        *fullPath = NULL;
        *fullPathLength = 0;
    }
    
    return ( result );
}

void ShowDocumentMessage::execute()
{
    // guard against the plugin instance already having been destroyed.
    if (mPluginInstance->getContext() == NULL) {
        return;
    }
    
    char* url = NULL;
    char* target = NULL;

    CFStringRef urlRef = ::CFURLGetString(mURL);
    if (urlRef) {
        if (::CFStringHasPrefix(urlRef, CFSTR("file:"))) {
            // work around for bug #108519, convert to HFS+ file system path.
            // question:  why can't CFURLGetFSRef be used to extract the FSRef
            // directly from the FSRef provided? I believe it's in the wrong
            // format, file:/path/to/document.html
            // 1. convert URL to an FSRef.
            FSRef fileRef;
            UInt8* urlBytes = (UInt8*) getCString(urlRef);
            ::CFRelease(urlRef);
            if (urlBytes && ::FSPathMakeRef(urlBytes + 5, &fileRef, NULL) == noErr) {
                delete[] urlBytes;
                // 2. convert FSRef to FSSpec.
                FSSpec fileSpec;
                if (::FSGetCatalogInfo(&fileRef, kFSCatInfoNone, NULL, NULL, &fileSpec, NULL) == noErr) {
                    // 3. Generate the full HFS+ path from the FSSpec.
                    Handle fullPath;
                    short fullPathLength;
                    if (::FSpGetFullPath(&fileSpec, &fullPathLength, &fullPath) == noErr) {
                        // 4. Prefix the HFS+ path with "file:///", convert ':' to '/'.
                        const char prefix[] = { "file:///" };
                        const size_t prefixLength = sizeof(prefix) - 1;
                        size_t urlLength = fullPathLength + prefixLength;
                        url = new char[1 + urlLength];
                        if (url) {
                            memcpy(url, prefix, prefixLength);
                            memcpy(url + prefixLength, *fullPath, fullPathLength);
                            ::DisposeHandle(fullPath);
                            url[urlLength] = '\0';
                            char* colon = strchr(url + prefixLength, ':');
                            while (colon) {
                                *colon = '/';
                                colon = strchr(colon + 1, ':');
                            }
                        }
                    }
                }
            }
        } else {
            url = getCString(urlRef);
            ::CFRelease(urlRef);
        }
    }
    
    target = getCString(mWindowName);
    
    if (url && target)
        thePluginManager->GetURL((nsIPluginInstance*)mPluginInstance, url, target);

    delete[] url;
    delete[] target;
}

static void showDocumentCallback(jobject applet, CFURLRef url, CFStringRef windowName, void *inUserData)
{
    // workaround for bug #108054, sometimes url parameter is NULL.
    if (url) {
        // use a timer on the main event loop to handle this?
        MRJContext* context = reinterpret_cast<MRJContext*>(inUserData);
        ShowDocumentMessage* message = new ShowDocumentMessage((MRJPluginInstance*)context->getInstance(), url, windowName);
        if (message) {
            OSStatus status = message->send();
            if (status != noErr) delete message;
        }
    }
}

static void fixAttributes(CFMutableDictionaryRef attributes, nsIPluginTagInfo* tagInfo, nsPluginWindow* window)
{
    // check for WIDTH & HEIGHT attributes that end with % or *, and replace those with the physical values.
    const char* width;
    if (tagInfo->GetAttribute("WIDTH", &width) == NS_OK) {
        if (strpbrk(width, "%*")) {
            cfref<CFStringRef> actualWidth = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), window->width);
            CFDictionaryReplaceValue(attributes, CFSTR("WIDTH"), actualWidth);
        }
    }
    
    const char* height;
    if (tagInfo->GetAttribute("HEIGHT", &height) == NS_OK) {
        if (strpbrk(height, "%*")) {
            cfref<CFStringRef> actualHeight = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), window->height);
            CFDictionaryReplaceValue(attributes, CFSTR("HEIGHT"), actualHeight);
        }
    }
}

#endif /* TARGET_CARBON */

Boolean MRJContext::loadApplet()
{
    OSStatus status = noErr;
    
#if TARGET_CARBON
    // Use whizzy new JavaEmbedding APIs to construct an AppletDescriptor.
    
    // gather all attributes and parameters.
    cfref<CFMutableDictionaryRef> attributes, parameters;
    nsIPluginTagInfo2* tagInfo2 = NULL;
    if (mPeer->QueryInterface(NS_GET_IID(nsIPluginTagInfo2), (void**)&tagInfo2) == NS_OK) {
        attributes = getAttributes(tagInfo2);
        fixAttributes(attributes, tagInfo2, mPluginWindow); 
        parameters = getParameters(tagInfo2);
        NS_RELEASE(tagInfo2);
    }
    
    cfref<CFURLRef> documentBase;
    if (!TARGET_RT_MAC_MACHO && ::strncmp(mDocumentBase, "file:", 5) == 0) {
        // file: URLs. Need to create the URL from HFS+ style using CFURLCreateWithFileSystemPath.
        // this is to workaround the same problem in bug #108519, Mozilla uses HFS+ paths that are
        // simply converted to '/' delimited URLs. The mach-o build won't have this problem, so
        // we'll need to detect that build somehow.
        char* path = mDocumentBase + 5;
        while (*path == '/') ++path;
        path = strdup(path);
        if (path) {
            // convert '/' to ':' characters.
            char* slash = strchr(path + 1, '/');
            while (slash) {
                *slash = ':';
                slash = strchr(slash + 1, '/');
            }
            cfref<CFStringRef> pathRef = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
            delete[] path;
            if (pathRef) {
                // unescape the path in case there are spaces in it
                cfref<CFStringRef> unescPathRef = CFURLCreateStringByReplacingPercentEscapes(NULL, pathRef, CFSTR(""));
                if (unescPathRef)
                    documentBase = CFURLCreateWithFileSystemPath(NULL, unescPathRef, kCFURLHFSPathStyle, false);
            }
        }
    } else {
        documentBase = CFURLCreateWithBytes(NULL, (const UInt8*)mDocumentBase,
                                            strlen(mDocumentBase), kCFStringEncodingUTF8, NULL);
    }
    
    if (attributes && parameters && documentBase) {
        AppletDescriptor desc = {
            documentBase,
            NULL,
            attributes,
            parameters,
        };
        JNIEnv* env = mSession->getCurrentEnv();
        status = ::CreateJavaApplet(env, desc, false, kUniqueArena, &mAppletFrame);
        if (status == noErr) {
            // install status/document callbacks.
            status = ::RegisterStatusCallback(env, mAppletFrame, &setStatusCallback, this);
            status = ::RegisterShowDocumentCallback(env, mAppletFrame, &showDocumentCallback, this);

            // wrap applet in a control.
            Rect bounds = { 0, 0, 100, 100 };
            status = ::CreateJavaControl(env, GetWindowFromPort(mPluginPort), &bounds, mAppletFrame, true, &mAppletControl);
            if (status == noErr) {
                status = ::SetJavaAppletState(env, mAppletFrame, kAppletStart);
            }
        }
    }
#else
    static JMAppletSecurity security = {
        kJMVersion,                     /* should be set to kJMVersion */
        eAppletHostAccess,              /* can this applet access network resources */
        eLocalAppletAccess,             /* can this applet access network resources */
        true,                           /* restrict access to system packages (com.apple.*, sun.*, netscape.*) also found in the property "mrj.security.system.access" */
        true,                           /* restrict classes from loading system packages (com.apple.*, sun.*, netscape.*) also found in the property "mrj.security.system.define" */
        true,                           /* restrict access to application packages found in the property "mrj.security.application.access" */
        true,                           /* restrict access to application packages found in the property "mrj.security.application.access" */
    };
    static JMAppletViewerCallbacks callbacks = {
        kJMVersion,                     /* should be set to kJMVersion */
        &showDocument,                  /* go to a url, optionally in a new window */
        &setStatusMessage,              /* applet changed status message */
    };
    
    /* Added by Mark: */
    /* 
     * Set proxy info 
     * It is only set if the new enhanced Plugin Manager exists.
     */
    if (thePluginManager2 != NULL) {
        /* Sample URL's to use for getting the HTTP proxy and FTP proxy */
        setProxyInfoForURL("http://www.mozilla.org/", eHTTPProxy);
        setProxyInfoForURL("ftp://ftp.mozilla.org/", eFTPProxy);
    }
    /* End set proxy info code */
    
    status = ::JMNewAppletViewer(&mViewer, mContext, mLocator, 0,
                                            &security, &callbacks, this);
    if (status == noErr) {
        status = ::JMSetAppletViewerData(mViewer, JMClientData(this));
        status = ::JMReloadApplet(mViewer);
    }
    
    if (status == noErr) {
        // for grins, force the applet to load right away, so we can report any error we might encounter eagerly.
        // jobject appletObject = getApplet();
    }
#endif
    
    return (status == noErr);
}

void MRJContext::suspendApplet()
{
#if !TARGET_CARBON
    if (mViewer != NULL)
        ::JMSuspendApplet(mViewer);
#endif
}

void MRJContext::resumeApplet()
{
#if !TARGET_CARBON
    if (mViewer != NULL)
        ::JMResumeApplet(mViewer);
#endif
}

static const char* getAttribute(nsIPluginInstancePeer* peer, const char* name)
{
    const char* value = NULL;
    nsIPluginTagInfo* tagInfo = NULL;
    if (NS_SUCCEEDED(peer->QueryInterface(NS_GET_IID(nsIPluginTagInfo), (void**)&tagInfo))) {
        tagInfo->GetAttribute(name, &value);
        NS_RELEASE(tagInfo);
    }
    return value;
}

jobject MRJContext::getApplet()
{
#if TARGET_CARBON
    if (appletLoaded() && mAppletObject == NULL) {
        // mAppletFrame is an instance of javap com.apple.mrj.JavaEmbedding.JE_AppletViewer, as of Mac OS X 10.1.
        // In Mac OS X 10.1, it implemented the java.applet.AppletContext interface, but this is no longer true
        // in Mac OS X 10.2 (Jaguar). However both versions of JE_AppletViewer have a public field panel, which
        // is an instance of com.apple.mrj.JavaEmbedding.JE_AppletViewerPanel, which extends sun.applet.AppletPanel,
        // which contains the method public java.applet.Applet getApplet(). Apple needs to provide us with an API
        // that we can use, but for now this is the best that we can do.
        JNIEnv* env = mSession->getCurrentEnv();
        jclass frameClass = env->GetObjectClass(mAppletFrame);
        if (frameClass) {
            jfieldID panelID = env->GetFieldID(frameClass, "panel", "Lcom/apple/mrj/JavaEmbedding/JE_AppletViewerPanel;");
            if (panelID) {
                jobject appletPanel = env->GetObjectField(mAppletFrame, panelID);
                if (appletPanel) {
                    jclass panelClass = env->GetObjectClass(appletPanel);
                    jmethodID getAppletMethod = env->GetMethodID(panelClass, "getApplet", "()Ljava/applet/Applet;");
                    if (getAppletMethod) {
                        jobject applet = env->CallObjectMethod(appletPanel, getAppletMethod);
                        if (applet) {
                            mAppletObject = env->NewGlobalRef(applet);
                            env->DeleteLocalRef(applet);
                        }
                    } else {
                        env->ExceptionClear();
                    }
                    env->DeleteLocalRef(panelClass);
                    env->DeleteLocalRef(appletPanel);
                } else {
                    env->ExceptionClear();
                }
            } else {
                env->ExceptionClear();
            }
            env->DeleteLocalRef(frameClass);
        } else {
            env->ExceptionClear();
        }
    }
    return mAppletObject;
#else
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
                    if (mDocumentBase != NULL) {
                        appletURL[0] = ::strlen(mDocumentBase);
                        ::BlockMoveData(mDocumentBase, appletURL + 1, mDocumentBase[0]);
                    }
                    ::StandardAlert(kAlertPlainAlert, "\pApplet failed to load from URL:", appletURL, NULL, &itemHit);
                }
            }
        }
        return appletObject;
    }
#endif
}

nsIPluginInstance* MRJContext::getInstance()
{
    return mPluginInstance;
}

nsIPluginInstancePeer* MRJContext::getPeer()
{
    return mPeer;
}

/**
 * At various times during development, it is useful to see what bits are supposed to be drawn by
 * the applet. Defining DEBUG_CLIPPING as 1 turns on this behavior.
 */
#define DEBUG_CLIPPING 0

void MRJContext::drawApplet()
{
#if TARGET_CARBON
    // ??? It's a control. Draw it with Draw1Control?
    JNIEnv* env = mSession->getCurrentEnv();
    OSStatus status = ::DrawJavaControl(env, mAppletControl);
#else
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
#endif /* !TARGET_CARBON */
}

void MRJContext::printApplet(nsPluginWindow* printingWindow)
{
#if TARGET_CARBON
#else
    jobject frameObject = NULL;
    jclass utilsClass = NULL;
    OSStatus status = noErr;
    JNIEnv* env = ::JMGetCurrentEnv(mSessionRef); 
    
    // put the printing port into window coordinates:  (0, 0) in upper left corner.
    GrafPtr printingPort = GrafPtr(printingWindow->window->port);
    LocalPort localPort(printingPort);
    localPort.Enter();
    ::ClipRect((Rect*)&printingWindow->clipRect);

    do {
        // try to use the printing API, if that fails, fall back on netscape.oji.AWTUtils.printContainer().
        if (&::JMDrawFrameInPort != NULL) {
            Point frameOrigin = { printingWindow->y, printingWindow->x };
            status = ::JMDrawFrameInPort(mViewerFrame, printingPort, frameOrigin, printingPort->clipRgn, false);
            if (status == noErr) break;
        }

        // get the frame object to print.
        frameObject = ::JMGetAWTFrameJNIObject(mViewerFrame, env);
        if (frameObject == NULL) break;

        // call the print methods of the applet viewer's frame.
        jclass utilsClass = env->FindClass("netscape/oji/AWTUtils");
        if (utilsClass == NULL) break;
        jmethodID printContainerMethod = env->GetStaticMethodID(utilsClass, "printContainer", "(Ljava/awt/Container;IIILjava/lang/Object;)V");
        if (printContainerMethod == NULL) break;
        
        // create a monitor to synchronize with.
        MRJMonitor notifier(mSession);

        // start the asynchronous print call.
        jvalue args[5];
        args[0].l = frameObject;
        args[1].i = jint(printingPort);
        args[2].i = jint(printingWindow->x);
        args[3].i = jint(printingWindow->y);
        args[4].l = notifier.getObject();
        OSStatus status = ::JMExecJNIStaticMethodInContext(mContext, env, utilsClass, printContainerMethod, 5, args);

        // now, wait for the print method to complete.
        if (status == noErr)
            notifier.wait();
    } while (0);

    // restore the origin & port.
    localPort.Exit();

    if (frameObject != NULL)
        env->DeleteLocalRef(frameObject);
    if (utilsClass != NULL)
        env->DeleteLocalRef(utilsClass);
#endif /* !TARGET_CARBON */
}

void MRJContext::activate(Boolean active)
{
#if TARGET_CARBON
    // ???
#else
    if (mViewerFrame != NULL) {
        ::JMFrameActivate(mViewerFrame, active);
        mIsActive = active;
    } else {
        mIsActive = false;
    }
#endif
}

void MRJContext::resume(Boolean inFront)
{
#if TARGET_CARBON
    // ???
#else
    if (mViewerFrame != NULL) {
        ::JMFrameResume(mViewerFrame, inFront);
    }
#endif
}

void MRJContext::click(const EventRecord* event, MRJFrame* appletFrame)
{
#if TARGET_CARBON
    // won't Carbon events just take care of everything?
    LocalPort port(mPluginPort);
    port.Enter();

    Point localWhere = event->where;
    ::GlobalToLocal(&localWhere);
    ::HandleControlClick(mAppletControl, localWhere,
                         event->modifiers, NULL);

    // the Java control seems to focus itself automatically when clicked in.
    mIsFocused = true;
    
    port.Exit();
#else
    nsPluginPort* npPort = mPluginWindow->window;
    
    // make the plugin's port current, and move its origin to (0, 0).
    LocalPort port(GrafPtr(npPort->port));
    port.Enter();

    // will we always be called in the right coordinate system?
    Point localWhere = event->where;
    ::GlobalToLocal(&localWhere);
    nsPluginRect& clipRect = mCachedClipRect;
    Rect bounds = { clipRect.top, clipRect.left, clipRect.bottom, clipRect.right };
    if (PtInRect(localWhere, &bounds)) {
        localToFrame(&localWhere);
        appletFrame->click(event, localWhere);
    }
    
    // restore the plugin port's origin, and restore the current port.
    port.Exit();
#endif
}

void MRJContext::keyPress(long message, short modifiers)
{
#if TARGET_CARBON
    // won't Carbon events just take care of everything?
#else
    if (mViewerFrame != NULL) {
        ::JMFrameKey(mViewerFrame, message & charCodeMask,
                    (message & keyCodeMask) >> 8, modifiers);
    }
#endif
}

void MRJContext::keyRelease(long message, short modifiers)
{
#if TARGET_CARBON
    // won't Carbon events just take care of everything?
#else
    if (mViewerFrame != NULL) {
        ::JMFrameKeyRelease(mViewerFrame, message & charCodeMask,
                    (message & keyCodeMask) >> 8, modifiers);
    }
#endif
}

void MRJContext::scrollingBegins()
{
    if (mScrollCounter++ == 0) {
        if (mAppletControl) {
            JNIEnv* env = mSession->getCurrentEnv();
            StopJavaControlAsyncDrawing(env, mAppletControl);
        }
    }
}

void MRJContext::scrollingEnds()
{
    if (--mScrollCounter == 0) {
        if (mAppletControl) {
            synchronizeClipping();
            JNIEnv* env = mSession->getCurrentEnv();
            RestartJavaControlAsyncDrawing(env, mAppletControl);
        }
    }
}

Boolean MRJContext::handleEvent(EventRecord* event)
{
    Boolean eventHandled = false;
    if (mAppletControl) {
        eventHandled = true;
        switch (event->what) {
        case updateEvt:
            drawApplet();
            break;
            
        case keyDown:
            if (mIsFocused) {
                ::HandleControlKey(mAppletControl,
                                   (event->message & keyCodeMask) >> 8,
                                   (event->message & charCodeMask),
                                   event->modifiers);
                eventHandled = true;
            }
            break;  

        case mouseDown:
            click(event, NULL);
            eventHandled = mIsFocused;
            break;

        case nsPluginEventType_GetFocusEvent:
            if (!mIsFocused) {
                ::SetKeyboardFocus(::GetWindowFromPort(mPluginPort), mAppletControl, kControlFocusNextPart);
                mIsFocused = true;
            }
            break;
        
        case nsPluginEventType_LoseFocusEvent:
            ::SetKeyboardFocus(::GetWindowFromPort(mPluginPort), mAppletControl, kControlFocusNoPart);
            mIsFocused = true;
            break;

        case nsPluginEventType_AdjustCursorEvent:
            ::IdleControls(::GetWindowFromPort(mPluginPort));
            break;
        
        case nsPluginEventType_MenuCommandEvent:
            // frame->menuSelected(event->message, event->modifiers);
            break;
        
        case nsPluginEventType_ScrollingBeginsEvent:
            scrollingBegins();
            break;
        
        case nsPluginEventType_ScrollingEndsEvent:
            scrollingEnds();
            break;
        }
    }
    return eventHandled;
}

void MRJContext::idle(short modifiers)
{
#if TARGET_CARBON
    // won't Carbon events just take care of everything?
#else
    // Put the port in to proper window coordinates.
    nsPluginPort* npPort = mPluginWindow->window;
    LocalPort port(GrafPtr(npPort->port));
    port.Enter();
    
    Point pt;
    ::GetMouse(&pt);
    localToFrame(&pt);
    ::JMFrameMouseOver(mViewerFrame, pt, modifiers);
    
    port.Exit();
#endif
}

// interim routine for compatibility between OLD plugin interface and new.

// FIXME:  when the window goes away, npWindow/pluginWindow is passed in as NULL.
// should tell the AWTContext that the window has gone away. could use an offscreen
// or empty clipped grafport.



OSStatus MRJContext::installEventHandlers(WindowRef window)
{
    // install mouseDown/mouseUp handlers for this window, so we can disable
    // async updates during mouse tracking.
    return noErr;
}

OSStatus MRJContext::removeEventHandlers(WindowRef window)
{
    return noErr;
}

void MRJContext::setWindow(nsPluginWindow* pluginWindow)
{
    if (pluginWindow != NULL) {
        mPluginWindow = pluginWindow;

        // establish the GrafPort the plugin will draw in.
        mPluginPort = pluginWindow->window->port;

        if (! appletLoaded())
            loadApplet();
    } else {
        // tell MRJ the window has gone away.
        mPluginWindow = NULL;
        
        // use a single, 0x0, empty port for all future drawing.
        mPluginPort = getEmptyPort();
    }
    synchronizeClipping();
}

static Boolean equalRect(const nsPluginRect* r1, const nsPluginRect* r2)
{
    SInt32* r1p = (SInt32*)r1;
    SInt32* r2p = (SInt32*)r2;
    return (r1p[0] == r2p[0] && r1p[1] == r2p[1]);
}

Boolean MRJContext::inspectWindow()
{
    // don't bother looking, if the applet doesn't exist yet.
    if (!appletLoaded())
        return false;

    Boolean recomputeClipping = false;
    
    if (mPluginWindow != NULL) {
        // Check for origin or clipping changes.
        nsPluginPort* npPort = mPluginWindow->window;
        if (mCachedOrigin.x != npPort->portx || mCachedOrigin.y != npPort->porty || !equalRect(&mCachedClipRect, &mPluginWindow->clipRect)) {
            // transfer over values to the window cache.
            recomputeClipping = true;
        }
    }
    
    if (recomputeClipping)
        synchronizeClipping();
    
    return recomputeClipping;
}

/**
 * This routine ensures that the browser and MRJ agree on what the current clipping
 * should be. If the browser has assigned us a window to draw in (see setWindow()
 * above), then we use that window's clipRect to set up clipping, which is cached
 * in mPluginClipping, as a region. Otherwise, mPluginClipping is set to an empty
 * region.
 */
void MRJContext::synchronizeClipping()
{
    // this is called on update events to make sure the clipping region is in sync with the browser's.
    if (mPluginWindow != NULL) {
        // plugin clipping is intersection of clipRgn and the clipRect.
        nsPluginRect clipRect = mPluginWindow->clipRect;
        nsPluginPort* pluginPort = mPluginWindow->window;
        clipRect.left += pluginPort->portx, clipRect.right += pluginPort->portx;
        clipRect.top += pluginPort->porty, clipRect.bottom += pluginPort->porty;
        ::SetRectRgn(mPluginClipping, clipRect.left, clipRect.top, clipRect.right, clipRect.bottom);
    } else {
        ::SetEmptyRgn(mPluginClipping);
    }
    synchronizeVisibility();
}

MRJFrame* MRJContext::findFrame(WindowRef window)
{
    MRJFrame* frame = NULL;

#if !TARGET_CARBON
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
#endif
    
    return frame;
}

GrafPtr MRJContext::getPort()
{
    return GrafPtr(mPluginPort);
}

void MRJContext::localToFrame(Point* pt)
{
    if (mPluginWindow != NULL) {
        // transform mouse to frame coordinates.
        nsPluginPort* npPort = mPluginWindow->window;
        pt->v += npPort->porty;
        pt->h += npPort->portx;
    }
}

void MRJContext::ensureValidPort()
{
    if (mPluginWindow != NULL) {
        nsPluginPort* npPort = mPluginWindow->window;
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

void MRJContext::synchronizeVisibility()
{
    OSStatus status;
    
    if (mAppletControl) {
        JNIEnv* env = mSession->getCurrentEnv();
        if (mPluginWindow != NULL) {
            nsPluginRect oldClipRect = mCachedClipRect;
            nsPluginPort* pluginPort = mPluginWindow->window;
            mCachedOrigin.x = pluginPort->portx;
            mCachedOrigin.y = pluginPort->porty;
            mCachedClipRect = mPluginWindow->clipRect;

            int posX = -pluginPort->portx;
            int posY = -pluginPort->porty;
            int clipX = mCachedClipRect.left;
            int clipY = mCachedClipRect.top;
            int clipWidth = (mCachedClipRect.right - mCachedClipRect.left);
            int clipHeight = (mCachedClipRect.bottom - mCachedClipRect.top);

#if TARGET_RT_MAC_MACHO
            // XXX hack a little bit to make it work...
            const SInt32 kTitleBarHeight = 22;
            posY -= kTitleBarHeight;
            clipY -= kTitleBarHeight;
#endif

            status = ::SizeJavaControl(env, mAppletControl, mPluginWindow->width, mPluginWindow->height);
            status = ::MoveAndClipJavaControl(env, mAppletControl, posX, posY,
                                              clipX, clipY, clipWidth, clipHeight);

            if (!mIsVisible) {
                status = ::ShowHideJavaControl(env, mAppletControl, true);
                mIsVisible = (status == noErr);
            } else {
                status = ::DrawJavaControl(env, mAppletControl);
            }

#if TARGET_RT_MAC_MACHO && 0
            if (GetCurrentKeyModifiers() & alphaLock) {
                // raise(SIGINT);
                printf("@@@ (posX = %d, posY = %d), (clipX = %d, clipY = %d, clipWidth = %d, clipHeight = %d) @@@\n",
                    posX, posY, clipX, clipY, clipWidth, clipHeight);
    
                Rect clipBounds;
                clipBounds.left = clipX, clipBounds.top = clipY;
                clipBounds.right = clipX + clipWidth, clipBounds.bottom = clipY + clipHeight;
                LocalPort port(mPluginPort);
                port.Enter();
                    FrameRect(&clipBounds);
                    InvertRect(&clipBounds);
                    QDFlushPortBuffer(mPluginPort, NULL);
                port.Exit();
            }
#endif
        } else {
           status = ::MoveAndClipJavaControl(env, mAppletControl, 0, 0, 0, 0, 0, 0);
           status = ::ShowHideJavaControl(env, mAppletControl, false);
           mIsVisible = false;
        }
    }
}

void MRJContext::showFrames()
{
#if TARGET_CARBON
    // Won't Carbon events handle all of this?
#else           
    UInt32 frameCount;
    OSStatus status = ::JMCountAWTContextFrames(mContext, &frameCount);
    if (status == noErr) {
        for (UInt32 frameIndex = 0; frameIndex < frameCount; frameIndex++) {
            JMFrameRef frameRef;
            status = ::JMGetAWTContextFrame(mContext, frameIndex, &frameRef);
            if (status == noErr) {
                ::JMFrameShowHide(frameRef, true);
//              MRJFrame* frame = getFrame(frameRef);
//              if (frame != NULL)
//                  frame->focusEvent(false);
            }
        }
    }
#endif
}

void MRJContext::hideFrames()
{
#if TARGET_CARBON
    // Won't Carbon events handle all of this?
#else           
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
#endif
}

/**
 * Ensure that any frames Java still has a reference to are no longer valid, so that we won't crash
 * after a plugin instance gets shut down. This is called by the destructor just in case, to avoid
 * some hard freeze crashes I've seen.
 */
void MRJContext::releaseFrames()
{
#if TARGET_CARBON
    // Won't Carbon events handle all of this?
#else           
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
#endif
}

void MRJContext::setDocumentBase(const char* documentBase)
{
    if (mDocumentBase != NULL)
        mDocumentBase = NULL;
    mDocumentBase = ::strdup(documentBase); 
}

const char* MRJContext::getDocumentBase()
{
    return mDocumentBase;
}

void MRJContext::setAppletHTML(const char* appletHTML, nsPluginTagType tagType)
{
    if (mAppletHTML != NULL)
        delete[] mAppletHTML;
        
    switch (tagType) {
    case nsPluginTagType_Applet:
        mAppletHTML = ::strdup(appletHTML);
        break;
    
    case nsPluginTagType_Object:
        {
            // If the HTML isn't an <applet> element, but is an <object> element, then
            // transform it so MRJ can deal with it gracefully. it sure would be
            // nice if some DOM code would deal with this for us. This code
            // is fragile, because it assumes the case of the classid attribute.

            // edit the <object> element, converting <object> to <applet>,
            // classid="java:JitterText.class" to code="JitterText.class",
            // and </object> to </applet>.
            string element(appletHTML);
            
            const char kAppletTag[] = "applet";
            const size_t kAppleTagSize = sizeof(kAppletTag) - 1;
            string::size_type startTag = element.find("<object");
            if (startTag != string::npos) {
                element.replace(startTag + 1, kAppleTagSize, kAppletTag);
            }
            
            string::size_type endTag = element.rfind("</object>");
            if (endTag != string::npos) {
                element.replace(endTag + 2, kAppleTagSize, kAppletTag);
            }

            const char kClassIDAttribute[] = "classid=\"java:";
            const char    kCodeAttribute[] = "code=\"";
            size_t kClassIDAttributeSize = sizeof(kClassIDAttribute) - 1;
            string::size_type classID = element.find(kClassIDAttribute);
            if (classID != string::npos) {
                element.replace(classID, kClassIDAttributeSize, kCodeAttribute);
            }
            
            mAppletHTML = ::strdup(element.c_str());
        }
        break;
    case nsPluginTagType_Embed:
        {
            nsIPluginTagInfo* tagInfo = NULL;
            if (mPeer->QueryInterface(NS_GET_IID(nsIPluginTagInfo), (void**)&tagInfo) == NS_OK) {
                // just synthesize an <APPLET> element out of whole cloth.
                mAppletHTML = synthesizeAppletElement(tagInfo);
                NS_RELEASE(tagInfo);
            }
        }
        break;
    }
}

const char* MRJContext::getAppletHTML()
{
    return mAppletHTML;
}

void MRJContext::setSecurityContext(MRJSecurityContext* context)
{
    NS_ADDREF(context);
    NS_IF_RELEASE(mSecurityContext);
    mSecurityContext = context;
}

MRJSecurityContext* MRJContext::getSecurityContext()
{
    return mSecurityContext;
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

#if TARGET_CARBON

struct EmptyPort {
    CGrafPtr mPort;
    
    EmptyPort() : mPort(NULL)
    {
        GrafPtr oldPort;
        ::GetPort(&oldPort);
        mPort = ::CreateNewPort();
        if (mPort) {
            ::SetPort(mPort);
            ::PortSize(0, 0);
        }
        ::SetPort(oldPort);
    }
    
    ~EmptyPort()
    {
        if (mPort)
            ::DisposePort(mPort);
    }

    operator CGrafPtr() { return mPort; }
};

#else

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

    operator CGrafPtr() { return this; }
};

#endif

CGrafPtr MRJContext::getEmptyPort()
{
    static EmptyPort emptyPort;
    return emptyPort;
}
