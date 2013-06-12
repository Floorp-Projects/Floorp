/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_ERRORS_H_
#define WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_ERRORS_H_

enum ViEErrors {
  // ViEBase.
  kViENotInitialized = 12000,        // Init has not been called successfully.
  kViEBaseVoEFailure,                // SetVoiceEngine. ViE failed to use VE instance. Check VE instance pointer.ConnectAudioChannel failed to set voice channel. Have SetVoiceEngine been called? Is the voice channel correct.
  kViEBaseChannelCreationFailed,     // CreateChannel.
  kViEBaseInvalidChannelId,          // The channel does not exist.
  kViEAPIDoesNotExist,               // Release called on Interface that has not been created.
  kViEBaseInvalidArgument,
  kViEBaseAlreadySending,            // StartSend called on channel that is already sending.
  kViEBaseNotSending,                // StopSend called on channel that is not sending.
  kViEBaseReceiveOnlyChannel,        // Can't send on a receive only channel.
  kViEBaseAlreadyReceiving,          // StartReceive called on channel that is already receiving.
  kViEBaseObserverAlreadyRegistered,  // RegisterObserver- an observer has already been set.
  kViEBaseObserverNotRegistered,     // DeregisterObserver - no observer has been registered.
  kViEBaseUnknownError,              // An unknown error has occurred. Check the log file.

  // ViECodec.
  kViECodecInvalidArgument  = 12100,    // Wrong input parameter to function.
  kViECodecObserverAlreadyRegistered,   // RegisterEncoderObserver, RegisterDecoderObserver.
  kViECodecObserverNotRegistered,       // DeregisterEncoderObserver, DeregisterDecoderObserver.
  kViECodecInvalidCodec,                // SetSendCodec,SetReceiveCodec- The codec structure is invalid.
  kViECodecInvalidChannelId,            // The channel does not exist.
  kViECodecInUse,                       // SetSendCodec- Can't change codec size or type when multiple channels use the same encoder.
  kViECodecReceiveOnlyChannel,          // SetSendCodec, can't change receive only channel.
  kViECodecUnknownError,                // An unknown error has occurred. Check the log file.

  // ViERender.
  kViERenderInvalidRenderId = 12200,  // No renderer with the ID exist. In AddRenderer - The render ID is invalid. No capture device, channel or file is allocated with that id.
  kViERenderAlreadyExists,            // AddRenderer: the renderer already exist.
  kViERenderInvalidFrameFormat,       // AddRender (external renderer). The user has requested a frame format that we don't support.
  kViERenderUnknownError,             // An unknown error has occurred. Check the log file.

  // ViECapture.
  kViECaptureDeviceAlreadyConnected = 12300,  // ConnectCaptureDevice - A capture device has already been connected to this video channel.
  kViECaptureDeviceDoesNotExist,              // No capture device exist with the provided capture id or unique name.
  kViECaptureDeviceInvalidChannelId,          // ConnectCaptureDevice, DisconnectCaptureDevice- No Channel exist with the provided channel id.
  kViECaptureDeviceNotConnected,              // DisconnectCaptureDevice- No capture device is connected to the channel.
  kViECaptureDeviceNotStarted,                // Stop- The capture device is not started.
  kViECaptureDeviceAlreadyStarted,            // Start- The capture device is already started.
  kViECaptureDeviceAlreadyAllocated,          // AllocateCaptureDevice The device is already allocated.
  kViECaptureDeviceMaxNoDevicesAllocated,     // AllocateCaptureDevice Max number of devices already allocated.
  kViECaptureObserverAlreadyRegistered,       // RegisterObserver- An observer is already registered. Need to deregister first.
  kViECaptureDeviceObserverNotRegistered,     // DeregisterObserver- No observer is registered.
  kViECaptureDeviceUnknownError,              // An unknown error has occurred. Check the log file.
  kViECaptureDeviceMacQtkitNotSupported,      // QTKit handles the capture devices automatically. Thus querying capture capabilities is not supported.

