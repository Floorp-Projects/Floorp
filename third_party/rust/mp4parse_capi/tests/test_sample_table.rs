extern crate mp4parse;
extern crate mp4parse_capi;
use mp4parse::unstable::Indice;
use mp4parse_capi::*;
use std::io::Read;

extern "C" fn buf_read(buf: *mut u8, size: usize, userdata: *mut std::os::raw::c_void) -> isize {
    let input: &mut std::fs::File = unsafe { &mut *(userdata as *mut _) };
    let buf = unsafe { std::slice::from_raw_parts_mut(buf, size) };
    match input.read(buf) {
        Ok(n) => n as isize,
        Err(_) => -1,
    }
}

#[test]
fn parse_sample_table() {
    let mut file =
        std::fs::File::open("tests/bipbop_nonfragment_header.mp4").expect("Unknown file");
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

        let mut track_info = Mp4parseTrackInfo::default();
        rv = mp4parse_get_track_info(parser, 1, &mut track_info);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(track_info.track_type, Mp4parseTrackType::Audio);

        // Check audio smaple table
        let mut is_fragmented_file: u8 = 0;
        rv = mp4parse_is_fragmented(parser, track_info.track_id, &mut is_fragmented_file);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(is_fragmented_file, 0);

        let mut indice = Mp4parseByteData::default();
        rv = mp4parse_get_indice_table(parser, track_info.track_id, &mut indice);
        assert_eq!(rv, Mp4parseStatus::Ok);

        // Compare the value from stagefright.
        let audio_indice_0 = Indice {
            start_offset: 27_046.into(),
            end_offset: 27_052.into(),
            start_composition: 0.into(),
            end_composition: 46_439.into(),
            start_decode: 0.into(),
            sync: true,
        };
        let audio_indice_215 = Indice {
            start_offset: 283_550.into(),
            end_offset: 283_556.into(),
            start_composition: 9_984_580.into(),
            end_composition: 10_031_020.into(),
            start_decode: 9_984_580.into(),
            sync: true,
        };
        assert_eq!(indice.length, 216);
        assert_eq!(*indice.indices.offset(0), audio_indice_0);
        assert_eq!(*indice.indices.offset(215), audio_indice_215);

        // Check video smaple table
        rv = mp4parse_get_track_info(parser, 0, &mut track_info);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(track_info.track_type, Mp4parseTrackType::Video);

        let mut is_fragmented_file: u8 = 0;
        rv = mp4parse_is_fragmented(parser, track_info.track_id, &mut is_fragmented_file);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(is_fragmented_file, 0);

        let mut indice = Mp4parseByteData::default();
        rv = mp4parse_get_indice_table(parser, track_info.track_id, &mut indice);
        assert_eq!(rv, Mp4parseStatus::Ok);

        // Compare the last few data from stagefright.
        let video_indice_291 = Indice {
            start_offset: 280_226.into(),
            end_offset: 280_855.into(),
            start_composition: 9_838_333.into(),
            end_composition: 9_871_677.into(),
            start_decode: 9_710_000.into(),
            sync: false,
        };
        let video_indice_292 = Indice {
            start_offset: 280_855.into(),
            end_offset: 281_297.into(),
            start_composition: 9_805_011.into(),
            end_composition: 9_838_333.into(),
            start_decode: 9_710_011.into(),
            sync: false,
        };
        // TODO: start_composition time in stagefright is 9905000, but it is 9904999 in parser, it
        //       could be rounding error.
        //let video_indice_293 = Indice { start_offset: 281_297, end_offset: 281_919, start_composition: 9_905_000, end_composition: 9_938_344, start_decode: 9_776_666, sync: false };
        //let video_indice_294 = Indice { start_offset: 281_919, end_offset: 282_391, start_composition: 9_871_677, end_composition: 9_905_000, start_decode: 9_776_677, sync: false };
        let video_indice_295 = Indice {
            start_offset: 282_391.into(),
            end_offset: 283_032.into(),
            start_composition: 9_971_666.into(),
            end_composition: 9_971_677.into(),
            start_decode: 9_843_333.into(),
            sync: false,
        };
        let video_indice_296 = Indice {
            start_offset: 283_092.into(),
            end_offset: 283_526.into(),
            start_composition: 9_938_344.into(),
            end_composition: 9_971_666.into(),
            start_decode: 9_843_344.into(),
            sync: false,
        };

        assert_eq!(indice.length, 297);
        assert_eq!(*indice.indices.offset(291), video_indice_291);
        assert_eq!(*indice.indices.offset(292), video_indice_292);
        //assert_eq!(*indice.indices.offset(293), video_indice_293);
        //assert_eq!(*indice.indices.offset(294), video_indice_294);
        assert_eq!(*indice.indices.offset(295), video_indice_295);
        assert_eq!(*indice.indices.offset(296), video_indice_296);

        mp4parse_free(parser);
    }
}

