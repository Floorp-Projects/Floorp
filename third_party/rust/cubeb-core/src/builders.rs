// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ffi;
use {ChannelLayout, SampleFormat, StreamParams, StreamPrefs};

#[derive(Debug)]
pub struct StreamParamsBuilder(ffi::cubeb_stream_params);

impl Default for StreamParamsBuilder {
    fn default() -> Self {
        StreamParamsBuilder(ffi::cubeb_stream_params {
            format: ffi::CUBEB_SAMPLE_S16NE,
            ..Default::default()
        })
    }
}

impl StreamParamsBuilder {
    pub fn new() -> Self {
        Default::default()
    }

    pub fn format(mut self, format: SampleFormat) -> Self {
        self.0.format = format.into();
        self
    }

    pub fn rate(mut self, rate: u32) -> Self {
        self.0.rate = rate;
        self
    }

    pub fn channels(mut self, channels: u32) -> Self {
        self.0.channels = channels;
        self
    }

    pub fn layout(mut self, layout: ChannelLayout) -> Self {
        self.0.layout = layout.into();
        self
    }

    pub fn prefs(mut self, prefs: StreamPrefs) -> Self {
        self.0.prefs = prefs.bits();
        self
    }

    pub fn take(&self) -> StreamParams {
        StreamParams::from(self.0)
    }
}

#[cfg(test)]
mod tests {
    use SampleFormat;
    use {ffi, StreamParamsBuilder, StreamPrefs};

    #[test]
    fn stream_params_builder_channels() {
        let params = StreamParamsBuilder::new().channels(2).take();
        assert_eq!(params.channels(), 2);
    }

    #[test]
    fn stream_params_builder_format() {
        macro_rules! check(
            ($($real:ident),*) => (
                $(let params = StreamParamsBuilder::new()
                  .format(super::SampleFormat::$real)
                  .take();
                assert_eq!(params.format(), super::SampleFormat::$real);
                )*
            ) );

        check!(S16LE, S16BE, Float32LE, Float32BE);
    }

    #[test]
    fn stream_params_builder_format_native_endian() {
        let params = StreamParamsBuilder::new()
            .format(SampleFormat::S16NE)
            .take();
        assert_eq!(
            params.format(),
            if cfg!(target_endian = "little") {
                super::SampleFormat::S16LE
            } else {
                super::SampleFormat::S16BE
            }
        );

        let params = StreamParamsBuilder::new()
            .format(SampleFormat::Float32NE)
            .take();
        assert_eq!(
            params.format(),
            if cfg!(target_endian = "little") {
                SampleFormat::Float32LE
            } else {
                SampleFormat::Float32BE
            }
        );
    }

    #[test]
    fn stream_params_builder_layout() {
        macro_rules! check(
            ($($real:ident),*) => (
                $(let params = StreamParamsBuilder::new()
                  .layout(super::ChannelLayout::$real)
                  .take();
                assert_eq!(params.layout(), super::ChannelLayout::$real);
                )*
            ) );

        check!(
            UNDEFINED,
            MONO,
            MONO_LFE,
            STEREO,
            STEREO_LFE,
            _3F,
            _3F_LFE,
            _2F1,
            _2F1_LFE,
            _3F1,
            _3F1_LFE,
            _2F2,
            _2F2_LFE,
            QUAD,
            QUAD_LFE,
            _3F2,
            _3F2_LFE,
            _3F2_BACK,
            _3F2_LFE_BACK,
            _3F3R_LFE,
            _3F4_LFE
        );
    }

    #[test]
    fn stream_params_builder_rate() {
        let params = StreamParamsBuilder::new().rate(44100).take();
        assert_eq!(params.rate(), 44100);
    }

    #[test]
    fn stream_params_builder_to_raw_channels() {
        let params = StreamParamsBuilder::new().channels(2).take();
        let raw = unsafe { &*params.as_ptr() };
        assert_eq!(raw.channels, 2);
    }

    #[test]
    fn stream_params_builder_to_raw_format() {
        macro_rules! check(
            ($($real:ident => $raw:ident),*) => (
                $(let params = super::StreamParamsBuilder::new()
                  .format(SampleFormat::$real)
                  .take();
                  let raw = unsafe { &*params.as_ptr() };
                  assert_eq!(raw.format, ffi::$raw);
                )*
            ) );

        check!(S16LE => CUBEB_SAMPLE_S16LE,
               S16BE => CUBEB_SAMPLE_S16BE,
               Float32LE => CUBEB_SAMPLE_FLOAT32LE,
               Float32BE => CUBEB_SAMPLE_FLOAT32BE);
    }

    #[test]
    fn stream_params_builder_format_to_raw_native_endian() {
        let params = StreamParamsBuilder::new()
            .format(SampleFormat::S16NE)
            .take();
        let raw = unsafe { &*params.as_ptr() };
        assert_eq!(
            raw.format,
            if cfg!(target_endian = "little") {
                ffi::CUBEB_SAMPLE_S16LE
            } else {
                ffi::CUBEB_SAMPLE_S16BE
            }
        );

        let params = StreamParamsBuilder::new()
            .format(SampleFormat::Float32NE)
            .take();
        let raw = unsafe { &*params.as_ptr() };
        assert_eq!(
            raw.format,
            if cfg!(target_endian = "little") {
                ffi::CUBEB_SAMPLE_FLOAT32LE
            } else {
                ffi::CUBEB_SAMPLE_FLOAT32BE
            }
        );
    }

    #[test]
    fn stream_params_builder_to_raw_layout() {
        macro_rules! check(
            ($($real:ident => $raw:ident),*) => (
                $(let params = super::StreamParamsBuilder::new()
                  .layout(super::ChannelLayout::$real)
                  .take();
                  let raw = unsafe { &*params.as_ptr() };
                  assert_eq!(raw.layout, ffi::$raw);
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

    #[test]
    fn stream_params_builder_to_raw_rate() {
        let params = StreamParamsBuilder::new().rate(44100).take();
        let raw = unsafe { &*params.as_ptr() };
        assert_eq!(raw.rate, 44100);
    }

    #[test]
    fn stream_params_builder_prefs_default() {
        let params = StreamParamsBuilder::new().take();
        assert_eq!(params.prefs(), StreamPrefs::NONE);
    }

    #[test]
    fn stream_params_builder_prefs() {
        let params = StreamParamsBuilder::new()
            .prefs(StreamPrefs::LOOPBACK)
            .take();
        assert_eq!(params.prefs(), StreamPrefs::LOOPBACK);
    }
}
