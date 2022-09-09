use cubeb_backend::{ChannelLayout, SampleFormat};
use std::mem;
use std::os::raw::{c_int, c_void};

extern crate audio_mixer;
pub use self::audio_mixer::Channel;

const CHANNEL_OERDER: [audio_mixer::Channel; audio_mixer::Channel::count()] = [
    audio_mixer::Channel::FrontLeft,
    audio_mixer::Channel::FrontRight,
    audio_mixer::Channel::FrontCenter,
    audio_mixer::Channel::LowFrequency,
    audio_mixer::Channel::BackLeft,
    audio_mixer::Channel::BackRight,
    audio_mixer::Channel::FrontLeftOfCenter,
    audio_mixer::Channel::FrontRightOfCenter,
    audio_mixer::Channel::BackCenter,
    audio_mixer::Channel::SideLeft,
    audio_mixer::Channel::SideRight,
    audio_mixer::Channel::TopCenter,
    audio_mixer::Channel::TopFrontLeft,
    audio_mixer::Channel::TopFrontCenter,
    audio_mixer::Channel::TopFrontRight,
    audio_mixer::Channel::TopBackLeft,
    audio_mixer::Channel::TopBackCenter,
    audio_mixer::Channel::TopBackRight,
    audio_mixer::Channel::Silence,
];

pub fn get_channel_order(channel_layout: ChannelLayout) -> Vec<audio_mixer::Channel> {
    let mut map = channel_layout.bits();
    let mut order = Vec::new();
    let mut channel_index: usize = 0;
    while map != 0 {
        if map & 1 == 1 {
            order.push(CHANNEL_OERDER[channel_index]);
        }
        map >>= 1;
        channel_index += 1;
    }
    order
}

fn get_default_channel_order(channel_count: usize) -> Vec<audio_mixer::Channel> {
    assert_ne!(channel_count, 0);
    let mut channels = Vec::with_capacity(channel_count);
    for channel in CHANNEL_OERDER.iter().take(channel_count) {
        channels.push(*channel);
    }

    if channel_count > CHANNEL_OERDER.len() {
        channels.extend(vec![
            audio_mixer::Channel::Silence;
            channel_count - CHANNEL_OERDER.len()
        ]);
    }

    channels
}

#[derive(Debug)]
enum MixerType {
    IntegerMixer(audio_mixer::Mixer<i16>),
    FloatMixer(audio_mixer::Mixer<f32>),
}

impl MixerType {
    fn new(
        format: SampleFormat,
        input_channels: &[audio_mixer::Channel],
        output_channels: &[audio_mixer::Channel],
    ) -> Self {
        match format {
            SampleFormat::S16LE | SampleFormat::S16BE | SampleFormat::S16NE => {
                cubeb_log!("Create an integer type(i16) mixer");
                Self::IntegerMixer(audio_mixer::Mixer::<i16>::new(
                    input_channels,
                    output_channels,
                ))
            }
            SampleFormat::Float32LE | SampleFormat::Float32BE | SampleFormat::Float32NE => {
                cubeb_log!("Create an floating type(f32) mixer");
                Self::FloatMixer(audio_mixer::Mixer::<f32>::new(
                    input_channels,
                    output_channels,
                ))
            }
        }
    }

    fn sample_size(&self) -> usize {
        match self {
            MixerType::IntegerMixer(_) => mem::size_of::<i16>(),
            MixerType::FloatMixer(_) => mem::size_of::<f32>(),
        }
    }

    fn input_channels(&self) -> &[Channel] {
        match self {
            MixerType::IntegerMixer(m) => m.input_channels(),
            MixerType::FloatMixer(m) => m.input_channels(),
        }
    }

    fn output_channels(&self) -> &[Channel] {
        match self {
            MixerType::IntegerMixer(m) => m.output_channels(),
            MixerType::FloatMixer(m) => m.output_channels(),
        }
    }

