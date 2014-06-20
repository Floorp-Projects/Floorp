/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "CC_Common.h"

namespace CSF {
class AudioControl;
}

namespace mozilla {
template<>
struct HasDangerousPublicDestructor<CSF::AudioControl>
{
  static const bool value = true;
};
}

namespace CSF
{
        DECLARE_NS_PTR(AudioControl)
	class ECC_API AudioControl
	{
	public:
                NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioControl)
		// device names are in UTF-8 encoding

		virtual std::vector<std::string> getRecordingDevices() = 0;
		virtual std::vector<std::string> getPlayoutDevices() = 0;

		virtual std::string getRecordingDevice() = 0;
		virtual std::string getPlayoutDevice() = 0;

		virtual bool setRecordingDevice( const std::string& name ) = 0;
		virtual bool setPlayoutDevice( const std::string& name ) = 0;

        virtual bool setDefaultVolume( int ) = 0;
        virtual int getDefaultVolume() = 0;

        virtual bool setRingerVolume( int ) = 0;
        virtual int getRingerVolume() = 0;

        virtual ~AudioControl(){};
	};
};
