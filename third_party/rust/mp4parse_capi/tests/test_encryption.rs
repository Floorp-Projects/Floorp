extern crate mp4parse_capi;
use mp4parse_capi::*;
use std::io::Read;

extern "C" fn buf_read(buf: *mut u8, size: usize, userdata: *mut std::os::raw::c_void) -> isize {
    let input: &mut std::fs::File = unsafe { &mut *(userdata as *mut _) };
    let mut buf = unsafe { std::slice::from_raw_parts_mut(buf, size) };
    match input.read(&mut buf) {
        Ok(n) => n as isize,
        Err(_) => -1,
    }
}

#[test]
#[allow(clippy::cognitive_complexity)] // TODO: Consider simplifying this
fn parse_cenc() {
    let mut file = std::fs::File::open("tests/short-cenc.mp4").expect("Unknown file");
    let io = Mp4parseIo {
        read: Some(buf_read),
        userdata: &mut file as *mut _ as *mut std::os::raw::c_void,
    };

    unsafe {
        let mut parser = std::ptr::null_mut();
        let mut rv = mp4parse_new(&io, &mut parser);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert!(!parser.is_null());
        let mut counts: u32 = 0;
        rv = mp4parse_get_track_count(parser, &mut counts);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(counts, 2);

        // Make sure we have a video track and it's at index 0
        let mut video_track_info = Mp4parseTrackInfo::default();
        rv = mp4parse_get_track_info(parser, 0, &mut video_track_info);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(video_track_info.track_type, Mp4parseTrackType::Video);

        // Make sure we have a audio track and it's at index 1
        let mut audio_track_info = Mp4parseTrackInfo::default();
        rv = mp4parse_get_track_info(parser, 1, &mut audio_track_info);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(audio_track_info.track_type, Mp4parseTrackType::Audio);

        // Verify video track and crypto information
        let mut video = Mp4parseTrackVideoInfo::default();
        rv = mp4parse_get_track_video_info(parser, 0, &mut video);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(video.sample_info_count, 1);
        assert_eq!((*video.sample_info).codec_type, Mp4parseCodec::Avc);
        assert_eq!((*video.sample_info).image_width, 320);
        assert_eq!((*video.sample_info).image_height, 240);
        let protected_data = &(*video.sample_info).protected_data;
        assert_eq!(
            protected_data.scheme_type,
            Mp4ParseEncryptionSchemeType::Cenc
        );
        assert_eq!(protected_data.is_encrypted, 0x01);
        assert_eq!(protected_data.iv_size, 16);
        assert_eq!(protected_data.kid.length, 16);
        let expected_kid = [
            0x7e, 0x57, 0x1d, 0x01, 0x7e, 0x57, 0x1d, 0x01, 0x7e, 0x57, 0x1d, 0x01, 0x7e, 0x57,
            0x1d, 0x01,
        ];
        for (i, expected_byte) in expected_kid.iter().enumerate() {
            assert_eq!(&(*protected_data.kid.data.add(i)), expected_byte);
        }
        assert_eq!(protected_data.crypt_byte_block, 0);
        assert_eq!(protected_data.skip_byte_block, 0);
        assert_eq!(protected_data.constant_iv.length, 0);

        // Verify audio track and crypto information
        let mut audio = Mp4parseTrackAudioInfo::default();
        rv = mp4parse_get_track_audio_info(parser, 1, &mut audio);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(audio.sample_info_count, 1);
        assert_eq!((*audio.sample_info).codec_type, Mp4parseCodec::Aac);
        assert_eq!((*audio.sample_info).channels, 2);
        assert_eq!((*audio.sample_info).bit_depth, 16);
        assert_eq!((*audio.sample_info).sample_rate, 44100);
        let protected_data = &(*audio.sample_info).protected_data;
        assert_eq!(protected_data.is_encrypted, 0x01);
        assert_eq!(protected_data.iv_size, 16);
        assert_eq!(protected_data.kid.length, 16);
        let expected_kid = [
            0x7e, 0x57, 0x1d, 0x02, 0x7e, 0x57, 0x1d, 0x02, 0x7e, 0x57, 0x1d, 0x02, 0x7e, 0x57,
            0x1d, 0x02,
        ];
        for (i, expected_byte) in expected_kid.iter().enumerate() {
            assert_eq!(&(*protected_data.kid.data.add(i)), expected_byte);
        }
        assert_eq!(protected_data.crypt_byte_block, 0);
        assert_eq!(protected_data.skip_byte_block, 0);
        assert_eq!(protected_data.constant_iv.length, 0);
    }
}

