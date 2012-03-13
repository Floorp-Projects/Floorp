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

#ifndef WebrtcAUDIOMEDIAPROVIDER_H_
#define WebrtcAUDIOMEDIAPROVIDER_H_

#ifndef _USE_CPVE

#include "csf_common.h"
#include "CSFAudioControl.h"
#include "CSFAudioTermination.h"
#include "WebrtcAudioCodecSelector.h"

#include "voe_base.h"
#include "voe_file.h"
#include "voe_hardware.h"
#include "voe_dtmf.h"
#include "voe_network.h"
#include "voe_audio_processing.h"
#include "voe_volume_control.h"
#include "voe_encryption.h"
#include <string>
#include <map>
#include "base/lock.h"


namespace CSF
{
    class WebrtcMediaProvider;
    class WebrtcToneGenerator;
    class WebrtcRingGenerator;
    class WebrtcAudioStream;
    class WebrtcVideoProvider;
    DECLARE_PTR(WebrtcAudioStream);

    class WebrtcAudioProvider : public AudioControl, AudioTermination, webrtc::VoiceEngineObserver,
            webrtc::VoEConnectionObserver
            ,webrtc::TraceCallback {
    friend class WebrtcVideoProvider;

    public:

        WebrtcAudioProvider( WebrtcMediaProvider* provider );

        // destructor
        ~WebrtcAudioProvider();

        // initialize members and Webrtc API
        // return 0 if initialzation succeeded
        int init();

        // AudioControl
        AudioControl* getAudioControl() { return this; }

        std::vector<std::string> getRecordingDevices();
        std::vector<std::string> getPlayoutDevices();

        std::string getRecordingDevice() { return recordingDevice; }
        std::string getPlayoutDevice() { return playoutDevice; }

        bool setRecordingDevice( const std::string& name );
        bool setPlayoutDevice( const std::string& name );

        bool setDefaultVolume( int volume );
        int getDefaultVolume();

        bool setRingerVolume( int volume );
        int getRingerVolume();

        bool setVolume( int streamId, int volume );
        int  getVolume( int streamId );

        AudioTermination * getAudioTermination() { return this; }

        int  getCodecList( CodecRequestType requestType );

        int  rxAlloc    ( int groupId, int streamId, int requestedPort );
        void setLocalIP    ( const char* addr ) { localIP = addr; }
        int  rxOpen        ( int groupId, int streamId, int requestedPort, int listenIp, bool isMulticast );
        int  rxStart    ( int groupId, int streamId, int payloadType, int packPeriod, int localPort, int rfc2833PayloadType,
                                EncryptionAlgorithm algorithm, unsigned char* key, int keyLen, unsigned char* salt, int saltLen, int mode, int party );
        void rxClose    ( int groupId, int streamId);
        void rxRelease    ( int groupId, int streamId, int port );
        int  txStart    ( int groupId, int streamId, int payloadType, int packPeriod, bool vad, short tos,
                                char* remoteIpAddr, int remotePort, int rfc2833PayloadType, EncryptionAlgorithm algorithm,
                                unsigned char* key, int keyLen, unsigned char* salt, int saltLen, int mode, int party );
        void txClose    ( int groupId, int streamId);

        int  toneStart    ( ToneType type, ToneDirection direction, int alertInfo, int groupId, int streamId, bool useBackup );
        int  toneStop    ( ToneType type, int groupId, int streamId);
        int  ringStart    ( int lineId, RingMode mode, bool once );
        int  ringStop    ( int lineId );
        int  sendDtmf    ( int streamId, int digit);
        bool  mute        ( int streamId, bool mute );
        bool isMuted    ( int streamId );
        void setMediaPorts( int startPort, int endPort ) { this->startPort = startPort; this->endPort = endPort; }
        void setDSCPValue (int value){this->DSCPValue = value;}
        void setVADEnabled(bool VADEnabled){this->VADEnabled = VADEnabled;}

        // used by video, for lip sync
        webrtc::VoiceEngine* getVoiceEngine() { return voeVoice; }

    protected:
        // WebrtcVoiceEngineObserver
        void CallbackOnError(const int errCode, const int channel);
        void Print(const webrtc::TraceLevel level, const char* message, const int length);
        // WebrtcVEConnectionObserver
        void OnPeriodicDeadOrAlive(int channel, bool alive);

        int getChannelForStreamId( int streamId );
        WebrtcAudioStreamPtr getStream( int streamId );
        WebrtcAudioStreamPtr getStreamByChannel( int channelId );

    private:
        WebrtcMediaProvider* provider;
        webrtc::VoiceEngine* voeVoice;
        webrtc::VoEBase* voeBase;
        webrtc::VoEFile* voeFile;
        webrtc::VoEHardware* voeHw;
        webrtc::VoEDtmf* voeDTMF;
        webrtc::VoENetwork* voeNetwork;
        webrtc::VoEVolumeControl* voeVolumeControl;
        webrtc::VoEAudioProcessing* voeVoiceQuality; 
        webrtc::VoEEncryption* voeEncryption;
        int localToneChannel;
        int localRingChannel;
        std::string recordingDevice;
        std::string playoutDevice;
        std::map<int, int> streamToChannel;
        std::map<int, WebrtcAudioStreamPtr> streamMap;
        WebrtcToneGenerator* toneGen;    // temporary, need to manage multiple tones
        WebrtcRingGenerator* ringGen;    // temporary, need to use audio device directly
        std::string localIP;
        int startPort;
        int endPort;
        int defaultVolume;  // Range 0-100
        int ringerVolume;   // Range 0-100
        int DSCPValue;
        bool VADEnabled;

        WebrtcAudioCodecSelector codecSelector;

        // Synchronisation (to avoid data corruption and worse given that so many threads call the media provider)
        // Never use this mutex in a callback from Webrtc - high probability of deadlock.

        Lock m_lock;
        // This mutex is to be held only for the narrowest possible scope while accessing the stream map
        // (but not while inspecting or changing a stream object).
        // Might be used in northbound and southbound calls.
        Lock streamMapMutex;
        bool stopping;
    };
    const unsigned short targetLeveldBOvdefault =3 ;
    const unsigned short digitalCompressionGaindBdefault =9;
    const bool limiterEnableon = true;
} // namespace

#endif
#endif /* WebrtcAUDIOMEDIAPROVIDER_H_ */
