/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
	MRJContext.cp
	
	Manages Java content using the MacOS Runtime for Java.
	
	by Patrick C. Beard.
 */

#include <string.h>
#include <stdlib.h>
#include <Appearance.h>
#include <Gestalt.h>
#include <TextCommon.h>

#include "MRJSession.h"
#include "MRJContext.h"
#include "MRJPlugin.h"
#include "MRJPage.h"
#include "MRJMonitor.h"
#include "AsyncMessage.h"
#include "TopLevelFrame.h"
#include "EmbeddedFrame.h"
#include "LocalPort.h"
#include "StringUtils.h"

#include "nsIPluginManager2.h"
#include "nsIPluginInstancePeer.h"
#include "nsIJVMPluginTagInfo.h"
#include "MRJSecurityContext.h"

#include <string>

using namespace std;

extern nsIPluginManager* thePluginManager;
extern nsIPluginManager2* thePluginManager2;

static OSStatus JMTextToStr255(JMTextRef textRef, Str255 str);
static char* JMTextToEncoding(JMTextRef textRef, JMTextEncoding encoding);

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

static RgnHandle NewEmptyRgn()
{
    RgnHandle region = ::NewRgn();
    if (region != NULL) ::SetEmptyRgn(region);
    return region;
}

MRJContext::MRJContext(MRJSession* session, MRJPluginInstance* instance)
	:	mPluginInstance(instance), mSession(session), mSessionRef(session->getSessionRef()), mPeer(NULL),
		mLocator(NULL), mContext(NULL), mViewer(NULL), mViewerFrame(NULL), mIsActive(false),
		mPluginWindow(NULL), mPluginClipping(NULL), mPluginPort(NULL),
		mDocumentBase(NULL), mAppletHTML(NULL), mPage(NULL), mSecurityContext(NULL)
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
	if (tagInfo->QueryInterface(NS_GET_IID(nsIPluginTagInfo2), &tagInfo2) == NS_OK) {
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

#if 1

static void fetchCompleted(JMAppletLocatorRef ref, JMLocatorErrors status) {}

void MRJContext::processAppletTag()
{
    // use the applet's HTML element to create a locator. this is required
    // in general, to specify a separate CODEBASE.

	nsIPluginTagInfo* tagInfo = NULL;
	if (mPeer->QueryInterface(NS_GET_IID(nsIPluginTagInfo), &tagInfo) == NS_OK) {
		nsIPluginTagInfo2* tagInfo2 = NULL;
		if (tagInfo->QueryInterface(NS_GET_IID(nsIPluginTagInfo2), &tagInfo2) == NS_OK) {
    		nsPluginTagType tagType = nsPluginTagType_Unknown;
    		if (tagInfo2->GetTagType(&tagType) == NS_OK) {
    			// get the URL of the HTML document	containing the applet, and the
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
			if (mPeer->QueryInterface(NS_GET_IID(nsIJVMPluginTagInfo), &jvmTagInfo) == NS_OK) {
				PRUint32 documentID;
				const char* codeBase;
				const char* archive;
				PRBool mayScript;
				if (tagInfo2->GetUniqueID(&documentID) != NS_OK) documentID = 0;
				if (jvmTagInfo->GetCodeBase(&codeBase) != NS_OK) codeBase = NULL;
				if (jvmTagInfo->GetArchive(&archive) != NS_OK) archive = NULL;
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

    	static JMAppletLocatorCallbacks callbacks = {
    		kJMVersion,						/* should be set to kJMVersion */
    		&fetchCompleted,				/* called when the html has been completely fetched */
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
    }
}

#else

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
	nsPluginTagType pluginTagType = nsPluginTagType_Embed;

	// obtain the applet's attributes & parameters.
	nsIPluginTagInfo* tagInfo = NULL;
	if (mPeer->QueryInterface(NS_GET_IID(nsIPluginTagInfo), &tagInfo) == NS_OK) {
		tagInfo->GetAttributes(attributes.fCount, attributes.fNames, attributes.fValues);
		nsIPluginTagInfo2* tagInfo2 = NULL;
		if (tagInfo->QueryInterface(NS_GET_IID(nsIPluginTagInfo2), &tagInfo2) == NS_OK) {
			tagInfo2->GetParameters(parameters.fCount, parameters.fNames, parameters.fValues);
			tagInfo2->GetTagType(&pluginTagType);

			// get the URL of the HTML document	containing the applet.
			const char* documentBase = NULL;
			if (tagInfo2->GetDocumentBase(&documentBase) == NS_OK && documentBase != NULL) {
				// record the document base, in case 
				setDocumentBase(documentBase);

				// establish a page context for this applet to run in.
				nsIJVMPluginTagInfo* jvmTagInfo = NULL;
				if (mPeer->QueryInterface(NS_GET_IID(nsIJVMPluginTagInfo), &jvmTagInfo) == NS_OK) {
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
			// if running as an EMBED tag plugin, there may be some attribute
			// rewriting going on. Sun's Java plugin uses attributes with the
			// "java_" prefix.
			if (pluginTagType == nsPluginTagType_Embed) {
				static const char kJavaPluginAttributePrefix[] = "java_";
				if (strncmp(name, kJavaPluginAttributePrefix, sizeof(kJavaPluginAttributePrefix) - 1) == 0)
					name += (sizeof(kJavaPluginAttributePrefix) - 1);
			}
			if (strcasecmp(name, "CODEBASE") == 0) {
				if (pageAttributes.codeBase == NULL)
					pageAttributes.codeBase = value;
			} else
			if (strcasecmp(name, "CODE") == 0) {
				status = ::JMNewTextRef(mSessionRef, &info.fAppletCode, kTextEncodingMacRoman, value, strlen(value));
			} else
			if (strcasecmp(name, "CLASSID") == 0) {
				// bug 6591: <object> uses classid="java:classname"
				value += 5;
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
		// setCodeBase(baseURL);
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

#endif

static MRJFrame* getFrame(JMFrameRef ref)
{
	MRJFrame* frame = NULL;
	if (ref != NULL) {
		::JMGetFrameData(ref, (JMClientData*)&frame);
	}
	
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

static char* JMTextToEncoding(JMTextRef textRef, JMTextEncoding encoding)
{
	UInt32 length = 0;
    OSStatus status = ::JMGetTextLengthInBytes(textRef, encoding, &length);
    if (status != noErr)
        return NULL;
    char* text = new char[length + 1];
    if (text != NULL) {
        UInt32 actualLength;
    	status = ::JMGetTextBytes(textRef, encoding, text, length, &actualLength);
	    if (status != noErr) {
	        delete text;
	        return NULL;
	    }
	    text[length] = '\0';
    }
    return text;
}

void MRJContext::exceptionOccurred(JMAWTContextRef context, JMTextRef exceptionName, JMTextRef exceptionMsg, JMTextRef stackTrace)
{
	// why not display this using the Appearance Manager's wizzy new alert?
	if (appearanceManagerExists()) {
		OSStatus status;
		Str255 error, explanation;
		status = ::JMTextToStr255(exceptionName, error);
		status = ::JMTextToStr255(exceptionMsg, explanation);

#if 0
        TextEncoding utf8 = CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicodeUTF8Format);
		char* where = ::JMTextToEncoding(stackTrace, utf8);
        if (where != NULL)
            delete[] where;
#endif
		
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

JMAppletViewerRef MRJContext::getViewerRef()
{
    return mViewer;
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

Boolean MRJContext::appletLoaded()
{
	return (mViewer != NULL);
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
	OSStatus status;
	
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

void MRJContext::printApplet(nsPluginWindow* printingWindow)
{
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
//    printf("mrjcontext::resume\n");
    
	if (mViewerFrame != NULL) {
		::JMFrameResume(mViewerFrame, inFront);
	}
}

void MRJContext::click(const EventRecord* event, MRJFrame* appletFrame)
{
	// inspectWindow();
//    printf("mrjcontext::click\n");

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
//    printf("mrjcontext::idle\n");

	// Put the port in to proper window coordinates.
	nsPluginPort* npPort = mPluginWindow->window;
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
	if (mContext != NULL) {
    	if (pluginWindow != NULL) {
    		if (pluginWindow->height != 0 && pluginWindow->width != 0) {
    			mPluginWindow = pluginWindow;

    			// establish the GrafPort the plugin will draw in.
    			mPluginPort = pluginWindow->window->port;

    			if (! appletLoaded())
    				loadApplet();
    		}
    	} else {
    		// tell MRJ the window has gone away.
    		mPluginWindow = NULL;
    		
    		// use a single, 0x0, empty port for all future drawing.
    		mPluginPort = getEmptyPort();
    	}
    	synchronizeClipping();
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

	// synchronizeVisibility();
	
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
		nsPluginPort* npPort = mPluginWindow->window;
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
	// always update the cached information.
	if (mViewerFrame != NULL) {
    	if (mPluginWindow != NULL) {
            nsPluginRect oldClipRect = mCachedClipRect;
            nsPluginPort* pluginPort = mPluginWindow->window;
    	    mCachedOrigin.x = pluginPort->portx;
    	    mCachedOrigin.y = pluginPort->porty;
    	    mCachedClipRect = mPluginWindow->clipRect;
    		
    		// compute the frame's origin and clipping.
    		
    		// JManager wants the origin expressed in window coordinates.
    		// npWindow refers to the entire mozilla view port whereas the nport
    		// refers to the actual rendered html window.
    		Point frameOrigin = { -pluginPort->porty, -pluginPort->portx };
    		GrafPtr framePort = (GrafPtr)mPluginPort;
    		OSStatus status = ::JMSetFrameVisibility(mViewerFrame, framePort,
    												 frameOrigin, mPluginClipping);

            // Invalidate the old clip rectangle, so that any bogus drawing that may
            // occurred at the old location, will be corrected.
        	LocalPort port(framePort);
            port.Enter();
        	::InvalRect((Rect*)&oldClipRect);
        	::InvalRect((Rect*)&mCachedClipRect);
        	port.Exit();
        } else {
            Point frameOrigin = { 0, 0 };
    		OSStatus status = ::JMSetFrameVisibility(mViewerFrame, GrafPtr(mPluginPort),
    												 frameOrigin, mPluginClipping);
        }
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
    	    if (mPeer->QueryInterface(NS_GET_IID(nsIPluginTagInfo), &tagInfo) == NS_OK) {
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
