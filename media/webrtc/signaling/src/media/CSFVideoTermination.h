/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CSFVIDEOMEDIATERMINATION_H_
#define CSFVIDEOMEDIATERMINATION_H_

#include <CSFMediaTermination.h>
#include <CSFVideoControl.h>

typedef enum
{
	VideoCodecMask_H264 = 1,
	VideoCodecMask_H263 = 2

} VideoCodecMask;

#if __cplusplus

namespace CSF
{
	class VideoTermination : public MediaTermination
	{
	public:
		virtual void setRemoteWindow( int streamId, VideoWindowHandle window) = 0;
		virtual int setExternalRenderer( int streamId, VideoFormat videoFormat, ExternalRendererHandle render) = 0;
		virtual void sendIFrame	( int streamId ) = 0;
		virtual bool  mute		( int streamId, bool mute ) = 0;
		virtual void setAudioStreamId( int streamId) = 0;
	};

} // namespace

#endif // __cplusplus

#endif /* CSFVIDEOMEDIATERMINATION_H_ */
