/// Check if needed fields are still public.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

extern crate mp4parse as mp4;

use std::io::{Cursor, Read};
use std::fs::File;

static MINI_MP4: &'static str = "tests/minimal.mp4";
static AUDIO_EME_MP4: &'static str = "tests/bipbop-cenc-audioinit.mp4";
static VIDEO_EME_MP4: &'static str = "tests/bipbop_480wp_1001kbps-cenc-video-key1-init.mp4";

// Taken from https://github.com/GuillaumeGomez/audio-video-metadata/blob/9dff40f565af71d5502e03a2e78ae63df95cfd40/src/metadata.rs#L53
#[test]
fn public_api() {
    let mut fd = File::open(MINI_MP4).expect("Unknown file");
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
                        assert!(!v.is_empty());
                        "AVC"
                    }
                    mp4::VideoCodecSpecific::VPxConfig(vpx) => {
                        // We don't enter in here, we just check if fields are public.
                        assert!(vpx.bit_depth > 0);
                        assert!(vpx.color_space > 0);
                        assert!(vpx.chroma_subsampling > 0);
                        assert!(!vpx.codec_init.is_empty());
                        "VPx"
                    }
                    mp4::VideoCodecSpecific::ESDSConfig(mp4v) => {
                        assert!(!mp4v.is_empty());
                        "MP4V"
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
                        assert_eq!(esds.audio_object_type.unwrap(), 2);
                        "ES"
                    }
                    mp4::AudioCodecSpecific::FLACSpecificBox(flac) => {
                        // STREAMINFO block must be present and first.
                        assert!(!flac.blocks.is_empty());
                        assert_eq!(flac.blocks[0].block_type, 0);
                        assert_eq!(flac.blocks[0].data.len(), 34);
                        "FLAC"
                    }
                    mp4::AudioCodecSpecific::OpusSpecificBox(opus) => {
                        // We don't enter in here, we just check if fields are public.
                        assert!(opus.version > 0);
                        "Opus"
                    }
                    mp4::AudioCodecSpecific::MP3 => {
                        "MP3"
                    }
                }, "ES");
                assert!(a.samplesize > 0);
                assert!(a.samplerate > 0);
            }
            Some(mp4::SampleEntry::Unknown) | None => {}
        }
    }
}

#[test]
fn public_audio_tenc() {
    let kid =
        vec![0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04,
             0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04];

    let mut fd = File::open(AUDIO_EME_MP4).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let mut context = mp4::MediaContext::new();
    mp4::read_mp4(&mut c, &mut context).expect("read_mp4 failed");
    for track in context.tracks {
        assert_eq!(track.codec_type, mp4::CodecType::EncryptedAudio);
        match track.data {
            Some(mp4::SampleEntry::Audio(a)) => {
                match a.protection_info.iter().find(|sinf| sinf.tenc.is_some()) {
                    Some(p) => {
                        assert_eq!(p.code_name, "mp4a");
                        if let Some(ref tenc) = p.tenc {
                            assert!(tenc.is_encrypted > 0);
                            assert_eq!(tenc.iv_size, 16);
                            assert_eq!(tenc.kid, kid);
                        } else {
                            assert!(false, "Invalid test condition");
                        }
                    },
                    _=> {
                        assert!(false, "Invalid test condition");
                    },
                }
            },
            _ => {
                assert!(false, "Invalid test condition");
            }
        }
    }
}

#[test]
fn public_video_cenc() {
    let system_id =
        vec![0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02,
             0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b];

    let kid =
        vec![0x7e, 0x57, 0x1d, 0x03, 0x7e, 0x57, 0x1d, 0x03,
             0x7e, 0x57, 0x1d, 0x03, 0x7e, 0x57, 0x1d, 0x11];

    let pssh_box =
        vec![0x00, 0x00, 0x00, 0x34, 0x70, 0x73, 0x73, 0x68,
             0x01, 0x00, 0x00, 0x00, 0x10, 0x77, 0xef, 0xec,
             0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e,
             0x52, 0xe2, 0xfb, 0x4b, 0x00, 0x00, 0x00, 0x01,
             0x7e, 0x57, 0x1d, 0x03, 0x7e, 0x57, 0x1d, 0x03,
             0x7e, 0x57, 0x1d, 0x03, 0x7e, 0x57, 0x1d, 0x11,
             0x00, 0x00, 0x00, 0x00];

    let mut fd = File::open(VIDEO_EME_MP4).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let mut context = mp4::MediaContext::new();
    mp4::read_mp4(&mut c, &mut context).expect("read_mp4 failed");
    for track in context.tracks {
        assert_eq!(track.codec_type, mp4::CodecType::EncryptedVideo);
        match track.data {
            Some(mp4::SampleEntry::Video(v)) => {
                match v.protection_info.iter().find(|sinf| sinf.tenc.is_some()) {
                    Some(p) => {
                        assert_eq!(p.code_name, "avc1");
                        if let Some(ref tenc) = p.tenc {
                            assert!(tenc.is_encrypted > 0);
                            assert_eq!(tenc.iv_size, 16);
                            assert_eq!(tenc.kid, kid);
                        } else {
                            assert!(false, "Invalid test condition");
                        }
                    },
                    _=> {
                        assert!(false, "Invalid test condition");
                    },
                }
            },
            _ => {
                assert!(false, "Invalid test condition");
            }
        }
    }

    for pssh in context.psshs {
        assert_eq!(pssh.system_id, system_id);
        for kid_id in pssh.kid {
            assert_eq!(kid_id, kid);
        }
        assert!(pssh.data.is_empty());
        assert_eq!(pssh.box_content, pssh_box);
    }
}
