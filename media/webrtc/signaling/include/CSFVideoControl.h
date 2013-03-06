/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "CC_Common.h"
#include "ECC_Types.h"

#include <string>
#include <vector>

namespace CSF
{
	DECLARE_NS_PTR(VideoControl)
	class ECC_API VideoControl
	{
	public:
                NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoControl)
		virtual ~VideoControl() {};

		virtual void setVideoMode( bool enable ) = 0;

		// window type is platform-specific
		virtual void setPreviewWindow( VideoWindowHandle window, int top, int left, int bottom, int right, RenderScaling style ) = 0;
		virtual void showPreviewWindow( bool show ) = 0;

		// device names are in UTF-8 encoding
		virtual std::vector<std::string> getCaptureDevices() = 0;

		virtual std::string getCaptureDevice() = 0;
		virtual bool setCaptureDevice( const std::string& name ) = 0;
	};

}; // namespace
