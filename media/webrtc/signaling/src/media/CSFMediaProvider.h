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