    fn mix(
        &self,
        input_buffer_ptr: *const (),
        input_buffer_size: usize,
        output_buffer_ptr: *mut (),
        output_buffer_size: usize,
        frames: usize,
    ) {
        use std::slice;

        // Check input buffer size.
        let size_needed = frames * self.input_channels().len() * self.sample_size();
        assert!(input_buffer_size >= size_needed);
        // Check output buffer size.
        let size_needed = frames * self.output_channels().len() * self.sample_size();
        assert!(output_buffer_size >= size_needed);

        match self {
            MixerType::IntegerMixer(m) => {
                let in_buf_ptr = input_buffer_ptr as *const i16;
                let out_buf_ptr = output_buffer_ptr as *mut i16;
                let input_buffer = unsafe {
                    slice::from_raw_parts(in_buf_ptr, frames * self.input_channels().len())
                };
                let output_buffer = unsafe {
                    slice::from_raw_parts_mut(out_buf_ptr, frames * self.output_channels().len())
                };
                let mut in_buf = input_buffer.chunks(self.input_channels().len());
                let mut out_buf = output_buffer.chunks_mut(self.output_channels().len());
                for _ in 0..frames {
                    m.mix(in_buf.next().unwrap(), out_buf.next().unwrap());
                }
            }
            MixerType::FloatMixer(m) => {
                let in_buf_ptr = input_buffer_ptr as *const f32;
                let out_buf_ptr = output_buffer_ptr as *mut f32;
                let input_buffer = unsafe {
                    slice::from_raw_parts(in_buf_ptr, frames * self.input_channels().len())
                };
                let output_buffer = unsafe {
                    slice::from_raw_parts_mut(out_buf_ptr, frames * self.output_channels().len())
                };
                let mut in_buf = input_buffer.chunks(self.input_channels().len());
                let mut out_buf = output_buffer.chunks_mut(self.output_channels().len());
                for _ in 0..frames {
                    m.mix(in_buf.next().unwrap(), out_buf.next().unwrap());
                }
            }
        };
    }
}

#[derive(Debug)]
pub struct Mixer {
    mixer: MixerType,
    // Only accessed from callback thread.
    buffer: Vec<u8>,
}

impl Mixer {
    pub fn new(
        format: SampleFormat,
        in_channel_count: usize,
        input_layout: ChannelLayout,
        out_channel_count: usize,
        mut output_channels: Vec<audio_mixer::Channel>,
    ) -> Self {
        assert!(in_channel_count > 0);
        assert!(out_channel_count > 0);

        cubeb_log!(
            "Creating a mixer with input channel count: {}, input layout: {:?},\
             out channel count: {}, output channels: {:?}",
            in_channel_count,
            input_layout,
            out_channel_count,
            output_channels
        );

        let input_channels = if in_channel_count as u32 != input_layout.bits().count_ones() {
            cubeb_log!(
                "Mismatch between input channels and layout. Applying default layout instead"
            );
            get_default_channel_order(in_channel_count)
        } else {
            get_channel_order(input_layout)
        };

        // When having one or two channel, force mono or stereo. Some devices (namely,
        // Bose QC35, mark 1 and 2), expose a single channel mapped to the right for
        // some reason. Some devices (e.g., builtin speaker on MacBook Pro 2018) map
        // the channel layout to the undefined channels.
        if out_channel_count == 1 {
            output_channels = vec![audio_mixer::Channel::FrontCenter];
        } else if out_channel_count == 2 {
            output_channels = vec![
                audio_mixer::Channel::FrontLeft,
                audio_mixer::Channel::FrontRight,
            ];
        }

        let all_silence = vec![audio_mixer::Channel::Silence; out_channel_count];
        if output_channels.is_empty()
            || out_channel_count != output_channels.len()
            || all_silence == output_channels
            || Self::non_silent_duplicate_channel_present(&output_channels)
        {
            cubeb_log!("Use invalid layout. Apply default layout instead");
            output_channels = get_default_channel_order(out_channel_count);
        }

        Self {
            mixer: MixerType::new(format, &input_channels, &output_channels),
            buffer: Vec::new(),
        }
    }

    pub fn update_buffer_size(&mut self, frames: usize) -> bool {
        let size_needed = frames * self.mixer.input_channels().len() * self.mixer.sample_size();
        let elements_needed = size_needed / mem::size_of::<u8>();
        if self.buffer.len() < elements_needed {
            self.buffer.resize(elements_needed, 0);
            true
        } else {
            false
        }
    }

    pub fn get_buffer_mut_ptr(&mut self) -> *mut u8 {
        self.buffer.as_mut_ptr()
    }

