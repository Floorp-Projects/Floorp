// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ffi;

bitflags! {
    /// Some common layout definitions
    pub struct ChannelLayout: ffi::cubeb_channel_layout {
        ///
        const FRONT_LEFT = ffi::CHANNEL_FRONT_LEFT;
        const FRONT_RIGHT = ffi::CHANNEL_FRONT_RIGHT;
        const FRONT_CENTER = ffi::CHANNEL_FRONT_CENTER;
        const LOW_FREQUENCY = ffi::CHANNEL_LOW_FREQUENCY;
        const BACK_LEFT = ffi::CHANNEL_BACK_LEFT;
        const BACK_RIGHT = ffi::CHANNEL_BACK_RIGHT;
        const FRONT_LEFT_OF_CENTER = ffi::CHANNEL_FRONT_LEFT_OF_CENTER;
        const FRONT_RIGHT_OF_CENTER = ffi::CHANNEL_FRONT_RIGHT_OF_CENTER;
        const BACK_CENTER = ffi::CHANNEL_BACK_CENTER;
        const SIDE_LEFT = ffi::CHANNEL_SIDE_LEFT;
        const SIDE_RIGHT = ffi::CHANNEL_SIDE_RIGHT;
        const TOP_CENTER = ffi::CHANNEL_TOP_CENTER;
        const TOP_FRONT_LEFT = ffi::CHANNEL_TOP_FRONT_LEFT;
        const TOP_FRONT_CENTER = ffi::CHANNEL_TOP_FRONT_CENTER;
        const TOP_FRONT_RIGHT = ffi::CHANNEL_TOP_FRONT_RIGHT;
        const TOP_BACK_LEFT = ffi::CHANNEL_TOP_BACK_LEFT;
        const TOP_BACK_CENTER = ffi::CHANNEL_TOP_BACK_CENTER;
        const TOP_BACK_RIGHT = ffi::CHANNEL_TOP_BACK_RIGHT;
    }
}

macro_rules! bits {
    ($($x:ident => $y:ident),*) => {
        $(pub const $x: ChannelLayout = ChannelLayout::from_bits_truncate(ffi::$y);)*
    }
}

impl ChannelLayout {
    bits!(UNDEFINED => CUBEB_LAYOUT_UNDEFINED,
          MONO => CUBEB_LAYOUT_MONO,
          MONO_LFE => CUBEB_LAYOUT_MONO_LFE,
          STEREO => CUBEB_LAYOUT_STEREO,
          STEREO_LFE => CUBEB_LAYOUT_STEREO_LFE,
          _3F => CUBEB_LAYOUT_3F,
          _3F_LFE => CUBEB_LAYOUT_3F_LFE,
          _2F1 => CUBEB_LAYOUT_2F1,
          _2F1_LFE => CUBEB_LAYOUT_2F1_LFE,
          _3F1 => CUBEB_LAYOUT_3F1,
          _3F1_LFE => CUBEB_LAYOUT_3F1_LFE,
          _2F2 => CUBEB_LAYOUT_2F2,
          _2F2_LFE => CUBEB_LAYOUT_2F2_LFE,
          QUAD => CUBEB_LAYOUT_QUAD,
          QUAD_LFE => CUBEB_LAYOUT_QUAD_LFE,
          _3F2 => CUBEB_LAYOUT_3F2,
          _3F2_LFE => CUBEB_LAYOUT_3F2_LFE,
          _3F2_BACK => CUBEB_LAYOUT_3F2_BACK,
          _3F2_LFE_BACK => CUBEB_LAYOUT_3F2_LFE_BACK,
          _3F3R_LFE => CUBEB_LAYOUT_3F3R_LFE,
          _3F4_LFE => CUBEB_LAYOUT_3F4_LFE
    );
}

impl From<ffi::cubeb_channel> for ChannelLayout {
    fn from(x: ffi::cubeb_channel) -> Self {
        ChannelLayout::from_bits_truncate(x)
    }
}

impl From<ChannelLayout> for ffi::cubeb_channel {
    fn from(x: ChannelLayout) -> Self {
        x.bits()
    }
}

