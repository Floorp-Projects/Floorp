/// Check if needed fields are still public.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
extern crate mp4parse as mp4;

use std::convert::TryInto;
use std::fs::File;
use std::io::{Cursor, Read};
use std::path::Path;

static MINI_MP4: &str = "tests/minimal.mp4";
static MINI_MP4_WITH_METADATA: &str = "tests/metadata.mp4";
static MINI_MP4_WITH_METADATA_STD_GENRE: &str = "tests/metadata_gnre.mp4";

static AUDIO_EME_CENC_MP4: &str = "tests/bipbop-cenc-audioinit.mp4";
static VIDEO_EME_CENC_MP4: &str = "tests/bipbop_480wp_1001kbps-cenc-video-key1-init.mp4";
// The cbcs files were created via shaka-packager from Firefox's test suite's bipbop.mp4 using:
// packager-win.exe
// in=bipbop.mp4,stream=audio,init_segment=bipbop_cbcs_audio_init.mp4,segment_template=bipbop_cbcs_audio_$Number$.m4s
// in=bipbop.mp4,stream=video,init_segment=bipbop_cbcs_video_init.mp4,segment_template=bipbop_cbcs_video_$Number$.m4s
// --protection_scheme cbcs --enable_raw_key_encryption
// --keys label=:key_id=7e571d047e571d047e571d047e571d21:key=7e5744447e5744447e5744447e574421
// --iv 11223344556677889900112233445566
// --generate_static_mpd --mpd_output bipbop_cbcs.mpd
// note: only the init files are needed for these tests
static AUDIO_EME_CBCS_MP4: &str = "tests/bipbop_cbcs_audio_init.mp4";
static VIDEO_EME_CBCS_MP4: &str = "tests/bipbop_cbcs_video_init.mp4";
static VIDEO_AV1_MP4: &str = "tests/tiny_av1.mp4";
static IMAGE_AVIF: &str = "av1-avif/testFiles/Microsoft/Monochrome.avif";
static IMAGE_AVIF_GRID: &str = "av1-avif/testFiles/Microsoft/Summer_in_Tomsk_720p_5x4_grid.avif";
static AVIF_TEST_DIR: &str = "av1-avif/testFiles";

// Adapted from https://github.com/GuillaumeGomez/audio-video-metadata/blob/9dff40f565af71d5502e03a2e78ae63df95cfd40/src/metadata.rs#L53
#[test]
#[allow(clippy::cognitive_complexity)] // TODO: Consider simplifying this
fn public_api() {
    let mut fd = File::open(MINI_MP4).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let mut context = mp4::MediaContext::new();
    mp4::read_mp4(&mut c, &mut context).expect("read_mp4 failed");
    assert_eq!(context.timescale, Some(mp4::MediaTimeScale(1000)));
    for track in context.tracks {
        match track.track_type {
            mp4::TrackType::Video => {
                // track part
                assert_eq!(track.duration, Some(mp4::TrackScaledTime(512, 0)));
                assert_eq!(track.empty_duration, Some(mp4::MediaScaledTime(0)));
                assert_eq!(track.media_time, Some(mp4::TrackScaledTime(0, 0)));
                assert_eq!(track.timescale, Some(mp4::TrackTimeScale(12800, 0)));

                // track.tkhd part
                let tkhd = track.tkhd.unwrap();
                assert_eq!(tkhd.disabled, false);
                assert_eq!(tkhd.duration, 40);
                assert_eq!(tkhd.width, 20_971_520);
                assert_eq!(tkhd.height, 15_728_640);

                // track.stsd part
                let stsd = track.stsd.expect("expected an stsd");
                let v = match stsd.descriptions.first().expect("expected a SampleEntry") {
                    mp4::SampleEntry::Video(v) => v,
                    _ => panic!("expected a VideoSampleEntry"),
                };
                assert_eq!(v.width, 320);
                assert_eq!(v.height, 240);
                assert_eq!(
                    match v.codec_specific {
                        mp4::VideoCodecSpecific::AVCConfig(ref avc) => {
                            assert!(!avc.is_empty());
                            "AVC"
                        }
                        mp4::VideoCodecSpecific::VPxConfig(ref vpx) => {
                            // We don't enter in here, we just check if fields are public.
                            assert!(vpx.bit_depth > 0);
                            assert!(vpx.colour_primaries > 0);
                            assert!(vpx.chroma_subsampling > 0);
                            assert!(!vpx.codec_init.is_empty());
                            "VPx"
                        }
                        mp4::VideoCodecSpecific::ESDSConfig(ref mp4v) => {
                            assert!(!mp4v.is_empty());
                            "MP4V"
                        }
                        mp4::VideoCodecSpecific::AV1Config(ref _av1c) => {
                            "AV1"
                        }
                    },
                    "AVC"
                );
            }
            mp4::TrackType::Audio => {
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

                // track.stsd part
                let stsd = track.stsd.expect("expected an stsd");
                let a = match stsd.descriptions.first().expect("expected a SampleEntry") {
                    mp4::SampleEntry::Audio(a) => a,
                    _ => panic!("expected a AudioSampleEntry"),
                };
                assert_eq!(
                    match a.codec_specific {
                        mp4::AudioCodecSpecific::ES_Descriptor(ref esds) => {
                            assert_eq!(esds.audio_codec, mp4::CodecType::AAC);
                            assert_eq!(esds.audio_sample_rate.unwrap(), 48000);
                            assert_eq!(esds.audio_object_type.unwrap(), 2);
                            "ES"
                        }
                        mp4::AudioCodecSpecific::FLACSpecificBox(ref flac) => {
                            // STREAMINFO block must be present and first.
                            assert!(!flac.blocks.is_empty());
                            assert_eq!(flac.blocks[0].block_type, 0);
                            assert_eq!(flac.blocks[0].data.len(), 34);
                            "FLAC"
                        }
                        mp4::AudioCodecSpecific::OpusSpecificBox(ref opus) => {
                            // We don't enter in here, we just check if fields are public.
                            assert!(opus.version > 0);
                            "Opus"
                        }
                        mp4::AudioCodecSpecific::ALACSpecificBox(ref alac) => {
                            assert!(alac.data.len() == 24 || alac.data.len() == 48);
                            "ALAC"
                        }
                        mp4::AudioCodecSpecific::MP3 => {
                            "MP3"
                        }
                        mp4::AudioCodecSpecific::LPCM => {
                            "LPCM"
                        }
                    },
                    "ES"
                );
                assert!(a.samplesize > 0);
                assert!(a.samplerate > 0.0);
            }
            mp4::TrackType::Metadata | mp4::TrackType::Unknown => {}
        }
    }
}

