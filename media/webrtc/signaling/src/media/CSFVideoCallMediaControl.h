/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CSFVIDEOCALLMEDIACONTROL_H_
#define CSFVIDEOCALLMEDIACONTROL_H_

#include <csf/CSFVideoMediaControl.h>

#if __cplusplus

namespace CSF
{
	class VideoCallMediaControl
	{
	public:
		virtual void setVideoMode( VideoEnableMode mode ) = 0;

		// window type is platform-specific
		virtual void setRemoteWindow( VideoWindowHandle window ) = 0;
		virtual void showRemoteWindow( bool show ) = 0;
	};

} // namespace

#endif // __cplusplus

#endif /* CSFVIDEOCALLMEDIACONTROL_H_ */