impl ChannelLayout {
    pub fn num_channels(&self) -> u32 {
        unsafe { ffi::cubeb_channel_layout_nb_channels(self.bits()) }
    }
}

#[cfg(test)]
mod test {
    use ffi;

    #[test]
    fn channel_layout_from_raw() {
        macro_rules! check(
            ($($raw:ident => $real:ident),*) => (
                $(let x = super::ChannelLayout::from(ffi::$raw);
                  assert_eq!(x, super::ChannelLayout::$real);
                )*
            ) );

        check!(CUBEB_LAYOUT_UNDEFINED => UNDEFINED,
               CUBEB_LAYOUT_MONO => MONO,
               CUBEB_LAYOUT_MONO_LFE => MONO_LFE,
               CUBEB_LAYOUT_STEREO => STEREO,
               CUBEB_LAYOUT_STEREO_LFE => STEREO_LFE,
               CUBEB_LAYOUT_3F => _3F,
               CUBEB_LAYOUT_3F_LFE => _3F_LFE,
               CUBEB_LAYOUT_2F1 => _2F1,
               CUBEB_LAYOUT_2F1_LFE => _2F1_LFE,
               CUBEB_LAYOUT_3F1 => _3F1,
               CUBEB_LAYOUT_3F1_LFE => _3F1_LFE,
               CUBEB_LAYOUT_2F2 => _2F2,
               CUBEB_LAYOUT_2F2_LFE => _2F2_LFE,
               CUBEB_LAYOUT_QUAD => QUAD,
               CUBEB_LAYOUT_QUAD_LFE => QUAD_LFE,
               CUBEB_LAYOUT_3F2 => _3F2,
               CUBEB_LAYOUT_3F2_LFE => _3F2_LFE,
               CUBEB_LAYOUT_3F2_BACK => _3F2_BACK,
               CUBEB_LAYOUT_3F2_LFE_BACK => _3F2_LFE_BACK,
               CUBEB_LAYOUT_3F3R_LFE => _3F3R_LFE,
               CUBEB_LAYOUT_3F4_LFE => _3F4_LFE);
    }

    #[test]
    fn channel_layout_into_raw() {
        macro_rules! check(
            ($($real:ident => $raw:ident),*) => (
                $(let x = super::ChannelLayout::$real;
                  let x: ffi::cubeb_channel_layout = x.into();
                  assert_eq!(x, ffi::$raw);
                )*
            ) );

        check!(UNDEFINED => CUBEB_LAYOUT_UNDEFINED,
               MONO => CUBEB_LAYOUT_MONO,
               MONO_LFE => CUBEB_LAYOUT_MONO_LFE,
               STEREO => CUBEB_LAYOUT_STEREO,
               STEREO_LFE => CUBEB_LAYOUT_STEREO_LFE,
               _3F => CUBEB_LAYOUT_3F,
               _3F_LFE => CUBEB_LAYOUT_3F_LFE,
               _2F1 => CUBEB_LAYOUT_2F1,
               _2F1_LFE=> CUBEB_LAYOUT_2F1_LFE,
               _3F1 => CUBEB_LAYOUT_3F1,
               _3F1_LFE =>  CUBEB_LAYOUT_3F1_LFE,
               _2F2 => CUBEB_LAYOUT_2F2,
               _2F2_LFE => CUBEB_LAYOUT_2F2_LFE,
               QUAD => CUBEB_LAYOUT_QUAD,
               QUAD_LFE => CUBEB_LAYOUT_QUAD_LFE,
               _3F2 => CUBEB_LAYOUT_3F2,
               _3F2_LFE => CUBEB_LAYOUT_3F2_LFE,
               _3F2_BACK => CUBEB_LAYOUT_3F2_BACK,
               _3F2_LFE_BACK => CUBEB_LAYOUT_3F2_LFE_BACK,
               _3F3R_LFE => CUBEB_LAYOUT_3F3R_LFE,
               _3F4_LFE => CUBEB_LAYOUT_3F4_LFE);
    }
}