#[test]
#[allow(clippy::cognitive_complexity)] // TODO: Consider simplifying this
fn public_metadata() {
    let mut fd = File::open(MINI_MP4_WITH_METADATA).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let mut context = mp4::MediaContext::new();
    mp4::read_mp4(&mut c, &mut context).expect("read_mp4 failed");
    let udta = context
        .userdata
        .expect("didn't find udta")
        .expect("failed to parse udta");
    let meta = udta.meta.expect("didn't find meta");
    assert_eq!(meta.title.unwrap(), "Title");
    assert_eq!(meta.artist.unwrap(), "Artist");
    assert_eq!(meta.album_artist.unwrap(), "Album Artist");
    assert_eq!(meta.comment.unwrap(), "Comments");
    assert_eq!(meta.year.unwrap(), "2019");
    assert_eq!(
        meta.genre.unwrap(),
        mp4::Genre::CustomGenre("Custom Genre".try_into().unwrap())
    );
    assert_eq!(meta.encoder.unwrap(), "Lavf56.40.101");
    assert_eq!(meta.encoded_by.unwrap(), "Encoded-by");
    assert_eq!(meta.copyright.unwrap(), "Copyright");
    assert_eq!(meta.track_number.unwrap(), 3);
    assert_eq!(meta.total_tracks.unwrap(), 6);
    assert_eq!(meta.disc_number.unwrap(), 5);
    assert_eq!(meta.total_discs.unwrap(), 10);
    assert_eq!(meta.beats_per_minute.unwrap(), 128);
    assert_eq!(meta.composer.unwrap(), "Composer");
    assert_eq!(meta.compilation.unwrap(), true);
    assert_eq!(meta.gapless_playback.unwrap(), false);
    assert_eq!(meta.podcast.unwrap(), false);
    assert_eq!(meta.advisory.unwrap(), mp4::AdvisoryRating::Clean);
    assert_eq!(meta.media_type.unwrap(), mp4::MediaType::Normal);
    assert_eq!(meta.rating.unwrap(), "50");
    assert_eq!(meta.grouping.unwrap(), "Grouping");
    assert_eq!(meta.category.unwrap(), "Category");
    assert_eq!(meta.keyword.unwrap(), "Keyword");
    assert_eq!(meta.description.unwrap(), "Description");
    assert_eq!(meta.lyrics.unwrap(), "Lyrics");
    assert_eq!(meta.long_description.unwrap(), "Long Description");
    assert_eq!(meta.tv_episode_name.unwrap(), "Episode Name");
    assert_eq!(meta.tv_network_name.unwrap(), "Network Name");
    assert_eq!(meta.tv_episode_number.unwrap(), 15);
    assert_eq!(meta.tv_season.unwrap(), 10);
    assert_eq!(meta.tv_show_name.unwrap(), "Show Name");
    assert_eq!(meta.hd_video.unwrap(), true);
    assert_eq!(meta.owner.unwrap(), "Owner");
    assert_eq!(meta.sort_name.unwrap(), "Sort Name");
    assert_eq!(meta.sort_album.unwrap(), "Sort Album");
    assert_eq!(meta.sort_artist.unwrap(), "Sort Artist");
    assert_eq!(meta.sort_album_artist.unwrap(), "Sort Album Artist");
    assert_eq!(meta.sort_composer.unwrap(), "Sort Composer");

    // Check for valid JPEG header
    let covers = meta.cover_art.unwrap();
    let cover = &covers[0];
    let mut bytes = [0u8; 4];
    bytes[0] = cover[0];
    bytes[1] = cover[1];
    bytes[2] = cover[2];
    assert_eq!(u32::from_le_bytes(bytes), 0x00ff_d8ff);
}

