/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef MEDIA_SESSION_ERRORS_H_
#define MEDIA_SESSION_ERRORS_H_

namespace mozilla
{
enum MediaConduitErrorCode
{
kMediaConduitNoError = 0,              // 0 for Success,greater than 0 imples error
kMediaConduitSessionNotInited = 10100, // Session not initialized.10100 serves as
                                       // base for the conduit errors
kMediaConduitMalformedArgument,        // Malformed input to Conduit API
kMediaConduitCaptureError,             // WebRTC capture APIs failed
kMediaConduitInvalidSendCodec,         // Wrong Send codec
kMediaConduitInvalidReceiveCodec,      // Wrong Recv Codec
kMediaConduitCodecInUse,               // Already applied Codec
kMediaConduitInvalidRenderer,          // Null or Wrong Renderer object
kMediaConduitRendererFail,             // Add Render called multiple times
kMediaConduitSendingAlready,           // Engine already trasmitting
kMediaConduitReceivingAlready,         // Engine already receiving
kMediaConduitTransportRegistrationFail,// Null or wrong transport interface
kMediaConduitInvalidTransport,         // Null or wrong transport interface
kMediaConduitChannelError,             // Configuration Error
kMediaConduitSocketError,              // Media Engine transport socket error
kMediaConduitRTPRTCPModuleError,       // Couldn't start RTP/RTCP processing
kMediaConduitRTPProcessingFailed,      // Processing incoming RTP frame failed
kMediaConduitUnknownError,             // More information can be found in logs
kMediaConduitExternalRecordingError,   // Couldn't start external recording
kMediaConduitRecordingError,           // Runtime recording error
kMediaConduitExternalPlayoutError,     // Couldn't start external playout
kMediaConduitPlayoutError,             // Runtime playout error
kMediaConduitMTUError,                 // Can't set MTU
kMediaConduitRTCPStatusError,          // Can't set RTCP mode
kMediaConduitKeyFrameRequestError,     // Can't set KeyFrameRequest mode
kMediaConduitNACKStatusError           // Can't set NACK mode
};

}

#endif