    // `update_buffer_size` must be called before this.
    pub fn mix(&self, frames: usize, dest_buffer: *mut c_void, dest_buffer_size: usize) -> c_int {
        let (src_buffer_ptr, src_buffer_size) = self.get_buffer_info();
        self.mixer.mix(
            src_buffer_ptr as *const (),
            src_buffer_size,
            dest_buffer as *mut (),
            dest_buffer_size,
            frames,
        );
        0
    }

    fn get_buffer_info(&self) -> (*const u8, usize) {
        (
            self.buffer.as_ptr(),
            self.buffer.len() * mem::size_of::<u8>(),
        )
    }

    fn non_silent_duplicate_channel_present(channels: &[audio_mixer::Channel]) -> bool {
        let mut bitmap: u32 = 0;
        for channel in channels {
            if channel != &Channel::Silence {
                if (bitmap & channel.bitmask()) != 0 {
                    return true;
                }
                bitmap |= channel.bitmask();
            }
        }
        false
    }
}

// This test gives a clear channel order of the ChannelLayout passed from cubeb interface.
#[test]
fn test_get_channel_order() {
    assert_eq!(
        get_channel_order(ChannelLayout::MONO),
        [Channel::FrontCenter]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::MONO_LFE),
        [Channel::FrontCenter, Channel::LowFrequency]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::STEREO),
        [Channel::FrontLeft, Channel::FrontRight]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::STEREO_LFE),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::LowFrequency
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_3F),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::FrontCenter
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_3F_LFE),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::FrontCenter,
            Channel::LowFrequency
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_2F1),
        [Channel::FrontLeft, Channel::FrontRight, Channel::BackCenter]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_2F1_LFE),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::LowFrequency,
            Channel::BackCenter
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_3F1),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::FrontCenter,
            Channel::BackCenter
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_3F1_LFE),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::FrontCenter,
            Channel::LowFrequency,
            Channel::BackCenter
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_2F2),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::SideLeft,
            Channel::SideRight
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_2F2_LFE),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::LowFrequency,
            Channel::SideLeft,
            Channel::SideRight
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::QUAD),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::BackLeft,
            Channel::BackRight
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::QUAD_LFE),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::LowFrequency,
            Channel::BackLeft,
            Channel::BackRight
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_3F2),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::FrontCenter,
            Channel::SideLeft,
            Channel::SideRight
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_3F2_LFE),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::FrontCenter,
            Channel::LowFrequency,
            Channel::SideLeft,
            Channel::SideRight
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_3F2_BACK),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::FrontCenter,
            Channel::BackLeft,
            Channel::BackRight
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_3F2_LFE_BACK),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::FrontCenter,
            Channel::LowFrequency,
            Channel::BackLeft,
            Channel::BackRight
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_3F3R_LFE),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::FrontCenter,
            Channel::LowFrequency,
            Channel::BackCenter,
            Channel::SideLeft,
            Channel::SideRight
        ]
    );
    assert_eq!(
        get_channel_order(ChannelLayout::_3F4_LFE),
        [
            Channel::FrontLeft,
            Channel::FrontRight,
            Channel::FrontCenter,
            Channel::LowFrequency,
            Channel::BackLeft,
            Channel::BackRight,
            Channel::SideLeft,
            Channel::SideRight
        ]
    );
}

#[test]
fn test_get_default_channel_order() {
    for len in 1..CHANNEL_OERDER.len() + 10 {
        let channels = get_default_channel_order(len);
        if len <= CHANNEL_OERDER.len() {
            assert_eq!(channels, &CHANNEL_OERDER[..len]);
        } else {
            let silences = vec![audio_mixer::Channel::Silence; len - CHANNEL_OERDER.len()];
            assert_eq!(channels[..CHANNEL_OERDER.len()], CHANNEL_OERDER);
            assert_eq!(&channels[CHANNEL_OERDER.len()..], silences.as_slice());
        }
    }
}

#[test]
fn test_non_silent_duplicate_channels() {
    let duplicate = [
        Channel::FrontLeft,
        Channel::Silence,
        Channel::FrontRight,
        Channel::FrontCenter,
        Channel::Silence,
        Channel::FrontRight,
    ];
    assert!(Mixer::non_silent_duplicate_channel_present(&duplicate));

    let non_duplicate = [
        Channel::FrontLeft,
        Channel::Silence,
        Channel::FrontRight,
        Channel::FrontCenter,
        Channel::Silence,
        Channel::Silence,
    ];
    assert!(!Mixer::non_silent_duplicate_channel_present(&non_duplicate));
}
