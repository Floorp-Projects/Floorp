/// Check if needed fields are still public.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

extern crate mp4parse as mp4;

use std::io::{Cursor, Read};
use std::fs::File;

// Taken from https://github.com/GuillaumeGomez/audio-video-metadata/blob/9dff40f565af71d5502e03a2e78ae63df95cfd40/src/metadata.rs#L53
#[test]
fn public_api() {
    let mut fd = File::open("tests/minimal.mp4").expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let mut context = mp4::MediaContext::new();
    mp4::read_mp4(&mut c, &mut context).expect("read_mp4 failed");
    assert_eq!(context.timescale, Some(mp4::MediaTimeScale(1000)));
    for track in context.tracks {
        match track.data {
            Some(mp4::SampleEntry::Video(v)) => {
                // track part
                assert_eq!(track.duration, Some(mp4::TrackScaledTime(512, 0)));
                assert_eq!(track.empty_duration, Some(mp4::MediaScaledTime(0)));
                assert_eq!(track.media_time, Some(mp4::TrackScaledTime(0, 0)));
                assert_eq!(track.timescale, Some(mp4::TrackTimeScale(12800, 0)));
                assert_eq!(v.width, 320);
                assert_eq!(v.height, 240);

                // track.tkhd part
                let tkhd = track.tkhd.unwrap();
                assert_eq!(tkhd.disabled, false);
                assert_eq!(tkhd.duration, 40);
                assert_eq!(tkhd.width, 20971520);
                assert_eq!(tkhd.height, 15728640);

                // track.data part
                assert_eq!(match v.codec_specific {
                    mp4::VideoCodecSpecific::AVCConfig(v) => {
                        assert!(v.len() > 0);
                        "AVC"
                    }
                    mp4::VideoCodecSpecific::VPxConfig(vpx) => {
                        // We don't enter in here, we just check if fields are public.
                        assert!(vpx.bit_depth > 0);
                        assert!(vpx.color_space > 0);
                        assert!(vpx.chroma_subsampling > 0);
                        assert!(vpx.codec_init.len() > 0);
                        "VPx"
                    }
                }, "AVC");
            }
            Some(mp4::SampleEntry::Audio(a)) => {
                // track part
                assert_eq!(track.duration, Some(mp4::TrackScaledTime(2944, 1)));
                assert_eq!(track.empty_duration, Some(mp4::MediaScaledTime(0)));
                assert_eq!(track.media_time, Some(mp4::TrackScaledTime(1024, 1)));
                assert_eq!(track.timescale, Some(mp4::TrackTimeScale(48000, 1)));

                // track.tkhd part
                let tkhd = track.tkhd.unwrap();
                assert_eq!(tkhd.disabled, false);
                assert_eq!(tkhd.duration, 62);
                assert_eq!(tkhd.width, 0);
                assert_eq!(tkhd.height, 0);

                // track.data part
                assert_eq!(match a.codec_specific {
                    mp4::AudioCodecSpecific::ES_Descriptor(esds) => {
                        assert_eq!(esds.audio_codec, mp4::CodecType::AAC);
                        assert_eq!(esds.audio_sample_rate.unwrap(), 48000);
                        "ES"
                    }
                    mp4::AudioCodecSpecific::FLACSpecificBox(flac) => {
                        // STREAMINFO block must be present and first.
                        assert!(flac.blocks.len() > 0);
                        assert!(flac.blocks[0].block_type == 0);
                        assert!(flac.blocks[0].data.len() == 34);
                        "FLAC"
                    }
                    mp4::AudioCodecSpecific::OpusSpecificBox(opus) => {
                        // We don't enter in here, we just check if fields are public.
                        assert!(opus.version > 0);
                        "Opus"
                    }
                }, "ES");
                assert!(a.samplesize > 0);
                assert!(a.samplerate > 0);
            }
            Some(mp4::SampleEntry::Unknown) | None => {}
        }
    }
}
