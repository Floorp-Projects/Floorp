use mp4parse_capi::*;
use num_traits::ToPrimitive;
use std::io::Read;

extern "C" fn buf_read(buf: *mut u8, size: usize, userdata: *mut std::os::raw::c_void) -> isize {
    let input: &mut std::fs::File = unsafe { &mut *(userdata as *mut _) };
    let buf = unsafe { std::slice::from_raw_parts_mut(buf, size) };
    match input.read(buf) {
        Ok(n) => n as isize,
        Err(_) => -1,
    }
}

unsafe fn parse_file_and_get_info(path: &str) -> (*mut Mp4parseAvifParser, Mp4parseAvifInfo) {
    let mut file = std::fs::File::open(path).expect("Unknown file");
    let io = Mp4parseIo {
        read: Some(buf_read),
        userdata: &mut file as *mut _ as *mut std::os::raw::c_void,
    };

    let mut parser = std::ptr::null_mut();
    let mut rv = mp4parse_avif_new(&io, ParseStrictness::Normal, &mut parser);
    assert_eq!(rv, Mp4parseStatus::Ok);
    assert!(!parser.is_null());

    let mut info = Mp4parseAvifInfo {
        premultiplied_alpha: Default::default(),
        major_brand: Default::default(),
        unsupported_features_bitfield: Default::default(),
        spatial_extents: std::ptr::null(),
        nclx_colour_information: std::ptr::null(),
        icc_colour_information: Default::default(),
        image_rotation: mp4parse::ImageRotation::D0,
        image_mirror: std::ptr::null(),
        pixel_aspect_ratio: std::ptr::null(),
        has_primary_item: Default::default(),
        primary_item_bit_depth: Default::default(),
        has_alpha_item: Default::default(),
        alpha_item_bit_depth: Default::default(),
        has_sequence: Default::default(),
        loop_mode: Default::default(),
        loop_count: Default::default(),
        color_track_id: Default::default(),
        color_track_bit_depth: Default::default(),
        alpha_track_id: Default::default(),
        alpha_track_bit_depth: Default::default(),
    };
    rv = mp4parse_avif_get_info(parser, &mut info);
    assert_eq!(rv, Mp4parseStatus::Ok);
    (parser, info)
}

fn check_loop_count(path: &str, expected_loop_count: i64) {
    let (parser, info) = unsafe { parse_file_and_get_info(path) };
    match info.loop_mode {
        Mp4parseAvifLoopMode::NoEdits => assert_eq!(expected_loop_count, -1),
        Mp4parseAvifLoopMode::LoopByCount => {
            assert_eq!(info.loop_count.to_i64(), Some(expected_loop_count))
        }
        Mp4parseAvifLoopMode::LoopInfinitely => assert_eq!(expected_loop_count, std::i64::MIN),
    }

    unsafe { mp4parse_avif_free(parser) };
}

fn check_timescale(path: &str, expected_timescale: u64) {
    let (parser, info) = unsafe { parse_file_and_get_info(path) };

    let mut indices: Mp4parseByteData = Mp4parseByteData::default();
    let mut timescale: u64 = 0;
    let rv = unsafe {
        mp4parse_avif_get_indice_table(parser, info.color_track_id, &mut indices, &mut timescale)
    };

    assert_eq!(rv, Mp4parseStatus::Ok);
    assert_eq!(timescale, expected_timescale);

    unsafe { mp4parse_avif_free(parser) };
}

#[test]
fn loop_once() {
    check_loop_count("tests/loop_1.avif", 1);
}

#[test]
fn loop_twice() {
    check_loop_count("tests/loop_2.avif", 2);
}

#[test]
fn loop_four_times_due_to_ceiling() {
    check_loop_count("tests/loop_ceiled_4.avif", 4);
}

#[test]
fn loop_forever() {
    check_loop_count("tests/loop_forever.avif", std::i64::MIN);
}

#[test]
fn no_edts() {
    check_loop_count("tests/no_edts.avif", -1);
}

#[test]
fn check_timescales() {
    check_timescale("tests/loop_1.avif", 2);
    check_timescale("tests/loop_2.avif", 2);
    check_timescale("tests/loop_ceiled_4.avif", 2);
    check_timescale("tests/loop_forever.avif", 2);
    check_timescale("tests/no_edts.avif", 16384);
}
