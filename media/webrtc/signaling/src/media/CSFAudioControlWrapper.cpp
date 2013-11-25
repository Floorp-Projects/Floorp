/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "CSFAudioControlWrapper.h"

static const char* logTag = "VcmSipccBinding";

namespace CSF {

	std::vector<std::string> AudioControlWrapper::getRecordingDevices()
	{
		if (_realAudioControl != nullptr)
		{
			return _realAudioControl->getRecordingDevices();
		}
		else
		{
			CSFLogWarn( logTag, "Attempt to getRecordingDevices for expired audio control");
			std::vector<std::string> vec;
			return vec;
		}
	}

	std::vector<std::string> AudioControlWrapper::getPlayoutDevices()
	{
		if (_realAudioControl != nullptr)
		{
			return _realAudioControl->getPlayoutDevices();
		}
		else
		{
			CSFLogWarn( logTag, "Attempt to getPlayoutDevices for expired audio control");
			std::vector<std::string> vec;
			return vec;
		}
	}

	std::string AudioControlWrapper::getRecordingDevice()
	{
		if (_realAudioControl != nullptr)
		{
			return _realAudioControl->getRecordingDevice();
		}
		else
		{
			CSFLogWarn( logTag, "Attempt to getRecordingDevice for expired audio control");
			return "";
		}
	}

	std::string AudioControlWrapper::getPlayoutDevice()
	{
		if (_realAudioControl != nullptr)
		{
			return _realAudioControl->getPlayoutDevice();
		}
		else
		{
			CSFLogWarn( logTag, "Attempt to getPlayoutDevice for expired audio control");
			return "";
		}
	}

	bool AudioControlWrapper::setRecordingDevice( const std::string& name )
	{
		if (_realAudioControl != nullptr)
		{
			return _realAudioControl->setRecordingDevice(name);
		}
		else
		{
			CSFLogWarn( logTag, "Attempt to setRecordingDevice to %s for expired audio control",
				name.c_str());
			return false;
		}
	}

	bool AudioControlWrapper::setPlayoutDevice( const std::string& name )
	{
		if (_realAudioControl != nullptr)
		{
			return _realAudioControl->setPlayoutDevice(name);
		}
		else
		{
			CSFLogWarn( logTag, "Attempt to setPlayoutDevice to %s for expired audio control",
				name.c_str());
			return false;
		}
	}

    bool AudioControlWrapper::setDefaultVolume( int volume )
    {
		if (_realAudioControl != nullptr)
		{
			return _realAudioControl->setDefaultVolume(volume);
		}
		else
		{
			CSFLogWarn( logTag, "Attempt to setDefaultVolume for expired audio control");
			return false;
		}
    }

    int AudioControlWrapper::getDefaultVolume()
    {
		if (_realAudioControl != nullptr)
		{
			return _realAudioControl->getDefaultVolume();
		}
		else
		{
			CSFLogWarn( logTag, "Attempt to getDefaultVolume for expired audio control");
			return -1;
		}
    }

    bool AudioControlWrapper::setRingerVolume( int volume )
    {
		if (_realAudioControl != nullptr)
		{
			return _realAudioControl->setRingerVolume(volume);
		}
		else
		{
			CSFLogWarn( logTag, "Attempt to setRingerVolume for expired audio control");
			return false;
		}
    }

    int AudioControlWrapper::getRingerVolume()
    {
		if (_realAudioControl != nullptr)
		{
			return _realAudioControl->getRingerVolume();
		}
		else
		{
			CSFLogWarn( logTag, "Attempt to getRingerVolume for expired audio control");
			return -1;
		}
    }

    AudioControlWrapper::~AudioControlWrapper()
    {
        delete _realAudioControl;
    }
}