#[test]
#[allow(clippy::cognitive_complexity)] // TODO: Consider simplifying this
fn public_metadata_gnre() {
    let mut fd = File::open(MINI_MP4_WITH_METADATA_STD_GENRE).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let mut context = mp4::MediaContext::new();
    mp4::read_mp4(&mut c, &mut context).expect("read_mp4 failed");
    let udta = context
        .userdata
        .expect("didn't find udta")
        .expect("failed to parse udta");
    let meta = udta.meta.expect("didn't find meta");
    assert_eq!(meta.title.unwrap(), "Title");
    assert_eq!(meta.artist.unwrap(), "Artist");
    assert_eq!(meta.album_artist.unwrap(), "Album Artist");
    assert_eq!(meta.comment.unwrap(), "Comments");
    assert_eq!(meta.year.unwrap(), "2019");
    assert_eq!(meta.genre.unwrap(), mp4::Genre::StandardGenre(3));
    assert_eq!(meta.encoder.unwrap(), "Lavf56.40.101");
    assert_eq!(meta.encoded_by.unwrap(), "Encoded-by");
    assert_eq!(meta.copyright.unwrap(), "Copyright");
    assert_eq!(meta.track_number.unwrap(), 3);
    assert_eq!(meta.total_tracks.unwrap(), 6);
    assert_eq!(meta.disc_number.unwrap(), 5);
    assert_eq!(meta.total_discs.unwrap(), 10);
    assert_eq!(meta.beats_per_minute.unwrap(), 128);
    assert_eq!(meta.composer.unwrap(), "Composer");
    assert_eq!(meta.compilation.unwrap(), true);
    assert_eq!(meta.gapless_playback.unwrap(), false);
    assert_eq!(meta.podcast.unwrap(), false);
    assert_eq!(meta.advisory.unwrap(), mp4::AdvisoryRating::Clean);
    assert_eq!(meta.media_type.unwrap(), mp4::MediaType::Normal);
    assert_eq!(meta.rating.unwrap(), "50");
    assert_eq!(meta.grouping.unwrap(), "Grouping");
    assert_eq!(meta.category.unwrap(), "Category");
    assert_eq!(meta.keyword.unwrap(), "Keyword");
    assert_eq!(meta.description.unwrap(), "Description");
    assert_eq!(meta.lyrics.unwrap(), "Lyrics");
    assert_eq!(meta.long_description.unwrap(), "Long Description");
    assert_eq!(meta.tv_episode_name.unwrap(), "Episode Name");
    assert_eq!(meta.tv_network_name.unwrap(), "Network Name");
    assert_eq!(meta.tv_episode_number.unwrap(), 15);
    assert_eq!(meta.tv_season.unwrap(), 10);
    assert_eq!(meta.tv_show_name.unwrap(), "Show Name");
    assert_eq!(meta.hd_video.unwrap(), true);
    assert_eq!(meta.owner.unwrap(), "Owner");
    assert_eq!(meta.sort_name.unwrap(), "Sort Name");
    assert_eq!(meta.sort_album.unwrap(), "Sort Album");
    assert_eq!(meta.sort_artist.unwrap(), "Sort Artist");
    assert_eq!(meta.sort_album_artist.unwrap(), "Sort Album Artist");
    assert_eq!(meta.sort_composer.unwrap(), "Sort Composer");

    // Check for valid JPEG header
    let covers = meta.cover_art.unwrap();
    let cover = &covers[0];
    let mut bytes = [0u8; 4];
    bytes[0] = cover[0];
    bytes[1] = cover[1];
    bytes[2] = cover[2];
    assert_eq!(u32::from_le_bytes(bytes), 0x00ff_d8ff);
}

