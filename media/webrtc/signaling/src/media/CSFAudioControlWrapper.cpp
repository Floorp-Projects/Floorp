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

#include "CSFAudioControlWrapper.h"
#include "CSFLogStream.h"

static const char* logTag = "VcmSipccBinding";

namespace CSF {

	std::vector<std::string> AudioControlWrapper::getRecordingDevices()
	{
		if (_realAudioControl != NULL)
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
		if (_realAudioControl != NULL)
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
		if (_realAudioControl != NULL)
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
		if (_realAudioControl != NULL)
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
		if (_realAudioControl != NULL)
		{
			return _realAudioControl->setRecordingDevice(name);
		}
		else
		{
			CSFLogWarnS( logTag, "Attempt to setRecordingDevice to " << name << " for expired audio control");
			return false;
		}
	}

	bool AudioControlWrapper::setPlayoutDevice( const std::string& name )
	{
		if (_realAudioControl != NULL)
		{
			return _realAudioControl->setPlayoutDevice(name);
		}
		else
		{
			CSFLogWarnS( logTag, "Attempt to setPlayoutDevice to " << name << " for expired audio control");
			return false;
		}
	}

    bool AudioControlWrapper::setDefaultVolume( int volume )
    {
		if (_realAudioControl != NULL)
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
		if (_realAudioControl != NULL)
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
		if (_realAudioControl != NULL)
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
		if (_realAudioControl != NULL)
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
