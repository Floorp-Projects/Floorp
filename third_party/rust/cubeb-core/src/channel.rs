// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ffi;

/// SMPTE channel layout (also known as wave order)
///
/// ---------------------------------------------------
/// Name          | Channels
/// ---------------------------------------------------
/// DUAL-MONO     | L   R
/// DUAL-MONO-LFE | L   R   LFE
/// MONO          | M
/// MONO-LFE      | M   LFE
/// STEREO        | L   R
/// STEREO-LFE    | L   R   LFE
/// 3F            | L   R   C
/// 3F-LFE        | L   R   C    LFE
/// 2F1           | L   R   S
/// 2F1-LFE       | L   R   LFE  S
/// 3F1           | L   R   C    S
/// 3F1-LFE       | L   R   C    LFE S
/// 2F2           | L   R   LS   RS
/// 2F2-LFE       | L   R   LFE  LS   RS
/// 3F2           | L   R   C    LS   RS
/// 3F2-LFE       | L   R   C    LFE  LS   RS
/// 3F3R-LFE      | L   R   C    LFE  RC   LS   RS
/// 3F4-LFE       | L   R   C    LFE  RLS  RRS  LS   RS
/// ---------------------------------------------------
///
/// The abbreviation of channel name is defined in following table:
/// ---------------------------
/// Abbr | Channel name
/// ---------------------------
/// M    | Mono
/// L    | Left
/// R    | Right
/// C    | Center
/// LS   | Left Surround
/// RS   | Right Surround
/// RLS  | Rear Left Surround
/// RC   | Rear Center
/// RRS  | Rear Right Surround
/// LFE  | Low Frequency Effects
/// ---------------------------
#[cfg_attr(target_env = "msvc", repr(i32))]
#[cfg_attr(not(target_env = "msvc"), repr(u32))]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ChannelLayout {
    Undefined = ffi::CUBEB_LAYOUT_UNDEFINED,
    DualMono = ffi::CUBEB_LAYOUT_DUAL_MONO,
    DualMonoLfe = ffi::CUBEB_LAYOUT_DUAL_MONO_LFE,
    Mono = ffi::CUBEB_LAYOUT_MONO,
    MonoLfe = ffi::CUBEB_LAYOUT_MONO_LFE,
    Stereo = ffi::CUBEB_LAYOUT_STEREO,
    StereoLfe = ffi::CUBEB_LAYOUT_STEREO_LFE,
    Layout3F = ffi::CUBEB_LAYOUT_3F,
    Layout3FLfe = ffi::CUBEB_LAYOUT_3F_LFE,
    Layout2F1 = ffi::CUBEB_LAYOUT_2F1,
    Layout2F1Lfe = ffi::CUBEB_LAYOUT_2F1_LFE,
    Layout3F1 = ffi::CUBEB_LAYOUT_3F1,
    Layout3F1Lfe = ffi::CUBEB_LAYOUT_3F1_LFE,
    Layout2F2 = ffi::CUBEB_LAYOUT_2F2,
    Layout2F2Lfe = ffi::CUBEB_LAYOUT_2F2_LFE,
    Layout3F2 = ffi::CUBEB_LAYOUT_3F2,
    Layout3F2Lfe = ffi::CUBEB_LAYOUT_3F2_LFE,
    Layout3F3RLfe = ffi::CUBEB_LAYOUT_3F3R_LFE,
    Layout3F4Lfe = ffi::CUBEB_LAYOUT_3F4_LFE,
}

impl From<ffi::cubeb_channel_layout> for ChannelLayout {
    fn from(x: ffi::cubeb_channel_layout) -> ChannelLayout {
        match x {
            ffi::CUBEB_LAYOUT_DUAL_MONO => ChannelLayout::DualMono,
            ffi::CUBEB_LAYOUT_DUAL_MONO_LFE => ChannelLayout::DualMonoLfe,
            ffi::CUBEB_LAYOUT_MONO => ChannelLayout::Mono,
            ffi::CUBEB_LAYOUT_MONO_LFE => ChannelLayout::MonoLfe,
            ffi::CUBEB_LAYOUT_STEREO => ChannelLayout::Stereo,
            ffi::CUBEB_LAYOUT_STEREO_LFE => ChannelLayout::StereoLfe,
            ffi::CUBEB_LAYOUT_3F => ChannelLayout::Layout3F,
            ffi::CUBEB_LAYOUT_3F_LFE => ChannelLayout::Layout3FLfe,
            ffi::CUBEB_LAYOUT_2F1 => ChannelLayout::Layout2F1,
            ffi::CUBEB_LAYOUT_2F1_LFE => ChannelLayout::Layout2F1Lfe,
            ffi::CUBEB_LAYOUT_3F1 => ChannelLayout::Layout3F1,
            ffi::CUBEB_LAYOUT_3F1_LFE => ChannelLayout::Layout3F1Lfe,
            ffi::CUBEB_LAYOUT_2F2 => ChannelLayout::Layout2F2,
            ffi::CUBEB_LAYOUT_2F2_LFE => ChannelLayout::Layout2F2Lfe,
            ffi::CUBEB_LAYOUT_3F2 => ChannelLayout::Layout3F2,
            ffi::CUBEB_LAYOUT_3F2_LFE => ChannelLayout::Layout3F2Lfe,
            ffi::CUBEB_LAYOUT_3F3R_LFE => ChannelLayout::Layout3F3RLfe,
            ffi::CUBEB_LAYOUT_3F4_LFE => ChannelLayout::Layout3F4Lfe,
            // TODO: Implement TryFrom
            // Everything else is just undefined.
            _ => ChannelLayout::Undefined,
        }
    }
}

impl Into<ffi::cubeb_channel_layout> for ChannelLayout {
    fn into(self) -> ffi::cubeb_channel_layout {
        self as ffi::cubeb_channel_layout
    }
}
