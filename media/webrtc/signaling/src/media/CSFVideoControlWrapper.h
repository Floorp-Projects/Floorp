/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CSFVIDEOCONTROLWRAPPER_H_
#define CSFVIDEOCONTROLWRAPPER_H_

#include "CC_Common.h"
#include "CSFVideoControl.h"

#if __cplusplus

#include <string>
#include <vector>

namespace CSF
{
    DECLARE_NS_PTR(VideoControlWrapper)
    typedef void *VideoWindowHandle;

	class ECC_API VideoControlWrapper : public VideoControl
	{
	public:
		explicit VideoControlWrapper(VideoControl * videoControl){_realVideoControl = videoControl;};

		virtual void setVideoMode( bool enable );

		// window type is platform-specific
		virtual void setPreviewWindow( VideoWindowHandle window, int top, int left, int bottom, int right, RenderScaling style );
		virtual void showPreviewWindow( bool show );

		// device names are in UTF-8 encoding
		virtual std::vector<std::string> getCaptureDevices();

		virtual std::string getCaptureDevice();
		virtual bool setCaptureDevice( const std::string& name );

		virtual void setVideoControl( VideoControl * videoControl ){_realVideoControl = videoControl;};

	private:
		VideoControl * _realVideoControl;
	};

} // namespace

#endif // __cplusplus

#endif /* CSFVIDEOCONTROLWRAPPER_H_ */
