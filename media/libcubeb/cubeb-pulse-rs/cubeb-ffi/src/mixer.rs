// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ::*;

static CHANNEL_LAYOUT_UNDEFINED: &'static [Channel] = &[CHANNEL_INVALID];
static CHANNEL_LAYOUT_DUAL_MONO: &'static [Channel] = &[CHANNEL_LEFT, CHANNEL_RIGHT];
static CHANNEL_LAYOUT_DUAL_MONO_LFE: &'static [Channel] = &[CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_LFE];
static CHANNEL_LAYOUT_MONO: &'static [Channel] = &[CHANNEL_MONO];
static CHANNEL_LAYOUT_MONO_LFE: &'static [Channel] = &[CHANNEL_MONO, CHANNEL_LFE];
static CHANNEL_LAYOUT_STEREO: &'static [Channel] = &[CHANNEL_LEFT, CHANNEL_RIGHT];
static CHANNEL_LAYOUT_STEREO_LFE: &'static [Channel] = &[CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_LFE];
static CHANNEL_LAYOUT_3F: &'static [Channel] = &[CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER];
static CHANNEL_LAYOUT_3FLFE: &'static [Channel] = &[CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_LFE];
static CHANNEL_LAYOUT_2F1: &'static [Channel] = &[CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_RCENTER];
static CHANNEL_LAYOUT_2F1LFE: &'static [Channel] = &[CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_LFE, CHANNEL_RCENTER];
static CHANNEL_LAYOUT_3F1: &'static [Channel] = &[CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_RCENTER];
static CHANNEL_LAYOUT_3F1LFE: &'static [Channel] = &[CHANNEL_LEFT,
                                                     CHANNEL_RIGHT,
                                                     CHANNEL_CENTER,
                                                     CHANNEL_LFE,
                                                     CHANNEL_RCENTER];
static CHANNEL_LAYOUT_2F2: &'static [Channel] = &[CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_LS, CHANNEL_RS];
static CHANNEL_LAYOUT_2F2LFE: &'static [Channel] = &[CHANNEL_LEFT,
                                                     CHANNEL_RIGHT,
                                                     CHANNEL_LFE,
                                                     CHANNEL_LS,
                                                     CHANNEL_RS];
static CHANNEL_LAYOUT_3F2: &'static [Channel] = &[CHANNEL_LEFT,
                                                  CHANNEL_RIGHT,
                                                  CHANNEL_CENTER,
                                                  CHANNEL_LS,
                                                  CHANNEL_RS];
static CHANNEL_LAYOUT_3F2LFE: &'static [Channel] = &[CHANNEL_LEFT,
                                                     CHANNEL_RIGHT,
                                                     CHANNEL_CENTER,
                                                     CHANNEL_LFE,
                                                     CHANNEL_LS,
                                                     CHANNEL_RS];
static CHANNEL_LAYOUT_3F3RLFE: &'static [Channel] = &[CHANNEL_LEFT,
                                                      CHANNEL_RIGHT,
                                                      CHANNEL_CENTER,
                                                      CHANNEL_LFE,
                                                      CHANNEL_RCENTER,
                                                      CHANNEL_LS,
                                                      CHANNEL_RS];
static CHANNEL_LAYOUT_3F4LFE: &'static [Channel] = &[CHANNEL_LEFT,
                                                     CHANNEL_RIGHT,
                                                     CHANNEL_CENTER,
                                                     CHANNEL_LFE,
                                                     CHANNEL_RLS,
                                                     CHANNEL_RRS,
                                                     CHANNEL_LS,
                                                     CHANNEL_RS];

pub fn channel_index_to_order(layout: ChannelLayout) -> &'static [Channel] {
    match layout {
        LAYOUT_DUAL_MONO => CHANNEL_LAYOUT_DUAL_MONO,
        LAYOUT_DUAL_MONO_LFE => CHANNEL_LAYOUT_DUAL_MONO_LFE,
        LAYOUT_MONO => CHANNEL_LAYOUT_MONO,
        LAYOUT_MONO_LFE => CHANNEL_LAYOUT_MONO_LFE,
        LAYOUT_STEREO => CHANNEL_LAYOUT_STEREO,
        LAYOUT_STEREO_LFE => CHANNEL_LAYOUT_STEREO_LFE,
        LAYOUT_3F => CHANNEL_LAYOUT_3F,
        LAYOUT_3F_LFE => CHANNEL_LAYOUT_3FLFE,
        LAYOUT_2F1 => CHANNEL_LAYOUT_2F1,
        LAYOUT_2F1_LFE => CHANNEL_LAYOUT_2F1LFE,
        LAYOUT_3F1 => CHANNEL_LAYOUT_3F1,
        LAYOUT_3F1_LFE => CHANNEL_LAYOUT_3F1LFE,
        LAYOUT_2F2 => CHANNEL_LAYOUT_2F2,
        LAYOUT_2F2_LFE => CHANNEL_LAYOUT_2F2LFE,
        LAYOUT_3F2 => CHANNEL_LAYOUT_3F2,
        LAYOUT_3F2_LFE => CHANNEL_LAYOUT_3F2LFE,
        LAYOUT_3F3R_LFE => CHANNEL_LAYOUT_3F3RLFE,
        LAYOUT_3F4_LFE => CHANNEL_LAYOUT_3F4LFE,
        _ => CHANNEL_LAYOUT_UNDEFINED,
    }
}