#[test]
fn public_audio_tenc() {
    let kid = vec![
        0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d,
        0x04,
    ];

    let mut fd = File::open(AUDIO_EME_CENC_MP4).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let mut context = mp4::MediaContext::new();
    mp4::read_mp4(&mut c, &mut context).expect("read_mp4 failed");
    for track in context.tracks {
        let stsd = track.stsd.expect("expected an stsd");
        let a = match stsd.descriptions.first().expect("expected a SampleEntry") {
            mp4::SampleEntry::Audio(a) => a,
            _ => panic!("expected a AudioSampleEntry"),
        };
        assert_eq!(a.codec_type, mp4::CodecType::EncryptedAudio);
        match a.protection_info.iter().find(|sinf| sinf.tenc.is_some()) {
            Some(ref p) => {
                assert_eq!(p.code_name, "mp4a");
                if let Some(ref schm) = p.scheme_type {
                    assert_eq!(schm.scheme_type.value, *b"cenc");
                } else {
                    panic!("Expected scheme type info");
                }
                if let Some(ref tenc) = p.tenc {
                    assert!(tenc.is_encrypted > 0);
                    assert_eq!(tenc.iv_size, 16);
                    assert_eq!(tenc.kid, kid);
                    assert_eq!(tenc.crypt_byte_block_count, None);
                    assert_eq!(tenc.skip_byte_block_count, None);
                    assert_eq!(tenc.constant_iv, None);
                } else {
                    panic!("Invalid test condition");
                }
            }
            _ => {
                panic!("Invalid test condition");
            }
        }
    }
}

