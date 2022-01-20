/// Check if needed fields are still public.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
extern crate mp4parse as mp4;

use mp4::{Error, ParseStrictness, Status};
use std::convert::TryInto;
use std::fs::File;
use std::io::{Cursor, Read, Seek, SeekFrom};

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
// This file contains invalid userdata in its copyright userdata. See
// https://bugzilla.mozilla.org/show_bug.cgi?id=1687357 for more information.
static VIDEO_INVALID_USERDATA: &str = "tests/invalid_userdata.mp4";
static IMAGE_AVIF: &str = "tests/valid.avif";
static IMAGE_AVIF_EXTENTS: &str = "tests/multiple-extents.avif";
static IMAGE_AVIF_ALPHA: &str = "tests/valid-alpha.avif";
static IMAGE_AVIF_ALPHA_PREMULTIPLIED: &str = "tests/1x1-black-alpha-50pct-premultiplied.avif";
static IMAGE_AVIF_CORRUPT: &str = "tests/corrupt/bug-1655846.avif";
static IMAGE_AVIF_CORRUPT_2: &str = "tests/corrupt/bug-1661347.avif";
static IMAGE_AVIF_IPMA_BAD_VERSION: &str = "tests/bad-ipma-version.avif";
static IMAGE_AVIF_IPMA_BAD_FLAGS: &str = "tests/bad-ipma-flags.avif";
static IMAGE_AVIF_IPMA_DUPLICATE_VERSION_AND_FLAGS: &str =
    "tests/corrupt/ipma-duplicate-version-and-flags.avif";
static IMAGE_AVIF_IPMA_DUPLICATE_ITEM_ID: &str = "tests/corrupt/ipma-duplicate-item_id.avif";
static IMAGE_AVIF_IPMA_INVALID_PROPERTY_INDEX: &str =
    "tests/corrupt/ipma-invalid-property-index.avif";
static IMAGE_AVIF_NO_HDLR: &str = "tests/corrupt/hdlr-not-first.avif";
static IMAGE_AVIF_HDLR_NOT_FIRST: &str = "tests/corrupt/no-hdlr.avif";
static IMAGE_AVIF_HDLR_NOT_PICT: &str = "tests/corrupt/hdlr-not-pict.avif";
static IMAGE_AVIF_HDLR_NONZERO_RESERVED: &str = "tests/hdlr-nonzero-reserved.avif";
static IMAGE_AVIF_HDLR_MULTIPLE_NUL: &str = "tests/invalid-avif-hdlr-name-multiple-nul.avif";
static IMAGE_AVIF_NO_MIF1: &str = "tests/no-mif1.avif";
static IMAGE_AVIF_NO_PITM: &str = "tests/corrupt/no-pitm.avif";
static IMAGE_AVIF_NO_PIXI: &str = "tests/corrupt/no-pixi.avif";
static IMAGE_AVIF_NO_AV1C: &str = "tests/corrupt/no-av1C.avif";
static IMAGE_AVIF_NO_ISPE: &str = "tests/corrupt/no-ispe.avif";
static IMAGE_AVIF_NO_ALPHA_AV1C: &str = "tests/corrupt/no-alpha-av1C.avif";
static IMAGE_AVIF_NO_ALPHA_PIXI: &str = "tests/corrupt/no-pixi-for-alpha.avif";
static IMAGE_AVIF_AV1C_MISSING_ESSENTIAL: &str = "tests/av1C-missing-essential.avif";
static IMAGE_AVIF_A1LX_MARKED_ESSENTIAL: &str = "tests/corrupt/a1lx-marked-essential.avif";
static IMAGE_AVIF_A1OP_MISSING_ESSENTIAL: &str = "tests/corrupt/a1op-missing-essential.avif";
static IMAGE_AVIF_IMIR_MISSING_ESSENTIAL: &str = "tests/imir-missing-essential.avif";
static IMAGE_AVIF_IROT_MISSING_ESSENTIAL: &str = "tests/irot-missing-essential.avif";
static IMAGE_AVIF_LSEL_MISSING_ESSENTIAL: &str = "tests/corrupt/lsel-missing-essential.avif";
static IMAGE_AVIF_CLAP_MISSING_ESSENTIAL: &str = "tests/clap-missing-essential.avif";
static AVIF_TEST_DIRS: &[&str] = &["tests", "av1-avif/testFiles", "link-u-avif-sample-images"];

// These files are
//   av1-avif/testFiles/Apple/multilayer_examples/animals_00_multilayer_a1op.avif
//   av1-avif/testFiles/Apple/multilayer_examples/animals_00_multilayer_a1lx.avif
//   av1-avif/testFiles/Apple/multilayer_examples/animals_00_multilayer_lsel.avif
// respectively, but with https://github.com/AOMediaCodec/av1-avif/issues/174 fixed
static AVIF_A1OP: &str = "tests/a1op.avif";
static AVIF_A1LX: &str = "tests/a1lx.avif";
static AVIF_LSEL: &str = "tests/lsel.avif";

static AVIF_CLAP: &str = "tests/clap-basic-1_3x3-to-1x1.avif";
static AVIF_GRID: &str = "av1-avif/testFiles/Microsoft/Summer_in_Tomsk_720p_5x4_grid.avif";
static AVIF_GRID_A1LX: &str =
    "av1-avif/testFiles/Apple/multilayer_examples/animals_00_multilayer_grid_a1lx.avif";
