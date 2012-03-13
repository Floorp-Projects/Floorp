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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

#ifndef _USE_CPVE

#include "CC_Common.h"

#ifdef LINUX
#include "X11/Xlib.h"
#endif

#include "WebrtcMediaProvider.h"
#include "WebrtcAudioProvider.h"
#ifndef NO_WEBRTC_VIDEO
#include "WebrtcVideoProvider.h"
#endif
#include "WebrtcLogging.h"

using namespace std;

#if (defined (_MSC_VER) && defined (_DEBUG))
/**
 * Workaround to allow the use of mixed debug & release code
 * since the Webrtc library is release only, but we still want debug projects
 * to test our own stuff. This symbol is missing from the debug crt library.
 */
extern "C" __declspec(dllexport) void __cdecl _invalid_parameter_noinfo(void) {  }
#endif /* (defined (_MSC_VER) && defined (_DEBUG)) */

bool g_IncludeWebrtcLogging=false;

static const char* logTag = "WebrtcMediaProvider";

namespace CSF {

// Public interface class

MediaProvider::~MediaProvider()
{
}

// static
MediaProvider * MediaProvider::create( )
{
    LOG_WEBRTC_DEBUG( logTag, "MediaProvider::create");

    WebrtcMediaProvider* mediaProvider = new WebrtcMediaProvider();
    LOG_WEBRTC_DEBUG( logTag, "MediaProvider::create new instance");

    if ( mediaProvider->init() != 0)
    {
        LOG_WEBRTC_ERROR( logTag, "cannot initialize WebrtcMediaProvider");
        delete mediaProvider;
        return NULL;
    }

    return mediaProvider;
}

// Implementation classes

WebrtcMediaProvider::WebrtcMediaProvider( )
{
    pAudio = new WebrtcAudioProvider( this );
    if (pAudio->init() != 0)
    {
        LOG_WEBRTC_ERROR( logTag, "Error calling pAudio->Init in WebrtcMediaProvider");
    }

    // for the moment, we only have video lib for windows
#ifndef NO_WEBRTC_VIDEO
    pVideo = new WebrtcVideoProvider( this );
#else
    pVideo = NULL;
#endif
}

WebrtcMediaProvider::~WebrtcMediaProvider()
{
#ifndef NO_WEBRTC_VIDEO
    delete pVideo;
#endif
    delete pAudio;
}

int WebrtcMediaProvider::init()
{
#ifndef NO_WEBRTC_VIDEO
    if (pVideo->init() != 0)
    {
        return -1;
    }
#endif

    return 0;
}

void WebrtcMediaProvider::shutdown()
{
}

void WebrtcMediaProvider::addMediaProviderObserver( MediaProviderObserver* observer )
{
    // just add all the events at once
    /*
    componentImpl->addObserver( eVideoModeChanged, observer);
    componentImpl->addObserver( eKeyFrameRequest, observer);
    componentImpl->addObserver( eMediaLost, observer);
    componentImpl->addObserver( eMediaRestored, observer);
    */
}

/*
void WebrtcMediaProvider::dispatchEvent(Event event)
{
    ComponentImpl::ObserverMapRange range = componentImpl->observerMapRangeForEventID(event.id);

    for (ComponentImpl::ObserverMapIterator it = range.first; it != range.second; it++)
    {
        switch (event.id)
        {
        case eVideoModeChanged:
            LOG_Webrtc_DEBUG( logTag, "Dispatching eVideoMode");
            ((MediaProviderObserver*)(*it).second)->onVideoModeChanged( event.context != 0 );
            break;

        case eKeyFrameRequest:
            LOG_Webrtc_DEBUG( logTag, "Dispatching eKeyFrameRequest");
            ((MediaProviderObserver*)(*it).second)->onKeyFrameRequested( (long)event.context );
            break;

        case eMediaLost:
            LOG_Webrtc_DEBUG( logTag, "Dispatching eMediaLost");
            ((MediaProviderObserver*)(*it).second)->onMediaLost( (long)event.context );
            break;

        case eMediaRestored:
            LOG_Webrtc_DEBUG( logTag, "Dispatching eMediaRestored");
            ((MediaProviderObserver*)(*it).second)->onMediaRestored( (long)event.context );
            break;

        default:
            CSFAssert(false, "Bad event id passed to WebrtcMediaProvider: %d", event.id);
            break;
        }
    }
}

CSFComponentRole WebrtcMediaProvider::getRole() const
{
    return kCSFMediaProvider_ComponentRole;
}

*/

AudioControl* WebrtcMediaProvider::getAudioControl()
{
    return pAudio ? pAudio->getAudioControl() : NULL;
}

VideoControl* WebrtcMediaProvider::getVideoControl()
{
#ifndef NO_WEBRTC_VIDEO
    return pVideo ? pVideo->getMediaControl() : NULL;
#else
    return NULL;
#endif
}

webrtc::VoiceEngine* WebrtcMediaProvider::getWebrtcVoiceEngine () {
    return ((pAudio != NULL) ? pAudio->getVoiceEngine() : NULL);
}

AudioTermination* WebrtcMediaProvider::getAudioTermination()
{
    return pAudio ? pAudio->getAudioTermination() : NULL;
}

VideoTermination* WebrtcMediaProvider::getVideoTermination()
{
#ifndef NO_WEBRTC_VIDEO
    return pVideo ? pVideo->getMediaTermination() : NULL;
#else
    return NULL;
#endif
}

bool WebrtcMediaProvider::getKey(
    const unsigned char* masterKey, 
    int masterKeyLen, 
    const unsigned char* masterSalt, 
    int masterSaltLen,
    unsigned char* key,
    unsigned int keyLen
    )
{
    LOG_WEBRTC_DEBUG( logTag, "getKey() masterKeyLen = %d, masterSaltLen = %d", masterKeyLen, masterSaltLen);

    if(masterKey == NULL || masterSalt == NULL)
    {
        LOG_WEBRTC_ERROR( logTag, "getKey() masterKey or masterSalt is NULL");
        return false;
    }

    if((masterKeyLen != WEBRTC_MASTER_KEY_LENGTH) || (masterSaltLen != WEBRTC_MASTER_SALT_LENGTH))
    {
        LOG_WEBRTC_ERROR( logTag, "getKey() invalid masterKeyLen or masterSaltLen length");
        return false;
    }

    if((key == NULL) || (keyLen != WEBRTC_KEY_LENGTH))
    {
        LOG_WEBRTC_ERROR( logTag, "getKey() invalid key or keyLen");
        return false;
    }

    memcpy(key, masterKey, WEBRTC_MASTER_KEY_LENGTH);
    memcpy(key + WEBRTC_MASTER_KEY_LENGTH, masterSalt, WEBRTC_MASTER_SALT_LENGTH);

    return true;
}

} // namespace CSF

#endif
