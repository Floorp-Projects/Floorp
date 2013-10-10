/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CSF_VCM_SIPCC_BINDING_H_
#define _CSF_VCM_SIPCC_BINDING_H_

extern "C"
{
#include "ccapi_types.h"
}

#include "sigslot.h"

class nsIThread;
class nsIEventTarget;

namespace mozilla {
    class NrIceMediaStream;
};

namespace CSF
{
    class AudioTermination;
    class VideoTermination;
    class AudioControl;
    class VideoControl;
    class MediaProvider;
    class MediaProviderObserver;

    class StreamObserver
    {
    public:
    	virtual void registerStream(cc_call_handle_t call, int streamId, bool isVideo) = 0;
    	virtual void deregisterStream(cc_call_handle_t call, int streamId) = 0;
    	virtual void dtmfBurst(int digit, int direction, int duration) = 0;
    	virtual void sendIFrame(cc_call_handle_t call) = 0;
    };
    class VcmSIPCCBinding : public sigslot::has_slots<>
    {
    public:
        VcmSIPCCBinding ();
        virtual ~VcmSIPCCBinding();

        // The getter is only for use by the vcm_* impl functions.
        void setStreamObserver(StreamObserver*);
        static StreamObserver* getStreamObserver();

        static AudioTermination * getAudioTermination();
        static VideoTermination * getVideoTermination();

        static AudioControl * getAudioControl();
        static VideoControl * getVideoControl();

        void setMediaProviderObserver(MediaProviderObserver* obs);
        static MediaProviderObserver * getMediaProviderObserver();

        static void setAudioCodecs(int codecMask);
        static void setVideoCodecs(int codecMask);

        static int getAudioCodecs();
        static int getVideoCodecs();

	static void setMainThread(nsIThread *thread);
	static nsIThread *getMainThread();
	static nsIEventTarget *getSTSThread();

	static void setSTSThread(nsIEventTarget *thread);

	static void connectCandidateSignal(mozilla::NrIceMediaStream* stream);

    private:
	void CandidateReady(mozilla::NrIceMediaStream* stream,
			    const std::string& candidate);

        static VcmSIPCCBinding * gSelf;
        StreamObserver* streamObserver;
        MediaProviderObserver *mediaProviderObserver;
        static int gAudioCodecMask;
        static int gVideoCodecMask;
	static nsIThread *gMainThread;
	static nsIEventTarget *gSTSThread;
    };
}

#endif


