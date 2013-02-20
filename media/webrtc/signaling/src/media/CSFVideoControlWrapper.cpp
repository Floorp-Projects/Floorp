/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "CSFVideoControlWrapper.h"

static const char* logTag = "VcmSipccBinding";

namespace CSF {

void VideoControlWrapper::setVideoMode( bool enable )
{
	if (_realVideoControl != NULL)
	{
		_realVideoControl->setVideoMode(enable);
	}
	else
	{
		CSFLogWarn( logTag, "Attempt to setVideoMode to %s for expired video control",
			enable ? "TRUE" : "FALSE");
	}
}

void VideoControlWrapper::setPreviewWindow( VideoWindowHandle window, int top, int left, int bottom, int right, RenderScaling style )
{
	if (_realVideoControl != NULL)
	{
		_realVideoControl->setPreviewWindow(window, top, left, bottom, right, style);
	}
	else
	{
		CSFLogWarn( logTag, "Attempt to setPreviewWindow for expired video control");
	}
}


void VideoControlWrapper::showPreviewWindow( bool show )
{
	if (_realVideoControl != NULL)
	{
		_realVideoControl->showPreviewWindow(show);
	}
	else
	{
		CSFLogWarn( logTag, "Attempt to showPreviewWindow( %s ) for expired video control",
			show ? "TRUE" : "FALSE");
	}
}


std::vector<std::string> VideoControlWrapper::getCaptureDevices()
{
	if (_realVideoControl != NULL)
	{
		return _realVideoControl->getCaptureDevices();
	}
	else
	{
		CSFLogWarn( logTag, "Attempt to getCaptureDevices for expired video control");
		std::vector<std::string> vec;
		return vec;
	}
}


std::string VideoControlWrapper::getCaptureDevice()
{
	if (_realVideoControl != NULL)
	{
		return _realVideoControl->getCaptureDevice();
	}
	else
	{
		CSFLogWarn( logTag, "Attempt to getCaptureDevice for expired video control");
		return "";
	}
}

bool VideoControlWrapper::setCaptureDevice( const std::string& name )
{
	if (_realVideoControl != NULL)
	{
		return _realVideoControl->setCaptureDevice(name);
	}
	else
	{
		CSFLogWarn( logTag, "Attempt to setCaptureDevice to %s for expired video control",
			name.c_str());
		return false;
	}
}

}
