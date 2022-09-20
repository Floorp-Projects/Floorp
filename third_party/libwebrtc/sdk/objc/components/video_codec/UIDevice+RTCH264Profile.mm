/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "UIDevice+RTCH264Profile.h"
#import "helpers/UIDevice+RTCDevice.h"

#include <algorithm>

namespace {

using namespace webrtc;

struct SupportedH264Profile {
  const RTCDeviceType deviceType;
  const H264ProfileLevelId profile;
};

constexpr SupportedH264Profile kH264MaxSupportedProfiles[] = {
    // iPhones with at least iOS 9
    {RTCDeviceTypeIPhone13ProMax,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP848
    {RTCDeviceTypeIPhone13Pro,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP852
    {RTCDeviceTypeIPhone13,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP851
    {RTCDeviceTypeIPhone13Mini,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP847
    {RTCDeviceTypeIPhoneSE2Gen,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP820
    {RTCDeviceTypeIPhone12ProMax,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP832
    {RTCDeviceTypeIPhone12Pro,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP831
    {RTCDeviceTypeIPhone12,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP830
    {RTCDeviceTypeIPhone12Mini,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP829
    {RTCDeviceTypeIPhone11ProMax,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP806
    {RTCDeviceTypeIPhone11Pro,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP805
    {RTCDeviceTypeIPhone11,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP804
    {RTCDeviceTypeIPhoneXS,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP779
    {RTCDeviceTypeIPhoneXSMax,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP780
    {RTCDeviceTypeIPhoneXR,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP781
    {RTCDeviceTypeIPhoneX,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP770
    {RTCDeviceTypeIPhone8,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP767
    {RTCDeviceTypeIPhone8Plus,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP768
    {RTCDeviceTypeIPhone7,
     {H264Profile::kProfileHigh, H264Level::kLevel5_1}},  // https://support.apple.com/kb/SP743
    {RTCDeviceTypeIPhone7Plus,
     {H264Profile::kProfileHigh, H264Level::kLevel5_1}},  // https://support.apple.com/kb/SP744
    {RTCDeviceTypeIPhoneSE,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP738
    {RTCDeviceTypeIPhone6S,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP726
    {RTCDeviceTypeIPhone6SPlus,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP727
    {RTCDeviceTypeIPhone6,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP705
    {RTCDeviceTypeIPhone6Plus,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP706
    {RTCDeviceTypeIPhone5SGSM,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP685
    {RTCDeviceTypeIPhone5SGSM_CDMA,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP685
    {RTCDeviceTypeIPhone5GSM,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP655
    {RTCDeviceTypeIPhone5GSM_CDMA,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP655
    {RTCDeviceTypeIPhone5CGSM,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP684
    {RTCDeviceTypeIPhone5CGSM_CDMA,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP684
    {RTCDeviceTypeIPhone4S,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP643

    // iPods with at least iOS 9
    {RTCDeviceTypeIPodTouch7G,
     {H264Profile::kProfileMain, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP796
    {RTCDeviceTypeIPodTouch6G,
     {H264Profile::kProfileMain, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP720
    {RTCDeviceTypeIPodTouch5G,
     {H264Profile::kProfileMain, H264Level::kLevel3_1}},  // https://support.apple.com/kb/SP657

    // iPads with at least iOS 9
    {RTCDeviceTypeIPadAir4Gen,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP828
    {RTCDeviceTypeIPad8,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP822
    {RTCDeviceTypeIPadPro4Gen12Inch,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP815
    {RTCDeviceTypeIPadPro4Gen11Inch,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP814
    {RTCDeviceTypeIPadAir3Gen,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP787
    {RTCDeviceTypeIPadMini5Gen,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP788
    {RTCDeviceTypeIPadPro3Gen12Inch,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP785
    {RTCDeviceTypeIPadPro3Gen11Inch,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP784
    {RTCDeviceTypeIPad7Gen10Inch,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP807
    {RTCDeviceTypeIPad2Wifi,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP622
    {RTCDeviceTypeIPad2GSM,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP622
    {RTCDeviceTypeIPad2CDMA,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP622
    {RTCDeviceTypeIPad2Wifi2,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP622
    {RTCDeviceTypeIPadMiniWifi,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP661
    {RTCDeviceTypeIPadMiniGSM,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP661
    {RTCDeviceTypeIPadMiniGSM_CDMA,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP661
    {RTCDeviceTypeIPad3Wifi,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP647
    {RTCDeviceTypeIPad3GSM_CDMA,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP647
    {RTCDeviceTypeIPad3GSM,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP647
    {RTCDeviceTypeIPad4Wifi,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP662
    {RTCDeviceTypeIPad4GSM,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP662
    {RTCDeviceTypeIPad4GSM_CDMA,
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},  // https://support.apple.com/kb/SP662
    {RTCDeviceTypeIPad5,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP751
    {RTCDeviceTypeIPad6,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP774
    {RTCDeviceTypeIPadAirWifi,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP692
    {RTCDeviceTypeIPadAirCellular,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP692
    {RTCDeviceTypeIPadAirWifiCellular,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP692
    {RTCDeviceTypeIPadAir2,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP708
    {RTCDeviceTypeIPadMini2GWifi,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP693
    {RTCDeviceTypeIPadMini2GCellular,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP693
    {RTCDeviceTypeIPadMini2GWifiCellular,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP693
    {RTCDeviceTypeIPadMini3,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP709
    {RTCDeviceTypeIPadMini4,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP725
    {RTCDeviceTypeIPadPro9Inch,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP739
    {RTCDeviceTypeIPadPro12Inch,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/sp723
    {RTCDeviceTypeIPadPro12Inch2,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP761
    {RTCDeviceTypeIPadPro10Inch,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP762
    {RTCDeviceTypeIPadMini6,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP850
    {RTCDeviceTypeIPad9,
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},  // https://support.apple.com/kb/SP849
    {RTCDeviceTypeIPadPro5Gen12Inch,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP844
    {RTCDeviceTypeIPadPro5Gen11Inch,
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},  // https://support.apple.com/kb/SP843
};

absl::optional<H264ProfileLevelId> FindMaxSupportedProfileForDevice(RTCDeviceType deviceType) {
  const auto* result = std::find_if(std::begin(kH264MaxSupportedProfiles),
                                    std::end(kH264MaxSupportedProfiles),
                                    [deviceType](const SupportedH264Profile& supportedProfile) {
                                      return supportedProfile.deviceType == deviceType;
                                    });
  if (result != std::end(kH264MaxSupportedProfiles)) {
    return result->profile;
  }
  return absl::nullopt;
}

}  // namespace

@implementation UIDevice (RTCH264Profile)

+ (absl::optional<webrtc::H264ProfileLevelId>)rtc_maxSupportedH264Profile {
  return FindMaxSupportedProfileForDevice([self deviceType]);
}

@end
