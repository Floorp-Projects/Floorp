/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *Webrtc
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

#ifndef WEBRTCAUDIOCODECSELECTOR_H_
#define WEBRTCAUDIOCODECSELECTOR_H_

#ifndef _USE_CPVE

#include <CSFAudioTermination.h>
#include "voe_base.h"
#include "voe_codec.h"
#include <map>


typedef enum {
  WebrtcAudioPayloadType_PCMU = 0,
  WebrtcAudioPayloadType_PCMA = 8,
  WebrtcAudioPayloadType_G722 = 9,
  WebrtcAudioPayloadType_iLBC = 102,
  WebrtcAudioPayloadType_ISAC = 103,
  WebrtcAudioPayloadType_TELEPHONE_EVENT = 106,
  WebrtcAudioPayloadType_ISACLC = 119,
  WebrtcAudioPayloadType_DIM = -1

} WebrtcAudioPayloadType;

const int ComfortNoisePayloadType = 13;
const int SamplingFreq8000Hz =8000;
const int SamplingFreq16000Hz =16000;
const int SamplingFreq32000Hz =32000;

namespace CSF
{

// A class to select a VOE audio codec
class WebrtcAudioCodecSelector
{
public:
	// the constructor
	WebrtcAudioCodecSelector();

	// the destructor
	~WebrtcAudioCodecSelector();

	int init( webrtc::VoiceEngine* voeVoice, bool useLowBandwidthCodecOnly, bool advertiseG722Codec );

	void release();

	// return a bit mask of the available codecs
	int  advertiseCodecs( CodecRequestType requestType );

	// select the VOE codec according to payload type and packet size
	// return 0 if a codec was selected
	int select( int payloadType, int dynamicPayloadType, int packetSize, webrtc::CodecInst& selectedCoded );

	// apply a sending codec to the channel
	// return 0 if codec could be applied
	int setSend(int channel, const webrtc::CodecInst& codec,int payloadType,bool vad);

	// apply a receiving codec to the channel
	// return 0 if codec could be applied
	int setReceive(int channel, const webrtc::CodecInst& codec);

private:
	// the reference to the GIPS Codec sub-interface
	webrtc::VoECodec* voeCodec;

	std::map<int, webrtc::CodecInst*> codecMap;
};

} // namespace CSF

#endif
#endif /* WEBRTCAUDIOCODECSELECTOR_H_ */
