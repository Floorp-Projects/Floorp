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
fn parse_fragment() {
    let mut file = std::fs::File::open("tests/bipbop_audioinit.mp4").expect("Unknown file");
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
        assert_eq!(track_info.track_id, 1);
        assert_eq!(track_info.duration, 0);
        assert_eq!(track_info.media_time, 0);

        let mut audio = Default::default();
        rv = mp4parse_get_track_audio_info(parser, 0, &mut audio);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(audio.sample_info_count, 1);

        assert_eq!((*audio.sample_info).codec_type, Mp4parseCodec::Aac);
        assert_eq!((*audio.sample_info).channels, 2);
        assert_eq!((*audio.sample_info).bit_depth, 16);
        assert_eq!((*audio.sample_info).sample_rate, 22050);
        assert_eq!((*audio.sample_info).extra_data.length, 27);
        assert_eq!((*audio.sample_info).codec_specific_config.length, 2);

        let mut is_fragmented_file: u8 = 0;
        rv = mp4parse_is_fragmented(parser, track_info.track_id, &mut is_fragmented_file);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(is_fragmented_file, 1);

        let mut fragment_info = Mp4parseFragmentInfo::default();
        rv = mp4parse_get_fragment_info(parser, &mut fragment_info);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(fragment_info.fragment_duration, 10_032_000);

        mp4parse_free(parser);
    }
}

#[test]
fn parse_opus_fragment() {
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
        assert_eq!(track_info.track_id, 1);
        assert_eq!(track_info.duration, 0);
        assert_eq!(track_info.media_time, 0);

        let mut audio = Default::default();
        rv = mp4parse_get_track_audio_info(parser, 0, &mut audio);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(audio.sample_info_count, 1);

        assert_eq!((*audio.sample_info).codec_type, Mp4parseCodec::Opus);
        assert_eq!((*audio.sample_info).channels, 1);
        assert_eq!((*audio.sample_info).bit_depth, 16);
        assert_eq!((*audio.sample_info).sample_rate, 48000);
        assert_eq!((*audio.sample_info).extra_data.length, 0);
        assert_eq!((*audio.sample_info).codec_specific_config.length, 19);

        let mut is_fragmented_file: u8 = 0;
        rv = mp4parse_is_fragmented(parser, track_info.track_id, &mut is_fragmented_file);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(is_fragmented_file, 1);

        let mut fragment_info = Mp4parseFragmentInfo::default();
        rv = mp4parse_get_fragment_info(parser, &mut fragment_info);
        assert_eq!(rv, Mp4parseStatus::Ok);

        mp4parse_free(parser);
    }
}