#[test]
fn public_video_cenc() {
    let system_id = vec![
        0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb,
        0x4b,
    ];

    let kid = vec![
        0x7e, 0x57, 0x1d, 0x03, 0x7e, 0x57, 0x1d, 0x03, 0x7e, 0x57, 0x1d, 0x03, 0x7e, 0x57, 0x1d,
        0x11,
    ];

    let pssh_box = vec![
        0x00, 0x00, 0x00, 0x34, 0x70, 0x73, 0x73, 0x68, 0x01, 0x00, 0x00, 0x00, 0x10, 0x77, 0xef,
        0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b, 0x00, 0x00,
        0x00, 0x01, 0x7e, 0x57, 0x1d, 0x03, 0x7e, 0x57, 0x1d, 0x03, 0x7e, 0x57, 0x1d, 0x03, 0x7e,
        0x57, 0x1d, 0x11, 0x00, 0x00, 0x00, 0x00,
    ];

    let mut fd = File::open(VIDEO_EME_CENC_MP4).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let mut context = mp4::MediaContext::new();
    mp4::read_mp4(&mut c, &mut context).expect("read_mp4 failed");
    for track in context.tracks {
        let stsd = track.stsd.expect("expected an stsd");
        let v = match stsd.descriptions.first().expect("expected a SampleEntry") {
            mp4::SampleEntry::Video(ref v) => v,
            _ => panic!("expected a VideoSampleEntry"),
        };
        assert_eq!(v.codec_type, mp4::CodecType::EncryptedVideo);
        match v.protection_info.iter().find(|sinf| sinf.tenc.is_some()) {
            Some(ref p) => {
                assert_eq!(p.code_name, "avc1");
                if let Some(ref schm) = p.scheme_type {
                    assert_eq!(schm.scheme_type.value, *b"cenc");
                } else {
                    panic!("Expected scheme type info");
                }
                if let Some(ref tenc) = p.tenc {
                    assert!(tenc.is_encrypted > 0);
                    assert_eq!(tenc.iv_size, 16);
                    assert_eq!(tenc.kid, kid);
                    assert_eq!(tenc.crypt_byte_block_count, None);
                    assert_eq!(tenc.skip_byte_block_count, None);
                    assert_eq!(tenc.constant_iv, None);
                } else {
                    panic!("Invalid test condition");
                }
            }
            _ => {
                panic!("Invalid test condition");
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

#[test]
fn public_audio_cbcs() {
    let system_id = vec![
        0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb,
        0x4b,
    ];

    let kid = vec![
        0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d,
        0x21,
    ];

    let default_iv = vec![
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
        0x66,
    ];

    let pssh_box = vec![
        0x00, 0x00, 0x00, 0x34, 0x70, 0x73, 0x73, 0x68, 0x01, 0x00, 0x00, 0x00, 0x10, 0x77, 0xef,
        0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b, 0x00, 0x00,
        0x00, 0x01, 0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04, 0x7e,
        0x57, 0x1d, 0x21, 0x00, 0x00, 0x00, 0x00,
    ];

    let mut fd = File::open(AUDIO_EME_CBCS_MP4).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let mut context = mp4::MediaContext::new();
    mp4::read_mp4(&mut c, &mut context).expect("read_mp4 failed");
    for track in context.tracks {
        let stsd = track.stsd.expect("expected an stsd");
        assert_eq!(stsd.descriptions.len(), 2);
        let mut found_encrypted_sample_description = false;
        for description in stsd.descriptions {
            match description {
                mp4::SampleEntry::Audio(ref a) => {
                    if let Some(p) = a.protection_info.iter().find(|sinf| sinf.tenc.is_some()) {
                        found_encrypted_sample_description = true;
                        assert_eq!(p.code_name, "mp4a");
                        if let Some(ref schm) = p.scheme_type {
                            assert_eq!(schm.scheme_type.value, *b"cbcs");
                        } else {
                            panic!("Expected scheme type info");
                        }
                        if let Some(ref tenc) = p.tenc {
                            assert!(tenc.is_encrypted > 0);
                            assert_eq!(tenc.iv_size, 0);
                            assert_eq!(tenc.kid, kid);
                            // Note: 0 for both crypt and skip seems odd but
                            // that's what shaka-packager produced. It appears
                            // to indicate full encryption.
                            assert_eq!(tenc.crypt_byte_block_count, Some(0));
                            assert_eq!(tenc.skip_byte_block_count, Some(0));
                            assert_eq!(tenc.constant_iv, Some(default_iv.clone().into()));
                        } else {
                            panic!("Invalid test condition");
                        }
                    }
                }
                _ => {
                    panic!("expected a VideoSampleEntry");
                }
            }
        }
        assert!(
            found_encrypted_sample_description,
            "Should have found an encrypted sample description"
        );
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

#[test]
#[allow(clippy::cognitive_complexity)] // TODO: Consider simplifying this
fn public_video_cbcs() {
    let system_id = vec![
        0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb,
        0x4b,
    ];

    let kid = vec![
        0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d,
        0x21,
    ];

    let default_iv = vec![
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
        0x66,
    ];

    let pssh_box = vec![
        0x00, 0x00, 0x00, 0x34, 0x70, 0x73, 0x73, 0x68, 0x01, 0x00, 0x00, 0x00, 0x10, 0x77, 0xef,
        0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b, 0x00, 0x00,
        0x00, 0x01, 0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04, 0x7e,
        0x57, 0x1d, 0x21, 0x00, 0x00, 0x00, 0x00,
    ];

    let mut fd = File::open(VIDEO_EME_CBCS_MP4).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let mut context = mp4::MediaContext::new();
    mp4::read_mp4(&mut c, &mut context).expect("read_mp4 failed");
    for track in context.tracks {
        let stsd = track.stsd.expect("expected an stsd");
        assert_eq!(stsd.descriptions.len(), 2);
        let mut found_encrypted_sample_description = false;
        for description in stsd.descriptions {
            match description {
                mp4::SampleEntry::Video(ref v) => {
                    assert_eq!(v.width, 400);
                    assert_eq!(v.height, 300);
                    if let Some(p) = v.protection_info.iter().find(|sinf| sinf.tenc.is_some()) {
                        found_encrypted_sample_description = true;
                        assert_eq!(p.code_name, "avc1");
                        if let Some(ref schm) = p.scheme_type {
                            assert_eq!(schm.scheme_type.value, *b"cbcs");
                        } else {
                            panic!("Expected scheme type info");
                        }
                        if let Some(ref tenc) = p.tenc {
                            assert!(tenc.is_encrypted > 0);
                            assert_eq!(tenc.iv_size, 0);
                            assert_eq!(tenc.kid, kid);
                            assert_eq!(tenc.crypt_byte_block_count, Some(1));
                            assert_eq!(tenc.skip_byte_block_count, Some(9));
                            assert_eq!(tenc.constant_iv, Some(default_iv.clone().into()));
                        } else {
                            panic!("Invalid test condition");
                        }
                    }
                }
                _ => {
                    panic!("expected a VideoSampleEntry");
                }
            }
        }
        assert!(
            found_encrypted_sample_description,
            "Should have found an encrypted sample description"
        );
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

#[test]
#[allow(clippy::cognitive_complexity)] // TODO: Consider simplifying this
fn public_video_av1() {
    let mut fd = File::open(VIDEO_AV1_MP4).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let mut context = mp4::MediaContext::new();
    mp4::read_mp4(&mut c, &mut context).expect("read_mp4 failed");
    for track in context.tracks {
        // track part
        assert_eq!(track.duration, Some(mp4::TrackScaledTime(512, 0)));
        assert_eq!(track.empty_duration, Some(mp4::MediaScaledTime(0)));
        assert_eq!(track.media_time, Some(mp4::TrackScaledTime(0, 0)));
        assert_eq!(track.timescale, Some(mp4::TrackTimeScale(12288, 0)));

        // track.tkhd part
        let tkhd = track.tkhd.unwrap();
        assert_eq!(tkhd.disabled, false);
        assert_eq!(tkhd.duration, 42);
        assert_eq!(tkhd.width, 4_194_304);
        assert_eq!(tkhd.height, 4_194_304);

        // track.stsd part
        let stsd = track.stsd.expect("expected an stsd");
        let v = match stsd.descriptions.first().expect("expected a SampleEntry") {
            mp4::SampleEntry::Video(ref v) => v,
            _ => panic!("expected a VideoSampleEntry"),
        };
        assert_eq!(v.codec_type, mp4::CodecType::AV1);
        assert_eq!(v.width, 64);
        assert_eq!(v.height, 64);

        match v.codec_specific {
            mp4::VideoCodecSpecific::AV1Config(ref av1c) => {
                // TODO: test av1c fields once ffmpeg is updated
                assert_eq!(av1c.profile, 0);
                assert_eq!(av1c.level, 0);
                assert_eq!(av1c.tier, 0);
                assert_eq!(av1c.bit_depth, 8);
                assert_eq!(av1c.monochrome, false);
                assert_eq!(av1c.chroma_subsampling_x, 1);
                assert_eq!(av1c.chroma_subsampling_y, 1);
                assert_eq!(av1c.chroma_sample_position, 0);
                assert_eq!(av1c.initial_presentation_delay_present, false);
                assert_eq!(av1c.initial_presentation_delay_minus_one, 0);
            }
            _ => panic!("Invalid test condition"),
        }
    }
}

#[test]
fn public_avif_primary_item() {
    let context = &mut mp4::AvifContext::new();
    let input = &mut File::open(IMAGE_AVIF).expect("Unknown file");
    mp4::read_avif(input, context).expect("read_avif failed");
    assert_eq!(context.primary_item.len(), 6979);
    assert_eq!(context.primary_item[0..4], [0x12, 0x00, 0x0a, 0x0a]);
}

#[test]
#[ignore] // Remove when we add support; see https://github.com/mozilla/mp4parse-rust/issues/198
fn public_avif_primary_item_is_grid() {
    let context = &mut mp4::AvifContext::new();
    let input = &mut File::open(IMAGE_AVIF_GRID).expect("Unknown file");
    mp4::read_avif(input, context).expect("read_avif failed");
    // Add some additional checks
}

#[test]
fn public_avif_read_samples() {
    env_logger::init();

    for entry in walkdir::WalkDir::new(AVIF_TEST_DIR) {
        let entry = entry.expect("AVIF entry");
        let path = entry.path();
        if !path.is_file() || path.extension().unwrap_or_default() != "avif" {
            eprintln!("Skipping {:?}", path);
            continue; // Skip directories, ReadMe.txt, etc.
        }
        if path == Path::new(IMAGE_AVIF_GRID) {
            eprintln!("Skipping {:?}", path);
            continue; // Remove when public_avif_primary_item_is_grid passes
        }
        println!("parsing {:?}", path);
        let context = &mut mp4::AvifContext::new();
        let input = &mut File::open(path).expect("Unknow file");
        mp4::read_avif(input, context).expect("read_avif failed");
    }
}
