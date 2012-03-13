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
#ifndef NO_WEBRTC_VIDEO

#include "CC_Common.h"
#ifdef LINUX
#include "X11/Xlib.h"
#elif defined __APPLE__
#include <Carbon/Carbon.h>
#endif
#include "VcmSIPCCBinding.h"
#include "WebrtcMediaProvider.h"
#include "WebrtcAudioProvider.h"
#include "WebrtcVideoProvider.h"
#include "WebrtcLogging.h"
#include "vie_encryption.h"

#include "base/lock.h"

using namespace std;
#include "string.h"

extern "C" void config_get_value (int id, void *buffer, int length);

static const char* logTag = "WebrtcVideoProvider";

#ifdef LINUX
static    clock_t currentTime, lastRequestTime;
#endif

namespace CSF {
//H264 may not be possible now ..
static webrtc::VideoCodec videoCodec;

WebrtcVideoProvider::WebrtcVideoProvider( WebrtcMediaProvider* provider )
: provider(provider),
  vieVideo(NULL),
  vieBase(NULL),
  vieCapture(NULL),
  vieRender(NULL),
  vieNetwork(NULL),
  vieEncryption(NULL),
  vieRtpRtcp(NULL),
  videoMode(true), 
  startPort(1024), 
  endPort(65535), 
  localRenderId(0),
  webCaptureId(0),
  vp8Idx(0),
  previewWindow(NULL), 
  DSCPValue(0)
{
	int error = 0;
    vieVideo = webrtc::VideoEngine::Create();
	if(vieVideo == NULL)
	{
		LOG_WEBRTC_ERROR( logTag, "WebrtcVideoProvider(): VieVideoEngine::Create() failed");
		return;
	}
//#ifdef ENABLE_WEBRTC_VIDEO_TRACE
	vieVideo->SetTraceFilter(webrtc::kTraceAll);
    vieVideo->SetTraceFile( "Vievideotrace.out" );
//#endif
	
	vieBase = webrtc::ViEBase::GetInterface(vieVideo);
	if(vieBase == NULL)
	{
		LOG_WEBRTC_ERROR( logTag, "WebrtcVideoProvider(): VieBase::GetInterface() failed");
		return;
	}
	
    if(vieBase->Init() == -1)
	{
		LOG_WEBRTC_ERROR( logTag, "WebrtcVideoProvider(): VieBase::Init() failed");
		return;
	}
	//set the voice engine
	error = vieBase->SetVoiceEngine(provider->getWebrtcVoiceEngine());
	if(error == -1)
	{
		LOG_WEBRTC_ERROR( logTag, "WebrtcVideoProvider(): Setting Voice Engine Failed ");
		return;

	}
	
	vieCodec = webrtc::ViECodec::GetInterface( vieVideo );
	if(vieCodec == NULL) 
	{
		LOG_WEBRTC_ERROR( logTag, "WebrtcVideoProvider(): VieCodec::GetInterface() failed");
		return;
	}
		
	vieRender = webrtc::ViERender::GetInterface(vieVideo);
	if(vieRender == NULL)
	{	
		LOG_WEBRTC_ERROR( logTag, "WebrtcVideoProvider(): VieRender::GetInterface() failed");
		return;
	}
	
	vieNetwork = webrtc::ViENetwork::GetInterface(vieVideo);
	if(vieNetwork == NULL)
	{
		LOG_WEBRTC_ERROR( logTag, "WebrtcVideoProvider(): VieNetwork::GetInterface() failed");
		return;
	}

	vieCapture = webrtc::ViECapture::GetInterface(vieVideo);
	if(vieCapture == NULL)
	{
		LOG_WEBRTC_ERROR( logTag, "WebrtcVideoProvider(): VieCapture::GetInterface() failed");
		return;
	}

	 vieRtpRtcp = webrtc::ViERTP_RTCP::GetInterface(vieVideo);
	
	
	memset(&videoCodec, 0 , sizeof(webrtc::VideoCodec));
    int nCodecs = vieCodec->NumberOfCodecs();
	int codecIdx = 0;
    for ( codecIdx = 0; codecIdx < nCodecs; codecIdx++ )
    {
        error = vieCodec->GetCodec( codecIdx, videoCodec);
		if (error == -1)
		{
        	LOG_WEBRTC_ERROR( logTag, " VieCodec:GetCodec Filed Error: %d ", vieBase->LastError());
			return;
		}
		if( strcmp(videoCodec.plName, "VP8") == 0 )
		{
			//defaulting to VP8 for now
			vp8Idx = codecIdx;
		}	
        LOG_WEBRTC_INFO( logTag, "codec @ %d %s pltype %d ", codecIdx,  
						           videoCodec.plName, videoCodec.plType );
		LOG_WEBRTC_INFO( logTag, "startBitrate is %d", videoCodec.startBitrate );
		LOG_WEBRTC_INFO( logTag, "maxBitrate is %d", videoCodec.maxBitrate );
		LOG_WEBRTC_INFO( logTag, "minBitrate is %d", videoCodec.minBitrate );
		LOG_WEBRTC_INFO( logTag, "maxFramerate is %d", videoCodec.maxFramerate );
		LOG_WEBRTC_INFO( logTag, "width is %d", videoCodec.width );
		LOG_WEBRTC_INFO( logTag, "height is %d", videoCodec.height );
            //break;
    }
#ifdef LINUX
    currentTime=lastRequestTime=clock();
#endif

	const unsigned int kMaxDeviceNameLength = 128;
	const unsigned int kMaxUniqueIdLength = 256;
    char deviceName[kMaxDeviceNameLength];
	char uniqueId[kMaxUniqueIdLength];
    memset (deviceName,0,128);
	memset(uniqueId,0,256);
	int captureIdx = 0;
	for(captureIdx = 0; 
		captureIdx < vieCapture->NumberOfCaptureDevices();
		captureIdx++)
	{
    	memset (deviceName,0,128);
		memset(uniqueId,0,256);
		error = vieCapture->GetCaptureDevice(captureIdx,deviceName,
											kMaxDeviceNameLength, uniqueId,
											kMaxUniqueIdLength);
		if(error == -1)
		{
			LOG_WEBRTC_ERROR(logTag," VieCapture:GetCaptureDevice: Failed %d", 
										 vieBase->LastError() );
			return;
		}	
		LOG_WEBRTC_DEBUG(logTag,"  Capture Device Index %d, Name %s",
									captureIdx, deviceName);
	}
	
	webCaptureId = 0;
   	memset (deviceName,0,128);
	memset(uniqueId,0,256);

    if ( vieCapture->GetCaptureDevice( 0, deviceName,kMaxDeviceNameLength, 
										uniqueId, kMaxUniqueIdLength ) == 0 )
    {
        LOG_WEBRTC_DEBUG( logTag, "Using capture device %s", deviceName);
        error = vieCapture->AllocateCaptureDevice( uniqueId,
										  kMaxUniqueIdLength, webCaptureId);
		if(error == -1)
		{
			LOG_WEBRTC_DEBUG(logTag,"  VieCapture: Allocate Capture Device Failed");
			return;
		}
		LOG_WEBRTC_DEBUG( logTag, "Capture Id to use is %d", webCaptureId);
		webrtc::CaptureCapability cap;
		int numCaps = vieCapture->NumberOfCapabilities(
													   uniqueId,kMaxUniqueIdLength);
		LOG_WEBRTC_DEBUG( logTag, "Number of Capabilities %d", numCaps);
        for ( int j = 0; j<numCaps; j++ )
        {
            if ( vieCapture->GetCaptureCapability(uniqueId,kMaxUniqueIdLength, j, cap ) != 0 ) break;
            LOG_WEBRTC_DEBUG( logTag, "type=%d width=%d height=%d maxFPS=%d", cap.rawType, cap.width, cap.height, cap.maxFPS );
        }
    }
    else
    {
        LOG_WEBRTC_WARN( logTag, "No camera found");
    }
	

}

int WebrtcVideoProvider::init()
{
	webrtc::VoiceEngine* voiceEngine = provider->getWebrtcVoiceEngine();
    if(voiceEngine)
    {
        vieEncryption = webrtc::ViEEncryption::GetInterface(vieVideo);
        if (vieEncryption == NULL)
        {
            LOG_WEBRTC_ERROR( logTag, "WebrtcVideoProvider(): VoEEncryption::GetInterface failed");
            return -1;
        }  
    }
    else
    {
        LOG_WEBRTC_ERROR( logTag, "WebrtcVideoProvider(): No VoE voice engine");
        return -1;
    }

    return 0;
}

WebrtcVideoProvider::~WebrtcVideoProvider()
{
    if(vieEncryption)
    {
        vieEncryption->Release();
        vieEncryption = NULL;
    }
	vieCapture->StopCapture(webCaptureId);
	vieCapture->ReleaseCaptureDevice(webCaptureId);
	vieCapture->Release();
	vieRender->Release();
	vieBase->Release();
	webrtc::VideoEngine::Delete(vieVideo);
}

void WebrtcVideoProvider::setVideoMode( bool enable )
{
    videoMode = enable;
}

void WebrtcVideoProvider::setRenderWindow( int streamId, WebrtcPlatformWindow window )
{
	LOG_WEBRTC_DEBUG( logTag, "setRenderWindow: streamId= %d, window=%p", streamId, window);
	AutoLock lock(streamMapMutex);
	// we always want to erase the old one. If window is non-null, it will be replaced,
	// otherwise passing in a null window value to this function leads to no mapping for this stream.
	streamIdToWindow.erase( streamId );
    if ( window != NULL )
    {
    	streamIdToWindow.insert( std::make_pair(streamId, RenderWindow( window)) );
    	// now that the window is in the map for this stream, refresh the stream with the window
    	setRenderWindowForStreamIdFromMap(streamId);
    	// the assumption is that the rendering may have been stopped when the previous window was destroyed.
    	//gipsVideo->GIPSVideo_StartRender( getChannelForStreamId( streamId ) );
    }
    else
    {
    	// for a null window we want to stop rendering for a given channel
    	vieRender->StopRender( getChannelForStreamId( streamId ) );
    }
	LOG_WEBRTC_DEBUG( logTag, "setRenderWindow: Exiting successfully");
   
}

const WebrtcVideoProvider::RenderWindow* WebrtcVideoProvider::getRenderWindow( int streamId )
{
	//AutoLock lock(streamMapMutex);
    std::map<int, RenderWindow>::const_iterator it = streamIdToWindow.find( streamId );
    return ( it != streamIdToWindow.end() ) ? &it->second : NULL;
}

void WebrtcVideoProvider::setPreviewWindow( void* window, int top, int left, int bottom, int right, RenderScaling style )
{
	AutoLock lock(m_lock);
	if(this->previewWindow != NULL)
	{
		//this is the local renderer
		vieRender->AddRenderer(webCaptureId,NULL,0,0.0f, 0.0f, 1.0f, 1.0f);
		delete this->previewWindow;
		this->previewWindow = NULL;
	}

	if(window != NULL)
	{
		// Set new window.
		LOG_WEBRTC_DEBUG(logTag, " Preview window: Adding Renderer ");
#ifdef WIN32
		//setRenderWindow( 0, (GipsPlatformWindow)window, top, left, bottom, right, style );
		vieRender->AddRenderer(webCaptureId, (HWND)window, 1, 0.0f, 0.0f, 1.0f, 1.0f );
#elif LINUX
		vieRender->AddRenderer(webCaptureId, (Window*)window,1, 0.0f, 0.0f, 1.0f, 1.0f );
#elif __APPLE__
		LOG_WEBRTC_DEBUG(logTag, " Preview window: Adding Renderer ");
		vieRender->AddRenderer(webCaptureId, window,0, 0.0f, 0.0f, 1.0f, 1.0f );
		vieRender->StartRender(webCaptureId);	
#endif
		this->previewWindow = new RenderWindow( (WebrtcPlatformWindow)window);
	}
}

void WebrtcVideoProvider::setRemoteWindow( int streamId, VideoWindowHandle window)
{
	AutoLock lock(m_lock);
    setRenderWindow( streamId, (WebrtcPlatformWindow)window);
}

int WebrtcVideoProvider::setExternalRenderer(int streamId, VideoFormat videoFormat,
												ExternalRendererHandle renderer)
{
	AutoLock lock(m_lock);
	int error = 0;
	LOG_WEBRTC_DEBUG( logTag, "WebrtcVideoProvider:: SetExternalRenderer , streamId: %d", streamId);
    int channel = getChannelForStreamId( streamId );
	if(channel != -1)
	{
		
		error = vieRender->AddRenderer(channel, (webrtc::RawVideoType)videoFormat, (webrtc::ExternalRenderer*) renderer); 
		if(error == -1)
		{
			LOG_WEBRTC_DEBUG( logTag, "WebrtcVideoProvider:: SetExternalRenderer: Failed: Error %d ", vieBase->LastError() );
		}
		return error;
	}	
	return -1;	
}

std::vector<std::string> WebrtcVideoProvider::getCaptureDevices()
{
	AutoLock lock(m_lock);
	const unsigned int kMaxDeviceNameLength = 128;
	const unsigned int kMaxUniqueIdLength = 256;
    char name[kMaxDeviceNameLength];
	char uniqueId[kMaxUniqueIdLength];
    std::vector<std::string> deviceList;
	LOG_WEBRTC_DEBUG(logTag," WebrtcVideoProvider: getCaptureDevices() ");
    // '100' is an arbitrary maximum, to defend against an endless loop;
    // in practice, GetCaptureDevice() should return -1 well before that
    for ( int i = 0; i < 100; i++ )
    {	
        if ( vieCapture->GetCaptureDevice( 0, name,kMaxDeviceNameLength, uniqueId, kMaxUniqueIdLength ) != 0 ) break;
        deviceList.push_back( name );
    }
    return deviceList;
}

bool WebrtcVideoProvider::setCaptureDevice( const std::string& name )
{
	LOG_WEBRTC_DEBUG(logTag," WebrtcVideoProvider: setCaptureDevice(): %s",name.c_str());
	
	AutoLock lock(m_lock);
	int error = 0;
	const int kMaxDeviceNameLength = 128;
	const int kMaxUniqueIdLength = 256;
	char deviceName[kMaxDeviceNameLength];
	char uniqueId[kMaxUniqueIdLength];
	
	
	int captureIdx = 0;
	for(captureIdx = 0;
		captureIdx < vieCapture->NumberOfCaptureDevices();
		captureIdx++)
	{
		memset (deviceName,0,128);
		memset(uniqueId,0,256);
		if (vieCapture->GetCaptureDevice( captureIdx, deviceName,kMaxDeviceNameLength, 
										 uniqueId, kMaxUniqueIdLength ) == 0 )
		{
			if(strcmp(name.c_str(),deviceName) == 0)
			{
				webCaptureId = -1; //nullify
				error = vieCapture->AllocateCaptureDevice( uniqueId,
														  kMaxUniqueIdLength, webCaptureId);
				if(error == -1)
				{
					LOG_WEBRTC_DEBUG(logTag,"  VieCapture: Allocate Capture Device Failed");
					return false;
				}
				break;
			}
		}
		else
		{
			LOG_WEBRTC_WARN( logTag, "No camera found");
			return false;
		}
		
	}
	
    return true;
	
}

int WebrtcVideoProvider::getCodecList( CodecRequestType requestType )
{
	AutoLock lock(m_lock);
    return VideoCodecMask_H264;
}

WebrtcVideoStreamPtr WebrtcVideoProvider::getStreamByChannel( int channel )
{
	//AutoLock lock(streamMapMutex);
    for( std::map<int, WebrtcVideoStreamPtr>::const_iterator it = streamMap.begin(); it != streamMap.end(); it++ )
    {
        WebrtcVideoStreamPtr stream = it->second;
        if(stream->channelId == channel)
            return stream;
    }
    return WebrtcVideoStreamPtr();
}

int WebrtcVideoProvider::getChannelForStreamId( int streamId )
{
    for( std::map<int, WebrtcVideoStreamPtr>::const_iterator it = streamMap.begin(); it != streamMap.end(); it++ )
    {
    	WebrtcVideoStreamPtr stream = it->second;
        if(stream->streamId == streamId)
            return stream->channelId;
    }
    return -1;
}

WebrtcVideoStreamPtr WebrtcVideoProvider::getStream( int streamId )
{
	AutoLock lock(streamMapMutex);
	std::map<int, WebrtcVideoStreamPtr>::const_iterator it = streamMap.find( streamId );
	return ( it != streamMap.end() ) ? it->second : WebrtcVideoStreamPtr();
}

void WebrtcVideoProvider::setMuteForStreamId( int streamId, bool muteVideo )
{
	WebrtcVideoStreamPtr stream = getStream(streamId);
	if(muteVideo == true && stream != NULL)
	{
		stream->isMuted = true;
	}
	else if (muteVideo == false && stream != NULL)
	{
		stream->isMuted = false;
	}
}

void WebrtcVideoProvider::setTxInitiatedForStreamId( int streamId, bool txInitiatedValue )
{
	WebrtcVideoStreamPtr stream = getStream(streamId);
	if(txInitiatedValue == true && stream != NULL)
	{
		stream->txInitialised = true;
	}
	else if (txInitiatedValue == false && stream != NULL)
	{
		stream->txInitialised = false;
	}
}

int WebrtcVideoProvider::rxAlloc( int groupId, int streamId, int requestedPort )
{
	AutoLock lock(m_lock);
    LOG_WEBRTC_INFO( logTag, "rxAllocVideo: groupId=%d, streamId=%d, requestedPort=%d", groupId, streamId, requestedPort  );
	int channel = -1;
    int error = vieBase->CreateChannel(channel);
    if ( error == -1 )
    {
        LOG_WEBRTC_DEBUG( logTag, "rxAllocVideo: CreateChannel failed, error %d", 
							vieBase->LastError() );
        return -1;
    }
	
	
	LOG_WEBRTC_DEBUG(logTag," Connecting Capture %d to Channel %d",
									webCaptureId, channel);
	error = vieCapture->ConnectCaptureDevice(webCaptureId, channel);
	if( error == -1)
	{
        LOG_WEBRTC_DEBUG( logTag, "rxAllocVideo: Connect Capture Device failed, error %d", 
							vieBase->LastError() );
        return -1;
	}
	
	LOG_WEBRTC_DEBUG(logTag," Starting the capture ");
	error  = vieCapture->StartCapture(webCaptureId);
	if( error == -1)
	{
        LOG_WEBRTC_DEBUG( logTag, "rxAllocVideo: Start Capture Device failed, error %d", 
						 vieBase->LastError() );
        return -1;
	}
	LOG_WEBRTC_DEBUG(logTag," CAPTURED ");
	
	//TODO: Suhas: Add code to enable key frame request
	vieRtpRtcp->SetRTCPStatus(channel, webrtc::kRtcpCompound_RFC4585);
	vieRtpRtcp->SetKeyFrameRequestMethod(
				channel, webrtc::kViEKeyFrameRequestPliRtcp);
	
    if ( previewWindow != NULL )
    {
#ifdef WIN32    // temporary
        // TODO: implement render scaling
    	// NDM not sure if this is available on linux - needs to check GIPS v4
    	// maybe this call should be moved to setPreviewWindw, since it does not seem
    	// to be linked to the current stream in any way
        //((GipsVideoEnginePlatform *)gipsVideo)->GIPSVideo_AddLocalRenderer( previewWindow, 0, 0.0f, 0.0f, 1.0f, 1.0f );
#endif
    }
    LOG_WEBRTC_INFO( logTag, "rxAllocVideo: Created channel %d", channel );

    int beginPort;        // where we started
    int tryPort;        // where we are now

    if ( requestedPort == 0  || requestedPort < startPort || requestedPort >= endPort )
        tryPort = startPort;
    else
        tryPort = requestedPort;

    beginPort = tryPort;

    do
    {
        if ( vieNetwork->SetLocalReceiver( channel, tryPort, 0, (char *)localIP.c_str() ) == 0 )
        {
            LOG_WEBRTC_DEBUG( logTag, "rxAllocVideo: Allocated port %d", tryPort );
			WebrtcVideoStreamPtr stream(new WebrtcVideoStream(streamId, channel));
			{
				AutoLock lock(streamMapMutex);
				streamMap[streamId] = stream;
				LOG_WEBRTC_DEBUG( logTag, "rxAllocVideo: created stream" );
			}
            return tryPort;
        }

        int errCode = vieBase->LastError();
        if ( errCode == 12061 /* Can't bind socket */ )        
        {
            tryPort += 2;
            if ( tryPort >= endPort )
                tryPort = startPort;
        }
        else
        {
            LOG_WEBRTC_ERROR( logTag, "rxAllocVideo: SetLocalReceiver returned error %d", errCode );
            vieBase->DeleteChannel( channel );
            return 0;
        }
    }
    while ( tryPort != beginPort );

    LOG_WEBRTC_WARN( logTag, "rxAllocVideo: No ports available?" );
    vieBase->DeleteChannel( channel );
    return 0;
}

int WebrtcVideoProvider::rxOpen( int groupId, int streamId, int requestedPort, int listenIp, bool isMulticast )
{
	AutoLock lock(m_lock);
    LOG_WEBRTC_ERROR( logTag, "rxOpen: groupId=%d, streamId=%d", groupId, streamId);

    int channel = getChannelForStreamId( streamId );
    if ( channel >= 0 )
    {
        int audioChannel = provider->pAudio->getChannelForStreamId(audioStreamId);
        if ( audioChannel >= 0 )
        {
            if ( vieBase->ConnectAudioChannel( channel, audioChannel ) != 0 )    // for lip sync
            {
                LOG_WEBRTC_ERROR( logTag, "rxOpen: SetAudioChannel failed on channel %d, error %d", channel, vieBase->LastError() );
            }
        }
        else
        {
            LOG_WEBRTC_ERROR( logTag, "rxOpen: getChannelForStreamId returned %d", audioChannel);
        }
        return requestedPort;
    }

    return 0;
}

int WebrtcVideoProvider::rxStart ( int groupId, int streamId, int payloadType, int packPeriod, int localPort, int rfc2833PayloadType,
                                 EncryptionAlgorithm algorithm, unsigned char* key, int keyLen, unsigned char* salt, int saltLen, int mode, int party )
{
	AutoLock lock(m_lock);
    LOG_WEBRTC_INFO( logTag, "rxStartVideo: groupId=%d, streamId=%d, pt=%d", groupId, streamId, payloadType );

	int error = 0;
    int channel = getChannelForStreamId( streamId );
    if ( channel >= 0 )
    {
		//Let's set the recieve code to all supported by webrtc today
#if defined __H264__
		webrtc::VideoCodec videoCodec = H264template;
		webrtc::VideoCodecUnion codecSpec;
		codecSpec.H264.level = 12;
		codecSpec.H264.quality= 0;

		h264.codecType = webrtc::kVideoCodecH264;
        h264.plType = payloadType;
        h264.maxBitrate = 500;
		h264.minBitrate = 300;
		h264.startBitrate = 300;
		h264.maxFramerate = 15;
        h264.codecSpecific = codecSpec;    
#endif

		memset(&videoCodec, 0 , sizeof(webrtc::VideoCodec));
		int codecIdx = 0;
		for(codecIdx = 0; codecIdx < vieCodec->NumberOfCodecs(); codecIdx++)
		{
			error = vieCodec->GetCodec(codecIdx, videoCodec);
			if(error == -1)
			{
				 LOG_WEBRTC_ERROR(logTag, "rxStart: Vie::GetCodec: failed: %d", 
									vieBase->LastError());	
				return -1;
			}
		  	// try to keep the test frame size small when I420
        	if (videoCodec.codecType == webrtc::kVideoCodecI420)
        	{
           		 videoCodec.width = 176;
           		 videoCodec.height = 144;
        	}

        	error = vieCodec->SetReceiveCodec(channel, videoCodec);
        	if (error == -1)
       		 {
				 LOG_WEBRTC_ERROR(logTag, "rxStart: Vie::SetReceiveCodec: failed: %d", 
									vieBase->LastError());	
       		     return -1;
        	}
		}//end for

		//Let's Set VP8 + VGA Frame Size for now as send codec- which happens to be codec0
		error = vieCodec->GetCodec(vp8Idx, videoCodec);
		if(error == -1)
		{
			 LOG_WEBRTC_ERROR(logTag, "rxStart: Vie::GetCodec for send failed: %d", 
								vieBase->LastError());	
       		  return -1;
		}
		videoCodec.width = 640;
		videoCodec.height = 480;
        if ( vieCodec->SetSendCodec( channel, videoCodec) != 0 )
        {
 
            LOG_WEBRTC_ERROR( logTag, "txStartVideo: ViE::SendCodec on channel %d failed, error %d", 
										channel, vieBase->LastError() );
        }
#ifdef LINUX
        // There's a problem with setting certain cameras e.g Cisco VT2 to 15 fps on Linux
        // If we fail then retrying seems to fix it
        else
        {
            if (vieCodec->SetSendCodec( channel, videoCodec ) != 0 )
            {
                LOG_WEBRTC_ERROR( logTag, "txStartVideo: ViECodec:SetSendCodec at 30 fps on channel %d failed, error %d", channel, vieBase->LastError() );
            }
        }
#endif
        switch(algorithm)
        {
            case EncryptionAlgorithm_NONE:
                LOG_WEBRTC_DEBUG( logTag, "rxStartVideo: using non-secure RTP for channel %d", channel);
                break;

            case EncryptionAlgorithm_AES_128_COUNTER:
            {
                // SRTP support was removed in WEBRTC
                LOG_WEBRTC_ERROR(logTag, "SRTP is not supported in WEBRTC");

                return -1;
            }  
        }
		
		//Start the Reciever Engine
		vieBase->StartReceive( channel );

        return 0;
    }
    return -1;
}

void WebrtcVideoProvider::setRenderWindowForStreamIdFromMap(int streamId)
{
	LOG_WEBRTC_DEBUG( logTag, "setRenderWindowForStreamIdFromMap, streamId: %d", streamId);
	int channel = getChannelForStreamId( streamId );
    const RenderWindow* remote = getRenderWindow(streamId );
    if ( remote != NULL )
    {
#ifdef WIN32    // temporary
        // TODO: implement render scaling
        if (  vieRender->AddRenderer( channel, (HWND) remote->window, 1, 0.0f, 0.0f, 1.0f, 1.0f ) != 0 )
        {
            LOG_WEBRTC_ERROR( logTag, "setRenderWindowForStreamIdFromMap: Addenderer on channel %d failed, error %d", 
								channel, vieBase->LastError() );
        }
#endif
#ifdef LINUX
        // TODO: implement render scaling
        if ( vieRender->AddRenderer( channel, (Window*)remote->window,1, 0.0f, 0.0f, 1.0f, 1.0f ) != 0 )
        {
            LOG_WEBRTC_ERROR( logTag, "setRenderWindowForStreamIdFromMap: AddRenderer on channel %d failed, error %d",
								channel, vieBase->LastError() );
        }
#endif
#ifdef __APPLE__
		LOG_WEBRTC_DEBUG(logTag, "AddRenderer for the channell %d", channel);
		if ( vieRender->AddRenderer( channel, (WindowRef*)remote->window, 1, 0.0f, 0.0f, 1.0f, 1.0f ) != 0 )
        {
            LOG_WEBRTC_ERROR( logTag, "setRenderWindowForStreamIdFromMap: AddRenderer on channel %d failed, error %d",
								 channel, vieBase->LastError() );
        }
#endif
    }
    else
    {
    	LOG_WEBRTC_ERROR( logTag, "Remote window is NULL" );
    }
}

void WebrtcVideoProvider::rxClose( int groupId, int streamId)
{
	AutoLock lock(m_lock);
    LOG_WEBRTC_INFO( logTag, "rxCloseVideo: groupId=%d, streamId=%d", groupId, streamId);
    int channel = getChannelForStreamId( streamId );
    if ( channel >= 0 )
    {
        vieRender->StopRender( channel );
        LOG_WEBRTC_INFO( logTag, "rxCloseVideo: Stop render on channel %d", channel );
		
    }
}

void WebrtcVideoProvider::rxRelease( int groupId, int streamId, int port )
{
	AutoLock lock(m_lock);
    LOG_WEBRTC_INFO( logTag, "rxReleaseVideo: groupId=%d, streamId=%d", groupId, streamId);
    int channel = getChannelForStreamId( streamId );
    if ( channel >= 0 )
    {
		vieBase->DisconnectAudioChannel( channel );
		vieBase->StopReceive(channel);
		vieRender->RemoveRenderer(channel);
        vieBase->DeleteChannel( channel );
        {
        	AutoLock lock(streamMapMutex);
        	streamMap.erase(streamId);
        }
        LOG_WEBRTC_DEBUG( logTag, "rxReleaseVideo: Delete channel %d, release port %d", channel, port);
    }
}

int WebrtcVideoProvider::txStart( int groupId, int streamId, int payloadType, int packPeriod, bool vad, short tos,
                                char* remoteIpAddr, int remotePort, int rfc2833PayloadType, EncryptionAlgorithm algorithm,
                                unsigned char* key, int keyLen, unsigned char* salt, int saltLen, int mode, int party  )
{
	AutoLock lock(m_lock);
    LOG_WEBRTC_INFO( logTag, "txStartVideo: groupId=%d, streamId=%d, pt=%d", groupId, streamId, payloadType );
    int channel = getChannelForStreamId( streamId );
	
    if ( channel >= 0 )
    {
        switch(algorithm)
        {
            case EncryptionAlgorithm_NONE:
                LOG_WEBRTC_DEBUG( logTag, "txStartVideo: using non-secure RTP for channel %d", channel);
                break;

            case EncryptionAlgorithm_AES_128_COUNTER:
            {
                // SRTP support was removed in WEBRTC
                LOG_WEBRTC_ERROR(logTag, "SRTP is not supported in WEBRTC");

                return -1;
            }  
        }

        vieNetwork->SetSendDestination( channel, remoteIpAddr, remotePort );
        // We might be muted - for example in the case where the call is being resumed, so respect that setting
		WebrtcVideoStreamPtr stream = getStream(streamId);
		
		if (stream != NULL && ! stream->isMuted)
    	{
			LOG_WEBRTC_DEBUG(logTag," Starting the send: Suhas ");	
    		vieBase->StartSend( channel );
    	}
        setTxInitiatedForStreamId(streamId, true);
        LOG_WEBRTC_DEBUG( logTag, "txStartVideo: Sending to %s:%d on channel %d", remoteIpAddr, remotePort, channel );
		
		
		int channel = getChannelForStreamId( streamId );
		if ( vieRender->StartRender( channel ) != 0 )
        {
            LOG_WEBRTC_ERROR( logTag, "TxStartVideo: StartRender on channel %d failed, error %d", channel, vieBase->LastError() );
        }
        else
        {
            LOG_WEBRTC_DEBUG( logTag, "TxStartVideo: Rendering on channel %d", channel );
        }

		        return 0;
    }
    return -1;
}

void WebrtcVideoProvider::txClose( int groupId, int streamId )
{
	AutoLock lock(m_lock);
    LOG_WEBRTC_INFO( logTag, "txCloseVideo: groupId=%d, streamId=%d", groupId, streamId );
    int channel = getChannelForStreamId( streamId );
    if ( channel >= 0 )
    {
        vieBase->StopSend( channel );
        LOG_WEBRTC_DEBUG( logTag, "txCloseVideo: Stop transmit on channel %d", channel );
        setTxInitiatedForStreamId(streamId, false);
		//let's stop the capture
		vieCapture->StopCapture(webCaptureId);
		vieCapture->DisconnectCaptureDevice(channel);
    }
}

bool WebrtcVideoProvider::mute(int streamId, bool muteVideo)
{
	AutoLock lock(m_lock);
    int channel = getChannelForStreamId( streamId );
    bool returnVal = false;
	WebrtcVideoStreamPtr stream = getStream(streamId);
    LOG_WEBRTC_INFO( logTag, "mute: streamId=%d, mute=%d, channel=%d", streamId, muteVideo, channel );
    	
    if ( channel >= 0 )
    {
    	if( muteVideo == true )
    	{
			if (stream->txInitialised)
			{
    			if (vieBase->StopSend( channel ) != -1)
    			{
    				returnVal= true;
    			}
    			else
    			{
    				LOG_WEBRTC_ERROR( logTag, "GIPS returned failure from GIPSVideo_StopSend");
    			}
			}
			else
			{
    			returnVal= true;				
			}
    	}
        else
        {
			if (stream->txInitialised)
			{
				if (vieBase->StartSend( channel ) != -1)
    			{
    				returnVal= true;
    			}
    			else
    			{
    				LOG_WEBRTC_ERROR( logTag, "GIPS returned failure from GIPSVideo_StartSend");
    			}
			}
			else
			{
				returnVal= true;
			}
        }
    	if (returnVal == true)
    	{
    		setMuteForStreamId( streamId, muteVideo );
    	}
    }
    return returnVal;
}

bool WebrtcVideoProvider::isMuted(int streamId)
{
	AutoLock lock(m_lock);
	WebrtcVideoStreamPtr stream = getStream(streamId);
	bool returnVal = false;

	if ((stream != NULL) && (stream->isMuted == true))
	{
		returnVal = true;
	}

	return returnVal;
}

bool WebrtcVideoProvider::setFullScreen(int streamId, bool fullScreen)
{
	AutoLock lock(m_lock);
	int returnVal = -1;
	LOG_WEBRTC_INFO(logTag," setFullScreen: Operation not supported ");
#ifdef WIN32
	/*const RenderWindow* renderWindow = getRenderWindow( streamId );
	if ( renderWindow != NULL )
	{
		if (fullScreen)
		{
			returnVal = ((GipsVideoEnginePlatform *)gipsVideo)->GIPSVideo_AddFullScreenRender((HWND)renderWindow->window);
		}
		else
		{
			returnVal = ((GipsVideoEnginePlatform *)gipsVideo)->GIPSVideo_AddFullScreenRender(NULL);  // turn full screen off
		}
	}*/
#endif
	return (returnVal == 0) ? true : false;
}

void WebrtcVideoProvider::sendIFrame( int streamId )
{
	AutoLock lock(m_lock);
    LOG_WEBRTC_INFO( logTag, "Remote end requested I-frame %d: " ,streamId );
    int channel = getChannelForStreamId( streamId );
    if ( channel >= 0 )
    {
        vieCodec->SendKeyFrame( channel );
    }
    else
    {
    	LOG_WEBRTC_INFO( logTag, "sendIFrame: getChannelForStreamId returned %d", channel );
    }
}

void WebrtcVideoProvider::RequestNewKeyFrame		(int channel)
{
#ifdef LINUX

    double elapsed;

    currentTime = clock();
    elapsed = ((double) (currentTime-lastRequestTime)) / CLOCKS_PER_SEC;
    // If last request was sent less than a second ago we debounce
    if (elapsed <=1)
    {
        LOG_WEBRTC_INFO(logTag, "Last request was sent less than a second ago - do nothing" );
        return;
    }
#endif

    LOG_WEBRTC_INFO(logTag, "Send Request for I-frame to originator" );
    WebrtcVideoStreamPtr stream= getStreamByChannel(channel);
    MediaProviderObserver *mpobs = VcmSIPCCBinding::getMediaProviderObserver();
    if (mpobs != NULL)
        mpobs->onKeyFrameRequested(stream->streamId);
#ifdef LINUX
    lastRequestTime = clock();
#endif
}

void WebrtcVideoProvider::IncomingRate(int channel, unsigned int frameRate, unsigned int bitrate)
{
    //LOG_WEBRTC_INFO( logTag, "IncomingRate channel %d, frameRate %d, bitrate %d", channel, frameRate, bitrate );
}

void WebrtcVideoProvider::IncomingCodecChanged(int channel, const webrtc::VideoCodec& videoCodec)
{
    LOG_WEBRTC_INFO( logTag, "IncomingCodecChanged channel %d, payloadType %d, width %d, height %d", channel, videoCodec.plType, videoCodec.width, videoCodec.height );
}

void WebrtcVideoProvider::IncomingCSRCChanged(int channel, unsigned int csrc, bool added)
{
//    LOG_WEBRTC_INFO( logTag, "IncomingRate channel %d, csrc %d, added %d", channel, csrc, added );
}

void WebrtcVideoProvider::IncomingSSRCChanged(int channel, unsigned int ssrc)
{
	//    LOG_WEBRTC_INFO( logTag, "IncomingRate channel %d, csrc %d, added %d", channel, csrc, added );
}
			
			
void WebrtcVideoProvider::OutgoingRate(int channel, unsigned int frameRate, unsigned int bitrate)
{
//    LOG_WEBRTC_INFO( logTag, "SendRate channel %d, frameRate %d, bitrate %d", channel, frameRate, bitrate );
}

void WebrtcVideoProvider:: setAudioStreamId(int streamId)
{
    audioStreamId=streamId;
}

} // namespace CSF

#endif // NO_WEBRTC_VIDEO
#endif
