/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CSFMEDIAPROVIDER_H_
#define CSFMEDIAPROVIDER_H_

#if __cplusplus

#include <string>

using namespace std;

namespace CSF
{
	class AudioControl;
	class VideoControl;
	class AudioTermination;
	class VideoTermination;
	class MediaProviderObserver;


	class MediaProvider
	{
	public:
		static MediaProvider* create( );///factory method for all MediaProvider derived types (ctor is protected).
		virtual ~MediaProvider() = 0;

		virtual int init() = 0;
		virtual void shutdown() = 0;

		virtual AudioControl* getAudioControl() = 0;
		virtual VideoControl* getVideoControl() = 0;
		virtual AudioTermination* getAudioTermination() = 0;
		virtual VideoTermination* getVideoTermination() = 0;

		virtual void addMediaProviderObserver( MediaProviderObserver* observer ) = 0;

	protected:
        MediaProvider() {};
	};

	class MediaProviderObserver
	{
	public:
		virtual void onVideoModeChanged( bool enable ) {}
		virtual void onKeyFrameRequested( int callId ) {}
		virtual void onMediaLost( int callId ) {}
		virtual void onMediaRestored( int callId ) {}
	};

} // namespace

#endif // __cplusplus

#endif /* CSFMEDIAPROVIDER_H_ */