static AVIF_AVIS_MAJOR_NO_PITM: &str =
    "av1-avif/testFiles/Netflix/avis/Chimera-AV1-10bit-480x270.avif";
/// This is av1-avif/testFiles/Netflix/avis/alpha_video.avif
/// but with https://github.com/AOMediaCodec/av1-avif/issues/177 fixed
static AVIF_AVIS_MAJOR_WITH_PITM_AND_ALPHA: &str = "tests/alpha_video_fixed.avif";
static AVIF_AVIS_MAJOR_NO_MOOV: &str = "tests/corrupt/alpha_video_moov_is_moop.avif";
static AVIF_NO_PIXI_IMAGES: &[&str] = &[IMAGE_AVIF_NO_PIXI, IMAGE_AVIF_NO_ALPHA_PIXI];
static AVIF_UNSUPPORTED_IMAGES: &[&str] = &[
    AVIF_A1LX,
    AVIF_A1OP,
    AVIF_CLAP,
    IMAGE_AVIF_CLAP_MISSING_ESSENTIAL,
    AVIF_GRID,
    AVIF_GRID_A1LX,
    AVIF_LSEL,
    AVIF_AVIS_MAJOR_NO_PITM,
    AVIF_AVIS_MAJOR_WITH_PITM_AND_ALPHA,
    "av1-avif/testFiles/Apple/multilayer_examples/animals_00_multilayer_a1lx.avif",
    "av1-avif/testFiles/Apple/multilayer_examples/animals_00_multilayer_a1op.avif",
    "av1-avif/testFiles/Apple/multilayer_examples/animals_00_multilayer_a1op_lsel.avif",
    "av1-avif/testFiles/Apple/multilayer_examples/animals_00_multilayer_lsel.avif",
    "av1-avif/testFiles/Apple/multilayer_examples/animals_00_multilayer_grid_lsel.avif",
    "av1-avif/testFiles/Link-U/kimono.crop.avif",
    "av1-avif/testFiles/Link-U/kimono.mirror-vertical.rotate270.crop.avif",
    "av1-avif/testFiles/Microsoft/Chimera_10bit_cropped_to_1920x1008.avif",
    "av1-avif/testFiles/Microsoft/Chimera_10bit_cropped_to_1920x1008_with_HDR_metadata.avif",
    "av1-avif/testFiles/Microsoft/Chimera_8bit_cropped_480x256.avif",
    "av1-avif/testFiles/Netflix/avis/alpha_video.avif",
    "av1-avif/testFiles/Xiph/abandoned_filmgrain.avif",
    "av1-avif/testFiles/Xiph/fruits_2layer_thumbsize.avif",
    "av1-avif/testFiles/Xiph/quebec_3layer_op2.avif",
    "av1-avif/testFiles/Xiph/tiger_3layer_1res.avif",
    "av1-avif/testFiles/Xiph/tiger_3layer_3res.avif",
    "link-u-avif-sample-images/kimono.crop.avif",
    "link-u-avif-sample-images/kimono.mirror-vertical.rotate270.crop.avif",
    "link-u-avif-sample-images/star-10bpc-with-alpha.avifs",
    "link-u-avif-sample-images/star-10bpc.avifs",
    "link-u-avif-sample-images/star-12bpc-with-alpha.avifs",
    "link-u-avif-sample-images/star-12bpc.avifs",
    "link-u-avif-sample-images/star-8bpc-with-alpha.avifs",
    "link-u-avif-sample-images/star-8bpc.avifs",
];
/// See https://github.com/AOMediaCodec/av1-avif/issues/150
///     https://github.com/AOMediaCodec/av1-avif/issues/174
/// and https://github.com/AOMediaCodec/av1-avif/issues/177
// TODO: make this into a map of expected errors?
static AV1_AVIF_CORRUPT_IMAGES: &[&str] = &[
    "link-u-avif-sample-images/plum-blossom-large.profile0.10bpc.yuv420.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile0.10bpc.yuv420.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile0.10bpc.yuv420.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile0.10bpc.yuv420.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile0.8bpc.yuv420.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile0.8bpc.yuv420.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile0.8bpc.yuv420.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile0.8bpc.yuv420.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile1.10bpc.yuv444.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile1.10bpc.yuv444.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile1.8bpc.yuv444.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile1.8bpc.yuv444.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.10bpc.yuv422.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.10bpc.yuv422.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.10bpc.yuv422.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.10bpc.yuv422.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.12bpc.yuv420.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.12bpc.yuv420.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.12bpc.yuv420.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.12bpc.yuv420.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.12bpc.yuv422.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.12bpc.yuv422.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.12bpc.yuv422.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.12bpc.yuv422.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.12bpc.yuv444.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.12bpc.yuv444.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.12bpc.yuv444.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.12bpc.yuv444.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.8bpc.yuv422.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.8bpc.yuv422.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.8bpc.yuv422.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-large.profile2.8bpc.yuv422.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile0.10bpc.yuv420.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile0.10bpc.yuv420.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile0.10bpc.yuv420.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile0.10bpc.yuv420.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile0.8bpc.yuv420.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile0.8bpc.yuv420.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile0.8bpc.yuv420.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile0.8bpc.yuv420.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile1.10bpc.yuv444.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile1.10bpc.yuv444.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile1.8bpc.yuv444.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile1.8bpc.yuv444.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.10bpc.yuv422.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.10bpc.yuv422.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.10bpc.yuv422.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.10bpc.yuv422.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.12bpc.yuv420.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.12bpc.yuv420.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.12bpc.yuv420.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.12bpc.yuv420.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.12bpc.yuv422.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.12bpc.yuv422.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.12bpc.yuv422.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.12bpc.yuv422.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.12bpc.yuv444.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.12bpc.yuv444.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.12bpc.yuv444.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.12bpc.yuv444.alpha-limited.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.8bpc.yuv422.alpha-full.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.8bpc.yuv422.alpha-full.monochrome.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.8bpc.yuv422.alpha-limited.avif",
    "link-u-avif-sample-images/plum-blossom-small.profile2.8bpc.yuv422.alpha-limited.monochrome.avif",
];
static AVIF_CORRUPT_IMAGES_DIR: &str = "tests/corrupt";
// The 1 frame h263 3gp file can be generated by ffmpeg with command
// "ffmpeg -i [input file] -f 3gp -vcodec h263 -vf scale=176x144 -frames:v 1 -an output.3gp"
static VIDEO_H263_3GP: &str = "tests/bbb_sunflower_QCIF_30fps_h263_noaudio_1f.3gp";
// The 1 frame AMR-NB 3gp file can be generated by ffmpeg with command
// "ffmpeg -i [input file] -f 3gp -acodec amr_nb -ar 8000 -ac 1 -frames:a 1 -vn output.3gp"
#[cfg(feature = "3gpp")]
static AUDIO_AMRNB_3GP: &str = "tests/amr_nb_1f.3gp";
// The 1 frame AMR-WB 3gp file can be generated by ffmpeg with command
// "ffmpeg -i [input file] -f 3gp -acodec amr_wb -ar 16000 -ac 1 -frames:a 1 -vn output.3gp"
#[cfg(feature = "3gpp")]
static AUDIO_AMRWB_3GP: &str = "tests/amr_wb_1f.3gp";
#[cfg(feature = "mp4v")]
// The 1 frame mp4v mp4 file can be generated by ffmpeg with command
// "ffmpeg -i [input file] -f mp4 -c:v mpeg4 -vf scale=176x144 -frames:v 1 -an output.mp4"
static VIDEO_MP4V_MP4: &str = "tests/bbb_sunflower_QCIF_30fps_mp4v_noaudio_1f.mp4";