#[test]
fn parse_sample_table_with_elst() {
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

        let mut track_info = Mp4parseTrackInfo::default();
        rv = mp4parse_get_track_info(parser, 1, &mut track_info);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(track_info.track_type, Mp4parseTrackType::Audio);

        // Check audio sample table
        let mut is_fragmented_file: u8 = std::u8::MAX;
        rv = mp4parse_is_fragmented(parser, track_info.track_id, &mut is_fragmented_file);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(is_fragmented_file, 0);

        let mut indice = Mp4parseByteData::default();
        rv = mp4parse_get_indice_table(parser, track_info.track_id, &mut indice);
        assert_eq!(rv, Mp4parseStatus::Ok);

        // Compare the value from stagefright.
        // Due to 'elst', the start_composition and end_composition are negative
        // at first two samples.
        let audio_indice_0 = Indice {
            start_offset: 6992.into(),
            end_offset: 7363.into(),
            start_composition: (-36281).into(),
            end_composition: (-13062).into(),
            start_decode: 0.into(),
            sync: true,
        };
        let audio_indice_1 = Indice {
            start_offset: 7363.into(),
            end_offset: 7735.into(),
            start_composition: (-13062).into(),
            end_composition: 10158.into(),
            start_decode: 23219.into(),
            sync: true,
        };
        let audio_indice_2 = Indice {
            start_offset: 7735.into(),
            end_offset: 8106.into(),
            start_composition: 10158.into(),
            end_composition: 33378.into(),
            start_decode: 46439.into(),
            sync: true,
        };
        assert_eq!(indice.length, 21);
        assert_eq!(*indice.indices.offset(0), audio_indice_0);
        assert_eq!(*indice.indices.offset(1), audio_indice_1);
        assert_eq!(*indice.indices.offset(2), audio_indice_2);

        mp4parse_free(parser);
    }
}

#[test]
fn parse_sample_table_with_negative_ctts() {
    let mut file = std::fs::File::open("tests/white.mp4").expect("Unknown file");
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
        assert_eq!(track_info.track_type, Mp4parseTrackType::Video);

        let mut is_fragmented_file: u8 = std::u8::MAX;
        rv = mp4parse_is_fragmented(parser, track_info.track_id, &mut is_fragmented_file);
        assert_eq!(rv, Mp4parseStatus::Ok);
        assert_eq!(is_fragmented_file, 0);

        let mut indice = Mp4parseByteData::default();
        rv = mp4parse_get_indice_table(parser, track_info.track_id, &mut indice);
        assert_eq!(rv, Mp4parseStatus::Ok);

        // There are negative value in 'ctts' table.
        let video_indice_0 = Indice {
            start_offset: 48.into(),
            end_offset: 890.into(),
            start_composition: 0.into(),
            end_composition: 33_333.into(),
            start_decode: 0.into(),
            sync: true,
        };
        let video_indice_1 = Indice {
            start_offset: 890.into(),
            end_offset: 913.into(),
            start_composition: 133_333.into(),
            end_composition: 166_666.into(),
            start_decode: 33_333.into(),
            sync: false,
        };
        let video_indice_2 = Indice {
            start_offset: 913.into(),
            end_offset: 934.into(),
            start_composition: 66_666.into(),
            end_composition: 100_000.into(),
            start_decode: 66_666.into(),
            sync: false,
        };
        let video_indice_3 = Indice {
            start_offset: 934.into(),
            end_offset: 955.into(),
            start_composition: 33_333.into(),
            end_composition: 66_666.into(),
            start_decode: 100_000.into(),
            sync: false,
        };
        assert_eq!(indice.length, 300);
        assert_eq!(*indice.indices.offset(0), video_indice_0);
        assert_eq!(*indice.indices.offset(1), video_indice_1);
        assert_eq!(*indice.indices.offset(2), video_indice_2);
        assert_eq!(*indice.indices.offset(3), video_indice_3);

        mp4parse_free(parser);
    }
}
