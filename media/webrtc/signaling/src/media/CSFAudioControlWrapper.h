/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "mozilla/RefPtr.h"
#include "CC_Common.h"
#include "CSFAudioControl.h"

namespace CSF
{
        DECLARE_NS_PTR(AudioControlWrapper)
	class ECC_API AudioControlWrapper : public AudioControl
	{
	public:
		// device names are in UTF-8 encoding

		explicit AudioControlWrapper(AudioControl * audioControl){_realAudioControl = audioControl;};
		virtual std::vector<std::string> getRecordingDevices();
		virtual std::vector<std::string> getPlayoutDevices();

		virtual std::string getRecordingDevice();
		virtual std::string getPlayoutDevice();

		virtual bool setRecordingDevice( const std::string& name );
		virtual bool setPlayoutDevice( const std::string& name );

        virtual bool setDefaultVolume( int volume );
        virtual int getDefaultVolume();

        virtual bool setRingerVolume( int volume );
        virtual int getRingerVolume();

		virtual void setAudioControl(AudioControl * audioControl){_realAudioControl = audioControl;};

	private:
		virtual ~AudioControlWrapper();

		RefPtr<AudioControl> _realAudioControl;
	};
};