// Adapted from https://github.com/GuillaumeGomez/audio-video-metadata/blob/9dff40f565af71d5502e03a2e78ae63df95cfd40/src/metadata.rs#L53
#[test]
fn public_api() {
    let mut fd = File::open(MINI_MP4).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
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
                assert!(!tkhd.disabled);
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
                        mp4::VideoCodecSpecific::H263Config(ref _h263) => {
                            "H263"
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
                assert!(!tkhd.disabled);
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
                        #[cfg(feature = "3gpp")]
                        mp4::AudioCodecSpecific::AMRSpecificBox(_) => {
                            "AMR"
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
fn public_metadata() {
    let mut fd = File::open(MINI_MP4_WITH_METADATA).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
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
    assert!(meta.compilation.unwrap());
    assert!(!meta.gapless_playback.unwrap());
    assert!(!meta.podcast.unwrap());
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
    assert!(meta.hd_video.unwrap());
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
fn public_metadata_gnre() {
    let mut fd = File::open(MINI_MP4_WITH_METADATA_STD_GENRE).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
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
    assert!(meta.compilation.unwrap());
    assert!(!meta.gapless_playback.unwrap());
    assert!(!meta.podcast.unwrap());
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
    assert!(meta.hd_video.unwrap());
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
fn public_invalid_metadata() {
    // Test that reading userdata containing invalid metadata is not fatal to parsing and that
    // expected values are still found.
    let mut fd = File::open(VIDEO_INVALID_USERDATA).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
    // Should have userdata.
    assert!(context.userdata.is_some());
    // But it should contain an error.
    assert!(context.userdata.unwrap().is_err());
    // Smoke test that other data has been parsed. Don't check everything, just make sure some
    // values are as expected.
    assert_eq!(context.tracks.len(), 2);
    for track in context.tracks {
        match track.track_type {
            mp4::TrackType::Video => {
                // Check some of the values in the video tkhd.
                let tkhd = track.tkhd.unwrap();
                assert!(!tkhd.disabled);
                assert_eq!(tkhd.duration, 231232);
                assert_eq!(tkhd.width, 83_886_080);
                assert_eq!(tkhd.height, 47_185_920);
            }
            mp4::TrackType::Audio => {
                // Check some of the values in the audio tkhd.
                let tkhd = track.tkhd.unwrap();
                assert!(!tkhd.disabled);
                assert_eq!(tkhd.duration, 231338);
                assert_eq!(tkhd.width, 0);
                assert_eq!(tkhd.height, 0);
            }
            _ => panic!("File should not contain other tracks."),
        }
    }
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
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
    for track in context.tracks {
        let stsd = track.stsd.expect("expected an stsd");
        let a = match stsd.descriptions.first().expect("expected a SampleEntry") {
            mp4::SampleEntry::Audio(a) => a,
            _ => panic!("expected a AudioSampleEntry"),
        };
        assert_eq!(a.codec_type, mp4::CodecType::EncryptedAudio);
        match a.protection_info.iter().find(|sinf| sinf.tenc.is_some()) {
            Some(p) => {
                assert_eq!(p.original_format, b"mp4a");
                if let Some(ref schm) = p.scheme_type {
                    assert_eq!(schm.scheme_type, b"cenc");
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
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
    for track in context.tracks {
        let stsd = track.stsd.expect("expected an stsd");
        let v = match stsd.descriptions.first().expect("expected a SampleEntry") {
            mp4::SampleEntry::Video(ref v) => v,
            _ => panic!("expected a VideoSampleEntry"),
        };
        assert_eq!(v.codec_type, mp4::CodecType::EncryptedVideo);
        match v.protection_info.iter().find(|sinf| sinf.tenc.is_some()) {
            Some(p) => {
                assert_eq!(p.original_format, b"avc1");
                if let Some(ref schm) = p.scheme_type {
                    assert_eq!(schm.scheme_type, b"cenc");
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
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
    for track in context.tracks {
        let stsd = track.stsd.expect("expected an stsd");
        assert_eq!(stsd.descriptions.len(), 2);
        let mut found_encrypted_sample_description = false;
        for description in stsd.descriptions {
            match description {
                mp4::SampleEntry::Audio(ref a) => {
                    if let Some(p) = a.protection_info.iter().find(|sinf| sinf.tenc.is_some()) {
                        found_encrypted_sample_description = true;
                        assert_eq!(p.original_format, b"mp4a");
                        if let Some(ref schm) = p.scheme_type {
                            assert_eq!(schm.scheme_type, b"cbcs");
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
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
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
                        assert_eq!(p.original_format, b"avc1");
                        if let Some(ref schm) = p.scheme_type {
                            assert_eq!(schm.scheme_type, b"cbcs");
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
fn public_video_av1() {
    let mut fd = File::open(VIDEO_AV1_MP4).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
    for track in context.tracks {
        // track part
        assert_eq!(track.duration, Some(mp4::TrackScaledTime(512, 0)));
        assert_eq!(track.empty_duration, Some(mp4::MediaScaledTime(0)));
        assert_eq!(track.media_time, Some(mp4::TrackScaledTime(0, 0)));
        assert_eq!(track.timescale, Some(mp4::TrackTimeScale(12288, 0)));

        // track.tkhd part
        let tkhd = track.tkhd.unwrap();
        assert!(!tkhd.disabled);
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
                assert!(!av1c.monochrome);
                assert_eq!(av1c.chroma_subsampling_x, 1);
                assert_eq!(av1c.chroma_subsampling_y, 1);
                assert_eq!(av1c.chroma_sample_position, 0);
                assert!(!av1c.initial_presentation_delay_present);
                assert_eq!(av1c.initial_presentation_delay_minus_one, 0);
            }
            _ => panic!("Invalid test condition"),
        }
    }
}

#[test]
fn public_mp4_bug_1185230() {
    let input = &mut File::open("tests/test_case_1185230.mp4").expect("Unknown file");
    let context = mp4::read_mp4(input).expect("read_mp4 failed");
    let number_video_tracks = context
        .tracks
        .iter()
        .filter(|t| t.track_type == mp4::TrackType::Video)
        .count();
    let number_audio_tracks = context
        .tracks
        .iter()
        .filter(|t| t.track_type == mp4::TrackType::Audio)
        .count();
    assert_eq!(number_video_tracks, 2);
    assert_eq!(number_audio_tracks, 2);
}

#[test]
fn public_mp4_ctts_overflow() {
    let input = &mut File::open("tests/clusterfuzz-testcase-minimized-mp4-6093954524250112")
        .expect("Unknown file");
    assert_invalid_data(mp4::read_mp4(input), "insufficient data in 'ctts' box");
}

#[test]
fn public_avif_primary_item() {
    let input = &mut File::open(IMAGE_AVIF).expect("Unknown file");
    let context = mp4::read_avif(input, ParseStrictness::Normal).expect("read_avif failed");
    assert_eq!(
        context.primary_item_coded_data().unwrap(),
        [
            0x12, 0x00, 0x0A, 0x07, 0x38, 0x00, 0x06, 0x90, 0x20, 0x20, 0x69, 0x32, 0x0C, 0x16,
            0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x79, 0x4C, 0xD2, 0x02
        ]
    );
}

#[test]
fn public_avif_primary_item_split_extents() {
    let input = &mut File::open(IMAGE_AVIF_EXTENTS).expect("Unknown file");
    let context = mp4::read_avif(input, ParseStrictness::Normal).expect("read_avif failed");
    assert_eq!(context.primary_item_coded_data().unwrap().len(), 52);
}

#[test]
fn public_avif_alpha_item() {
    for_strictness_result(IMAGE_AVIF_ALPHA, |_strictness, result| {
        assert!(result.is_ok());
    });
}

#[test]
fn public_avif_alpha_non_premultiplied() {
    for_strictness_result(IMAGE_AVIF_ALPHA, |_strictness, result| {
        let context = result.expect("read_avif failed");
        assert!(context.primary_item_coded_data().is_some());
        assert!(context.alpha_item_coded_data().is_some());
        assert!(!context.premultiplied_alpha);
    });
}

#[test]
fn public_avif_alpha_premultiplied() {
    for_strictness_result(IMAGE_AVIF_ALPHA_PREMULTIPLIED, |_strictness, result| {
        let context = result.expect("read_avif failed");
        assert!(context.primary_item_coded_data().is_some());
        assert!(context.alpha_item_coded_data().is_some());
        assert!(context.premultiplied_alpha);
    });
}

#[test]
fn public_avif_bug_1655846() {
    let input = &mut File::open(IMAGE_AVIF_CORRUPT).expect("Unknown file");
    assert!(mp4::read_avif(input, ParseStrictness::Normal).is_err());
}

#[test]
fn public_avif_bug_1661347() {
    let input = &mut File::open(IMAGE_AVIF_CORRUPT_2).expect("Unknown file");
    assert!(mp4::read_avif(input, ParseStrictness::Normal).is_err());
}

fn assert_invalid_data<T: std::fmt::Debug>(result: mp4::Result<T>, expected_msg: &str) {
    match result {
        Err(Error::InvalidData(msg)) if msg == expected_msg => {}
        Err(Error::InvalidData(msg)) if msg != expected_msg => {
            panic!(
                "Error message mismatch\nExpected: {}\nFound:    {}",
                expected_msg, msg
            );
        }
        r => panic!(
            "Expected Err(Error::InvalidData({:?}), found {:?}",
            expected_msg, r
        ),
    }
}

fn for_strictness_result(
    path: &str,
    check: impl Fn(ParseStrictness, mp4::Result<mp4::AvifContext>),
) {
    let input = &mut File::open(path).expect("Unknown file");

    for strictness in [
        ParseStrictness::Permissive,
        ParseStrictness::Normal,
        ParseStrictness::Strict,
    ] {
        input.seek(SeekFrom::Start(0)).expect("rewind failed");
        check(strictness, mp4::read_avif(input, strictness));
    }
}

/// Check that input generates the expected error only in strict parsing mode
fn assert_avif_should(path: &str, expected_msg: &str) {
    let input = &mut File::open(path).expect("Unknown file");
    assert_invalid_data(mp4::read_avif(input, ParseStrictness::Strict), expected_msg);
    input.seek(SeekFrom::Start(0)).expect("rewind failed");
    mp4::read_avif(input, ParseStrictness::Normal).expect("ParseStrictness::Normal failed");
    input.seek(SeekFrom::Start(0)).expect("rewind failed");
    mp4::read_avif(input, ParseStrictness::Permissive).expect("ParseStrictness::Permissive failed");
}

/// Check that input generates the expected error only in strict parsing mode
// TODO: replace assert_avif_should
fn assert_avif_should2(path: &str, expected: Status) {
    for_strictness_result(path, |strictness, result| {
        if strictness == ParseStrictness::Strict {
            assert_eq!(expected, Status::from(result));
        } else {
            assert!(result.is_ok());
        }
    })
}

/// Check that input generates the expected error unless in permissive parsing mode
fn assert_avif_shall(path: &str, expected_msg: &str) {
    let input = &mut File::open(path).expect("Unknown file");
    assert_invalid_data(mp4::read_avif(input, ParseStrictness::Strict), expected_msg);
    input.seek(SeekFrom::Start(0)).expect("rewind failed");
    assert_invalid_data(mp4::read_avif(input, ParseStrictness::Normal), expected_msg);
    input.seek(SeekFrom::Start(0)).expect("rewind failed");
    mp4::read_avif(input, ParseStrictness::Permissive).expect("ParseStrictness::Permissive failed");
}

// TODO: replace assert_avif_shall
fn assert_avif_shall2(path: &str, expected: Status) {
    for_strictness_result(path, |strictness, result| {
        if strictness == ParseStrictness::Permissive {
            assert!(result.is_ok());
        } else {
            assert_eq!(expected, Status::from(result));
        }
    })
}

// Technically all transforms shall be essential, but this appears likely to change
// so we only enforce it in strict parsing
// See https://github.com/mozilla/mp4parse-rust/issues/284

#[test]
fn public_avif_av1c_missing_essential() {
    assert_avif_should2(IMAGE_AVIF_AV1C_MISSING_ESSENTIAL, Status::TxformNoEssential);
}

#[test]
fn public_avif_clap_missing_essential() {
    for_strictness_result(IMAGE_AVIF_CLAP_MISSING_ESSENTIAL, |strictness, result| {
        if strictness == ParseStrictness::Strict {
            assert_eq!(Status::TxformNoEssential, Status::from(result));
        } else {
            assert_unsupported_nonfatal(&result, mp4::Feature::Clap);
        }
    })
}

#[test]
fn public_avif_imir_missing_essential() {
    assert_avif_should2(IMAGE_AVIF_IMIR_MISSING_ESSENTIAL, Status::TxformNoEssential);
}

#[test]
fn public_avif_irot_missing_essential() {
    assert_avif_should2(IMAGE_AVIF_IROT_MISSING_ESSENTIAL, Status::TxformNoEssential);
}

#[test]
fn public_avif_ipma_bad_version() {
    let expected_msg = "The ipma version 0 should be used unless 32-bit \
                        item_ID values are needed \
                        per ISOBMFF (ISO 14496-12:2020 § 8.11.14.1";
    assert_avif_should(IMAGE_AVIF_IPMA_BAD_VERSION, expected_msg);
}

#[test]
fn public_avif_ipma_bad_flags() {
    let expected_msg = "Unless there are more than 127 properties in the \
                        ItemPropertyContainerBox, flags should be equal to 0 \
                        per ISOBMFF (ISO 14496-12:2020 § 8.11.14.1";
    assert_avif_should(IMAGE_AVIF_IPMA_BAD_FLAGS, expected_msg);
}

#[test]
fn public_avif_ipma_duplicate_version_and_flags() {
    let expected_msg = "There shall be at most one ItemPropertyAssociationbox \
                        with a given pair of values of version and flags \
                        per ISOBMFF (ISO 14496-12:2020 § 8.11.14.1";
    assert_avif_shall(IMAGE_AVIF_IPMA_DUPLICATE_VERSION_AND_FLAGS, expected_msg);
}

#[test]
// TODO: convert this to a `assert_avif_shall` test to cover all `ParseStrictness` modes
// that would require crafting an input that validly uses multiple ipma boxes,
// which is kind of annoying to make pass the "should" requirements on flags and version
// as well as the "shall" requirement on duplicate version and flags
fn public_avif_ipma_duplicate_item_id() {
    let expected_msg = "There shall be at most one occurrence of a given item_ID, \
                        in the set of ItemPropertyAssociationBox boxes \
                        per ISOBMFF (ISO 14496-12:2020) § 8.11.14.1";
    let input = &mut File::open(IMAGE_AVIF_IPMA_DUPLICATE_ITEM_ID).expect("Unknown file");
    assert_invalid_data(
        mp4::read_avif(input, ParseStrictness::Permissive),
        expected_msg,
    )
}

#[test]
fn public_avif_ipma_invalid_property_index() {
    let expected_msg = "Invalid property index in ipma";
    assert_avif_shall(IMAGE_AVIF_IPMA_INVALID_PROPERTY_INDEX, expected_msg);
}

#[test]
fn public_avif_hdlr_first_in_meta() {
    let expected_msg = "The HandlerBox shall be the first contained box within \
                        the MetaBox \
                        per MIAF (ISO 23000-22:2019) § 7.2.1.5";
    assert_avif_shall(IMAGE_AVIF_NO_HDLR, expected_msg);
    assert_avif_shall(IMAGE_AVIF_HDLR_NOT_FIRST, expected_msg);
}

#[test]
fn public_avif_hdlr_is_pict() {
    let expected_msg = "The HandlerBox handler_type must be 'pict' \
                        per MIAF (ISO 23000-22:2019) § 7.2.1.5";
    assert_avif_shall(IMAGE_AVIF_HDLR_NOT_PICT, expected_msg);
}

#[test]
fn public_avif_hdlr_nonzero_reserved() {
    let expected_msg = "The HandlerBox 'reserved' fields shall be 0 \
                        per ISOBMFF (ISO 14496-12:2020) § 8.4.3.2";
    // This is a "should" despite the spec indicating a (somewhat ambiguous)
    // requirement that this field is set to zero.
    // See comments in read_hdlr
    assert_avif_should(IMAGE_AVIF_HDLR_NONZERO_RESERVED, expected_msg);
}

#[test]
fn public_avif_hdlr_multiple_nul() {
    let expected_msg = "The HandlerBox 'name' field shall have a NUL byte \
                        only in the final position \
                        per ISOBMFF (ISO 14496-12:2020) § 8.4.3.2";

    // This is a "should" despite the spec indicating a (somewhat ambiguous)
    // requirement about extra data in boxes
    // See comments in read_hdlr
    assert_avif_should(IMAGE_AVIF_HDLR_MULTIPLE_NUL, expected_msg);
}

#[test]
fn public_avif_no_mif1() {
    let expected_msg = "The FileTypeBox should contain 'mif1' in the compatible_brands list \
                        per MIAF (ISO 23000-22:2019) § 7.2.1.2";
    assert_avif_should(IMAGE_AVIF_NO_MIF1, expected_msg);
}

#[test]
fn public_avif_no_pitm() {
    assert_avif_shall2(IMAGE_AVIF_NO_PITM, Status::NoPrimaryItem);
}

#[test]
fn public_avif_pixi_present_for_displayable_images() {
    let expected_msg = "The pixel information property shall be associated with every image \
                        that is displayable (not hidden) \
                        per MIAF (ISO/IEC 23000-22:2019) specification § 7.3.6.6";
    let pixi_test = if cfg!(feature = "missing-pixi-permitted") {
        assert_avif_should
    } else {
        assert_avif_shall
    };

    pixi_test(IMAGE_AVIF_NO_PIXI, expected_msg);
    pixi_test(IMAGE_AVIF_NO_ALPHA_PIXI, expected_msg);
}

#[test]
fn public_avif_av1c_present_for_av01() {
    let expected_msg = "One AV1 Item Configuration Property (av1C) \
                        is mandatory for an image item of type 'av01' \
                        per AVIF specification § 2.2.1";
    assert_avif_shall(IMAGE_AVIF_NO_AV1C, expected_msg);
    assert_avif_shall(IMAGE_AVIF_NO_ALPHA_AV1C, expected_msg);
}

#[test]
fn public_avif_ispe_present() {
    let expected_msg = "Missing 'ispe' property for primary item, required \
                        per HEIF (ISO/IEC 23008-12:2017) § 6.5.3.1";
    assert_avif_shall(IMAGE_AVIF_NO_ISPE, expected_msg);
}

fn assert_unsupported_nonfatal(result: &mp4::Result<mp4::AvifContext>, feature: mp4::Feature) {
    match result {
        Ok(context) => {
            assert!(
                context.unsupported_features.contains(feature),
                "context.unsupported_features missing expected {:?}",
                feature
            );
        }
        r => panic!(
            "Expected Ok with unsupported_features containing {:?}, found {:?}",
            feature, r
        ),
    }
}

// Assert that across all strictness levels the given feature is tracked as
// being used, but unsupported. Additionally, if the feature is essential,
// assert that the primary item is not processed unless using permissive mode.
// TODO: Add similar tests for alpha
fn assert_unsupported(path: &str, feature: mp4::Feature, essential: bool) {
    for_strictness_result(path, |strictness, result| {
        assert_unsupported_nonfatal(&result, feature);
        match result {
            Ok(context) if essential => assert_eq!(
                context.primary_item_coded_data().is_some(),
                strictness == ParseStrictness::Permissive
            ),
            Ok(context) if !essential => assert!(context.primary_item_coded_data().is_some()),
            _ => panic!("Expected Ok, got {:?}", result),
        }
    });
}

fn assert_unsupported_nonessential(path: &str, feature: mp4::Feature) {
    assert_unsupported(path, feature, false);
}

fn assert_unsupported_essential(path: &str, feature: mp4::Feature) {
    assert_unsupported(path, feature, true);
}

#[test]
fn public_avif_a1lx() {
    assert_unsupported_nonessential(AVIF_A1LX, mp4::Feature::A1lx);
}

#[test]
fn public_avif_a1lx_marked_essential() {
    assert_avif_shall2(IMAGE_AVIF_A1LX_MARKED_ESSENTIAL, Status::A1lxEssential);
}

#[test]
fn public_avif_a1op() {
    assert_unsupported_essential(AVIF_A1OP, mp4::Feature::A1op);
}

#[test]
fn public_avif_a1op_missing_essential() {
    assert_avif_shall2(IMAGE_AVIF_A1OP_MISSING_ESSENTIAL, Status::A1opNoEssential);
}

#[test]
fn public_avif_lsel() {
    assert_unsupported_essential(AVIF_LSEL, mp4::Feature::Lsel);
}

#[test]
fn public_avif_lsel_missing_essential() {
    assert_avif_shall2(IMAGE_AVIF_LSEL_MISSING_ESSENTIAL, Status::LselNoEssential);
}

#[test]
fn public_avif_clap() {
    assert_unsupported_essential(AVIF_CLAP, mp4::Feature::Clap);
}

#[test]
fn public_avif_grid() {
    for file in &[AVIF_GRID, AVIF_GRID_A1LX] {
        let input = &mut File::open(file).expect(file);
        assert_unsupported_nonfatal(
            &mp4::read_avif(input, ParseStrictness::Normal),
            mp4::Feature::Grid,
        );
    }
}

#[test]
fn public_avis_major_no_pitm() {
    let input = &mut File::open(AVIF_AVIS_MAJOR_NO_PITM).expect("Unknown file");
    match mp4::read_avif(input, ParseStrictness::Normal) {
        Ok(context) => {
            assert_eq!(context.major_brand, mp4::AVIS_BRAND);
            assert!(context.unsupported_features.contains(mp4::Feature::Avis));
            assert!(context.primary_item_coded_data().is_none());
        }
        Err(e) => panic!("Expected Ok(_), found {:?}", e),
    }
}

#[test]
fn public_avis_major_with_pitm_and_alpha() {
    let input = &mut File::open(AVIF_AVIS_MAJOR_WITH_PITM_AND_ALPHA).expect("Unknown file");
    match mp4::read_avif(input, ParseStrictness::Normal) {
        Ok(context) => {
            assert_eq!(context.major_brand, mp4::AVIS_BRAND);
            assert!(context.unsupported_features.contains(mp4::Feature::Avis));
            assert!(context.primary_item_coded_data().is_some());
            assert!(context.alpha_item_coded_data().is_some());
        }
        Err(e) => panic!("Expected Ok(_), found {:?}", e),
    }
}

#[test]
fn public_avif_avis_major_no_moov() {
    assert_avif_shall2(AVIF_AVIS_MAJOR_NO_MOOV, Status::NoMoov);
}

#[test]
fn public_avif_read_samples() {
    public_avif_read_samples_impl(ParseStrictness::Normal);
}

#[test]
#[ignore] // See https://github.com/AOMediaCodec/av1-avif/issues/146
fn public_avif_read_samples_strict() {
    public_avif_read_samples_impl(ParseStrictness::Strict);
}

fn to_canonical_paths(strs: &[&str]) -> Vec<std::path::PathBuf> {
    strs.iter()
        .map(std::fs::canonicalize)
        .map(Result::unwrap)
        .collect()
}

fn public_avif_read_samples_impl(strictness: ParseStrictness) {
    let corrupt_images = to_canonical_paths(AV1_AVIF_CORRUPT_IMAGES);
    let unsupported_images = to_canonical_paths(AVIF_UNSUPPORTED_IMAGES);
    let legal_no_pixi_images = if cfg!(feature = "missing-pixi-permitted") {
        to_canonical_paths(AVIF_NO_PIXI_IMAGES)
    } else {
        vec![]
    };
    for dir in AVIF_TEST_DIRS {
        for entry in walkdir::WalkDir::new(dir) {
            let entry = entry.expect("AVIF entry");
            let path = entry.path();
            let extension = path.extension().unwrap_or_default();
            if !path.is_file() || (extension != "avif" && extension != "avifs") {
                eprintln!("Skipping {:?}", path);
                continue; // Skip directories, ReadMe.txt, etc.
            }
            let corrupt = (path.canonicalize().unwrap().parent().unwrap()
                == std::fs::canonicalize(AVIF_CORRUPT_IMAGES_DIR).unwrap()
                || corrupt_images.contains(&path.canonicalize().unwrap()))
                && !legal_no_pixi_images.contains(&path.canonicalize().unwrap());

            let unsupported = unsupported_images.contains(&path.canonicalize().unwrap());
            println!(
                "parsing {}{}{:?}",
                if corrupt { "(corrupt) " } else { "" },
                if unsupported { "(unsupported) " } else { "" },
                path,
            );
            let input = &mut File::open(path).expect("Unknow file");
            match mp4::read_avif(input, strictness) {
                Ok(c) if unsupported || corrupt => {
                    if unsupported {
                        assert!(!c.unsupported_features.is_empty());
                    } else {
                        panic!("Expected error parsing {:?}, found:\n{:?}", path, c)
                    }
                }
                Ok(c) => {
                    assert!(
                        c.unsupported_features.is_empty(),
                        "{:?}",
                        c.unsupported_features
                    );
                    eprintln!("Successfully parsed {:?}", path)
                }
                Err(e) if corrupt => {
                    eprintln!("Expected error parsing corrupt input {:?}: {:?}", path, e)
                }
                Err(e) => panic!("Unexpected error parsing {:?}: {:?}", path, e),
            }
        }
    }
}

#[test]
fn public_video_h263() {
    let mut fd = File::open(VIDEO_H263_3GP).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
    for track in context.tracks {
        let stsd = track.stsd.expect("expected an stsd");
        let v = match stsd.descriptions.first().expect("expected a SampleEntry") {
            mp4::SampleEntry::Video(ref v) => v,
            _ => panic!("expected a VideoSampleEntry"),
        };
        assert_eq!(v.codec_type, mp4::CodecType::H263);
        assert_eq!(v.width, 176);
        assert_eq!(v.height, 144);
        let _codec_specific = match v.codec_specific {
            mp4::VideoCodecSpecific::H263Config(_) => true,
            _ => {
                panic!("expected a H263Config",);
            }
        };
    }
}

#[test]
#[cfg(feature = "3gpp")]
fn public_audio_amrnb() {
    let mut fd = File::open(AUDIO_AMRNB_3GP).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
    for track in context.tracks {
        let stsd = track.stsd.expect("expected an stsd");
        let a = match stsd.descriptions.first().expect("expected a SampleEntry") {
            mp4::SampleEntry::Audio(ref v) => v,
            _ => panic!("expected a AudioSampleEntry"),
        };
        assert!(a.codec_type == mp4::CodecType::AMRNB);
        let _codec_specific = match a.codec_specific {
            mp4::AudioCodecSpecific::AMRSpecificBox(_) => true,
            _ => {
                panic!("expected a AMRSpecificBox",);
            }
        };
    }
}

#[test]
#[cfg(feature = "3gpp")]
fn public_audio_amrwb() {
    let mut fd = File::open(AUDIO_AMRWB_3GP).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
    for track in context.tracks {
        let stsd = track.stsd.expect("expected an stsd");
        let a = match stsd.descriptions.first().expect("expected a SampleEntry") {
            mp4::SampleEntry::Audio(ref v) => v,
            _ => panic!("expected a AudioSampleEntry"),
        };
        assert!(a.codec_type == mp4::CodecType::AMRWB);
        let _codec_specific = match a.codec_specific {
            mp4::AudioCodecSpecific::AMRSpecificBox(_) => true,
            _ => {
                panic!("expected a AMRSpecificBox",);
            }
        };
    }
}

#[test]
#[cfg(feature = "mp4v")]
fn public_video_mp4v() {
    let mut fd = File::open(VIDEO_MP4V_MP4).expect("Unknown file");
    let mut buf = Vec::new();
    fd.read_to_end(&mut buf).expect("File error");

    let mut c = Cursor::new(&buf);
    let context = mp4::read_mp4(&mut c).expect("read_mp4 failed");
    for track in context.tracks {
        let stsd = track.stsd.expect("expected an stsd");
        let v = match stsd.descriptions.first().expect("expected a SampleEntry") {
            mp4::SampleEntry::Video(ref v) => v,
            _ => panic!("expected a VideoSampleEntry"),
        };
        assert_eq!(v.codec_type, mp4::CodecType::MP4V);
        assert_eq!(v.width, 176);
        assert_eq!(v.height, 144);
        let _codec_specific = match v.codec_specific {
            mp4::VideoCodecSpecific::ESDSConfig(_) => true,
            _ => {
                panic!("expected a ESDSConfig",);
            }
        };
    }
}