#[test]
fn parse_cbcs() {
    let mut file = std::fs::File::open("tests/bipbop_cbcs_video_init.mp4").expect("Unknown file");
    let io = Mp4parseIo {
        read: Some(buf_read),
        userdata: &mut file as *mut _ as *mut std::os::raw::c_void,
    };

    unsafe {
        let mut parser = std::ptr::null_mut();
        let mut rv = mp4parse_new(&io, &mut parser);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert!(!parser.is_null());
        let mut counts: u32 = 0;
        rv = mp4parse_get_track_count(parser, &mut counts);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(counts, 1);

        // Make sure we have a video track
        let mut video_track_info = Mp4parseTrackInfo::default();
        rv = mp4parse_get_track_info(parser, 0, &mut video_track_info);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(video_track_info.track_type, Mp4parseTrackType::Video);

        // Verify video track and crypto information
        let mut video = Mp4parseTrackVideoInfo::default();
        rv = mp4parse_get_track_video_info(parser, 0, &mut video);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(video.sample_info_count, 2);
        assert_eq!((*video.sample_info).codec_type, Mp4parseCodec::Avc);
        assert_eq!((*video.sample_info).image_width, 400);
        assert_eq!((*video.sample_info).image_height, 300);
        let protected_data = &(*video.sample_info).protected_data;
        assert_eq!(
            protected_data.scheme_type,
            Mp4ParseEncryptionSchemeType::Cbcs
        );
        assert_eq!(protected_data.is_encrypted, 0x01);
        assert_eq!(protected_data.iv_size, 0);
        assert_eq!(protected_data.kid.length, 16);
        let expected_kid = [
            0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57, 0x1d, 0x04, 0x7e, 0x57,
            0x1d, 0x21,
        ];
        for (i, expected_byte) in expected_kid.iter().enumerate() {
            assert_eq!(&(*protected_data.kid.data.add(i)), expected_byte);
        }
        assert_eq!(protected_data.crypt_byte_block, 1);
        assert_eq!(protected_data.skip_byte_block, 9);
        assert_eq!(protected_data.constant_iv.length, 16);
        let expected_iv = [
            0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44,
            0x55, 0x66,
        ];
        for (i, expected_byte) in expected_iv.iter().enumerate() {
            assert_eq!(&(*protected_data.constant_iv.data.add(i)), expected_byte);
        }
    }
}

#[test]
fn parse_unencrypted() {
    // Ensure the encryption related data is not populated for files without
    // encryption metadata.
    let mut file = std::fs::File::open("tests/opus_audioinit.mp4").expect("Unknown file");
    let io = Mp4parseIo {
        read: Some(buf_read),
        userdata: &mut file as *mut _ as *mut std::os::raw::c_void,
    };

    unsafe {
        let mut parser = std::ptr::null_mut();
        let mut rv = mp4parse_new(&io, &mut parser);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert!(!parser.is_null());

        let mut counts: u32 = 0;
        rv = mp4parse_get_track_count(parser, &mut counts);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(counts, 1);

        let mut track_info = Mp4parseTrackInfo::default();
        rv = mp4parse_get_track_info(parser, 0, &mut track_info);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(track_info.track_type, Mp4parseTrackType::Audio);

        let mut audio = Mp4parseTrackAudioInfo::default();
        rv = mp4parse_get_track_audio_info(parser, 0, &mut audio);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(audio.sample_info_count, 1);
        let protected_data = &(*audio.sample_info).protected_data;
        assert_eq!(
            protected_data.scheme_type,
            Mp4ParseEncryptionSchemeType::None
        );
        assert_eq!(protected_data.is_encrypted, 0x00);
        assert_eq!(protected_data.iv_size, 0);
        assert_eq!(protected_data.kid.length, 0);
        assert_eq!(protected_data.crypt_byte_block, 0);
        assert_eq!(protected_data.skip_byte_block, 0);
        assert_eq!(protected_data.constant_iv.length, 0);
    }
}
