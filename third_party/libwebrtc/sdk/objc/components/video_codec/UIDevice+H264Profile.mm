/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "UIDevice+H264Profile.h"
#import "helpers/UIDevice+RTCDevice.h"

#include <algorithm>

namespace {

using namespace webrtc;

struct SupportedH264Profile {
  const char* machineName;
  const H264ProfileLevelId profile;
};

constexpr SupportedH264Profile kH264MaxSupportedProfiles[] = {
    // iPhones with at least iOS 12
    {"iPhone15,3", /* https://support.apple.com/kb/SP876 iPhone 14 Pro Max */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone15,2", /* https://support.apple.com/kb/SP875 iPhone 14 Pro */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},

    {"iPhone14,8", /* https://support.apple.com/kb/SP874 iPhone 14 Plus */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone14,7", /* https://support.apple.com/kb/SP873 iPhone 14 */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone14,6", /* https://support.apple.com/kb/SP867 iPhone SE 3rd Gen */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone14,5", /* https://support.apple.com/kb/SP851 iPhone 13 */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone14,4", /* https://support.apple.com/kb/SP847 Phone 13 Mini */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone14,3", /* https://support.apple.com/kb/SP848 iPhone 13 Pro Max */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone14,2", /* https://support.apple.com/kb/SP852 iPhone 13 Pro */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},

    {"iPhone13,4", /* https://support.apple.com/kb/SP832 iPhone 12 Pro Max */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone13,3", /* https://support.apple.com/kb/SP831 iPhone 12 Pro */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone13,2", /* https://support.apple.com/kb/SP830 iPhone 12 */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone13,1", /* https://support.apple.com/kb/SP829 iPhone 12 mini */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},

    {"iPhone12,8", /* https://support.apple.com/kb/SP820 iPhone SE (2nd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone12,5", /* https://support.apple.com/kb/SP806 iPhone 11 Pro Max */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone12,3", /* https://support.apple.com/kb/SP805 iPhone 11 pro */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone12,1", /* https://support.apple.com/kb/SP804 iPhone 11 */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},

    {"iPhone11,8", /* https://support.apple.com/kb/SP781 iPhone XR */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone11,6", /* https://support.apple.com/kb/SP780 iPhone XS Max */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone11,4", /* https://support.apple.com/kb/SP780 iPhone XS Max */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone11,2", /* https://support.apple.com/kb/SP779 iPhone XS */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},

    {"iPhone10,6", /* https://support.apple.com/kb/SP770 iPhone X */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone10,5", /* https://support.apple.com/kb/SP768 iPhone 8 Plus */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone10,4", /* https://support.apple.com/kb/SP767 iPhone 8 */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone10,3", /* https://support.apple.com/kb/SP770 iPhone X */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone10,2", /* https://support.apple.com/kb/SP768 iPhone 8 Plus */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPhone10,1", /* https://support.apple.com/kb/SP767 iPhone 8 */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},

    {"iPhone9,4", /* https://support.apple.com/kb/SP744 iPhone 7 Plus */
     {H264Profile::kProfileHigh, H264Level::kLevel5_1}},
    {"iPhone9,3", /* https://support.apple.com/kb/SP743 iPhone 7 */
     {H264Profile::kProfileHigh, H264Level::kLevel5_1}},
    {"iPhone9,2", /* https://support.apple.com/kb/SP744 iPhone 7 Plus */
     {H264Profile::kProfileHigh, H264Level::kLevel5_1}},
    {"iPhone9,1", /* https://support.apple.com/kb/SP743 iPhone 7 */
     {H264Profile::kProfileHigh, H264Level::kLevel5_1}},

    {"iPhone8,4", /* https://support.apple.com/kb/SP738 iPhone SE */
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},
    {"iPhone8,2", /* https://support.apple.com/kb/SP727 iPhone 6s Plus */
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},
    {"iPhone8,1", /* https://support.apple.com/kb/SP726 iPhone 6s */
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},

    {"iPhone7,2", /* https://support.apple.com/kb/SP705 iPhone 6 */
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},
    {"iPhone7,1", /* https://support.apple.com/kb/SP706 iPhone 6 Plus */
     {H264Profile::kProfileHigh, H264Level::kLevel4_2}},

    {"iPhone6,2", /* https://support.apple.com/kb/SP685 iPhone 5s */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPhone6,1", /* https://support.apple.com/kb/SP685 iPhone 5s */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},

    // iPods with at least iOS 12
    {"iPod9,1", /* https://support.apple.com/kb/SP796 iPod touch (7th generation) */
     {H264Profile::kProfileMain, H264Level::kLevel4_1}},
    {"iPod7,1", /* https://support.apple.com/kb/SP720 iPod touch (6th generation) */
     {H264Profile::kProfileMain, H264Level::kLevel4_1}},

    // iPads with at least iOS 12
    {"iPad14,6", /* https://support.apple.com/kb/SP883 iPad Pro 12.9-inch (6th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad14,5", /* https://support.apple.com/kb/SP883 iPad Pro 12.9-inch (6th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad14,4", /* https://support.apple.com/kb/SP882 iPad Pro 11-inch (4th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad14,3", /* https://support.apple.com/kb/SP882 iPad Pro 11-inch (4th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad14,2", /* https://support.apple.com/kb/SP850 iPad mini (6th generation)  */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad14,1", /* https://support.apple.com/kb/SP850 iPad mini (6th generation)  */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},

    {"iPad13,19", /* https://support.apple.com/kb/SP884 iPad (10th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,18", /* https://support.apple.com/kb/SP884 iPad (10th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,17", /* https://support.apple.com/kb/SP866 iPad Air (5th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,16", /* https://support.apple.com/kb/SP866 iPad Air (5th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,11", /* https://support.apple.com/kb/SP844 iPad Pro, 12.9-inch (5th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,10", /* https://support.apple.com/kb/SP844 iPad Pro, 12.9-inch (5th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,9", /* https://support.apple.com/kb/SP844 iPad Pro, 12.9-inch (5th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,8", /* https://support.apple.com/kb/SP844 iPad Pro, 12.9-inch (5th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,7", /* https://support.apple.com/kb/SP843 iPad Pro, 11-inch (3rd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,6", /* https://support.apple.com/kb/SP843 iPad Pro, 11-inch (3rd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,5", /* https://support.apple.com/kb/SP843 iPad Pro, 11-inch (3rd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,4", /* https://support.apple.com/kb/SP843 iPad Pro, 11-inch (3rd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,2", /* https://support.apple.com/kb/SP828 iPad Air (4th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad13,1", /* https://support.apple.com/kb/SP828 iPad Air (4th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},

    {"iPad12,2", /* https://support.apple.com/kb/SP849 iPad (9th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad12,1", /* https://support.apple.com/kb/SP849 iPad (9th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},

    {"iPad11,7", /* https://support.apple.com/kb/SP822 iPad (8th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad11,6", /* https://support.apple.com/kb/SP822 iPad (8th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad11,4", /* https://support.apple.com/kb/SP787 iPad Air (3rd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad11,3", /* https://support.apple.com/kb/SP787 iPad Air (3rd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad11,2", /* https://support.apple.com/kb/SP788 iPad mini (5th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad11,1", /* https://support.apple.com/kb/SP788 iPad mini (5th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},

    {"iPad8,12", /* https://support.apple.com/kb/SP815 iPad Pro 12.9-inch (4th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad8,11", /* https://support.apple.com/kb/SP815 iPad Pro 12.9-inch (4th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad8,10", /* https://support.apple.com/kb/SP814 iPad Pro 11-inch (2nd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad8,9", /* https://support.apple.com/kb/SP814 iPad Pro 11-inch (2nd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad8,8", /* https://support.apple.com/kb/SP785 iPad Pro 12.9-inch (3rd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad8,7", /* https://support.apple.com/kb/SP785 iPad Pro 12.9-inch (3rd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad8,6", /* https://support.apple.com/kb/SP785 iPad Pro 12.9-inch (3rd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad8,5", /* https://support.apple.com/kb/SP785 iPad Pro 12.9-inch (3rd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad8,4", /* https://support.apple.com/kb/SP784 iPad Pro 11-inch (1st generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad8,3", /* https://support.apple.com/kb/SP784 iPad Pro 11-inch (1st generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad8,2", /* https://support.apple.com/kb/SP784 iPad Pro 11-inch (1st generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},
    {"iPad8,1", /* https://support.apple.com/kb/SP784 iPad Pro 11-inch (1st generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_2}},

    {"iPad7,12", /* https://support.apple.com/kb/SP807 iPad (7th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad7,11", /* https://support.apple.com/kb/SP807 iPad (7th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad7,6", /* https://support.apple.com/kb/SP774 iPad (6th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad7,5", /* https://support.apple.com/kb/SP774 iPad (6th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad7,4", /* https://support.apple.com/kb/SP762 iPad Pro (10.5-inch) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_1}},
    {"iPad7,3", /* https://support.apple.com/kb/SP762 iPad Pro (10.5-inch) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_1}},
    {"iPad7,2", /* https://support.apple.com/kb/SP761 iPad Pro (12.9-inch) (2nd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_1}},
    {"iPad7,1", /* https://support.apple.com/kb/SP761 iPad Pro (12.9-inch) (2nd generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_1}},

    {"iPad6,12", /* https://support.apple.com/kb/SP751 iPad (5th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad6,11", /* https://support.apple.com/kb/SP751 iPad (5th generation) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad6,8", /* https://support.apple.com/kb/SP723 iPad Pro (12.9-inch) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad6,7", /* https://support.apple.com/kb/SP723 iPad Pro (12.9-inch) */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad6,4", /* https://support.apple.com/kb/SP739 iPad Pro (9.7-inch) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_1}},
    {"iPad6,3", /* https://support.apple.com/kb/SP739 iPad Pro (9.7-inch) */
     {H264Profile::kProfileHigh, H264Level::kLevel5_1}},

    {"iPad5,4", /* https://support.apple.com/kb/sp708 iPad Air 2 */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad5,3", /* https://support.apple.com/kb/sp708 iPad Air 2 */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad5,2", /* https://support.apple.com/kb/sp725 iPad mini 4 */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad5,1", /* https://support.apple.com/kb/sp725 iPad mini 4 */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},

    {"iPad4,9", /* https://support.apple.com/kb/sp709 iPad mini 3 */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad4,8", /* https://support.apple.com/kb/sp709 iPad mini 3 */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad4,7", /* https://support.apple.com/kb/sp709 iPad mini 3 */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad4,6", /* https://support.apple.com/kb/sp693 iPad mini 2 with Retina display */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad4,5", /* https://support.apple.com/kb/sp693 iPad mini 2 with Retina display */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad4,4", /* https://support.apple.com/kb/sp693 iPad mini 2 with Retina display */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad4,3", /* https://support.apple.com/kb/SP692 iPad Air */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad4,2", /* https://support.apple.com/kb/SP692 iPad Air */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
    {"iPad4,1", /* https://support.apple.com/kb/SP692 iPad Air */
     {H264Profile::kProfileHigh, H264Level::kLevel4_1}},
};

absl::optional<H264ProfileLevelId> FindMaxSupportedProfileForDevice(NSString* machineName) {
  const auto* result =
      std::find_if(std::begin(kH264MaxSupportedProfiles),
                   std::end(kH264MaxSupportedProfiles),
                   [machineName](const SupportedH264Profile& supportedProfile) {
                     return [machineName isEqualToString:@(supportedProfile.machineName)];
                   });
  if (result != std::end(kH264MaxSupportedProfiles)) {
    return result->profile;
  }
  if ([machineName hasPrefix:@"iPhone"] || [machineName hasPrefix:@"iPad"]) {
    H264ProfileLevelId fallbackProfile{H264Profile::kProfileHigh, H264Level::kLevel4_1};
    return fallbackProfile;
  }
  if ([machineName hasPrefix:@"iPod"]) {
    H264ProfileLevelId fallbackProfile{H264Profile::kProfileMain, H264Level::kLevel4_1};
    return fallbackProfile;
  }
  return absl::nullopt;
}

}  // namespace

@implementation UIDevice (H264Profile)

+ (absl::optional<webrtc::H264ProfileLevelId>)maxSupportedH264Profile {
  return FindMaxSupportedProfileForDevice([UIDevice machineName]);
}

@end