  // ViEFile.
  kViEFileInvalidChannelId  = 12400,  // No Channel exist with the provided channel id.
  kViEFileInvalidArgument,            // Incorrect input argument
  kViEFileAlreadyRecording,           // StartRecordOutgoingVideo - already recording channel
  kViEFileVoENotSet,                  // StartRecordOutgoingVideo. Failed to access voice engine. Has SetVoiceEngine been called?
  kViEFileNotRecording,               // StopRecordOutgoingVideo
  kViEFileMaxNoOfFilesOpened,         // StartPlayFile
  kViEFileNotPlaying,                 // StopPlayFile. The file with the provided id is not playing.
  kViEFileObserverAlreadyRegistered,  // RegisterObserver
  kViEFileObserverNotRegistered,      // DeregisterObserver
  kViEFileInputAlreadyConnected,      // SendFileOnChannel- the video channel already have a connected input.
  kViEFileNotConnected,               // StopSendFileOnChannel- No file is being sent on the channel.
  kViEFileVoEFailure,                 // SendFileOnChannel,StartPlayAudioLocally - failed to play audio stream
  kViEFileInvalidRenderId,            // SetRenderTimeoutImage and SetRenderStartImage: Renderer with the provided render id does not exist.
  kViEFileInvalidFile,                // Can't open the file with provided filename. Is the path and file format correct?
  kViEFileInvalidCapture,             // Can't use ViEPicture. Is the object correct?
  kViEFileSetRenderTimeoutError,      // SetRenderTimeoutImage- Please see log file.
  kViEFileSetStartImageError,         // SetRenderStartImage error. Please see log file.
  kViEFileUnknownError,               // An unknown error has occurred. Check the log file.

  // ViENetwork.
  kViENetworkInvalidChannelId = 12500,   // No Channel exist with the provided channel id.
  kViENetworkAlreadyReceiving,           // SetLocalReceiver: Can not change ports while receiving.
  kViENetworkLocalReceiverNotSet,        // GetLocalReceiver: SetLocalReceiver not called.
  kViENetworkAlreadySending,             // SetSendDestination
  kViENetworkDestinationNotSet,          // GetSendDestination
  kViENetworkInvalidArgument,            // GetLocalIP- Check function  arguments.
  kViENetworkSendCodecNotSet,            // SetSendGQoS- Need to set the send codec first.
  kViENetworkServiceTypeNotSupported,    // SetSendGQoS
  kViENetworkNotSupported,               // SetSendGQoS Not supported on this OS.
  kViENetworkObserverAlreadyRegistered,  // RegisterObserver
  kViENetworkObserverNotRegistered,      // SetPeriodicDeadOrAliveStatus - Need to call RegisterObserver first, DeregisterObserver if no observer is registered.
  kViENetworkUnknownError,               // An unknown error has occurred. Check the log file.

  // ViERTP_RTCP.
  kViERtpRtcpInvalidChannelId = 12600,   // No Channel exist with the provided channel id.
  kViERtpRtcpAlreadySending,             // The channel is already sending. Need to stop send before calling this API.
  kViERtpRtcpNotSending,                 // The channel needs to be sending in order for this function to work.
  kViERtpRtcpRtcpDisabled,               // Functions failed because RTCP is disabled.
  kViERtpRtcpObserverAlreadyRegistered,  // An observer is already registered. Need to deregister the old first.
  kViERtpRtcpObserverNotRegistered,      // No observer registered.
  kViERtpRtcpUnknownError,               // An unknown error has occurred. Check the log file.

  // ViEEncryption.
  kViEEncryptionInvalidChannelId = 12700,  // Channel id does not exist.
  kViEEncryptionInvalidSrtpParameter,      // DEPRECATED
  kViEEncryptionSrtpNotSupported,          // DEPRECATED
  kViEEncryptionUnknownError,              // An unknown error has occurred. Check the log file.

  // ViEImageProcess.
  kViEImageProcessInvalidChannelId  = 12800,  // No Channel exist with the provided channel id.
  kViEImageProcessInvalidCaptureId,          // No capture device exist with the provided capture id.
  kViEImageProcessFilterExists,              // RegisterCaptureEffectFilter,RegisterSendEffectFilter,RegisterRenderEffectFilter - Effect filter already registered.
  kViEImageProcessFilterDoesNotExist,        // DeRegisterCaptureEffectFilter,DeRegisterSendEffectFilter,DeRegisterRenderEffectFilter - Effect filter not registered.
  kViEImageProcessAlreadyEnabled,            // EnableDeflickering,EnableDenoising,EnableColorEnhancement- Function already enabled.
  kViEImageProcessAlreadyDisabled,           // EnableDeflickering,EnableDenoising,EnableColorEnhancement- Function already disabled.
  kViEImageProcessUnknownError               // An unknown error has occurred. Check the log file.
};

#endif  // WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_ERRORS_H_
