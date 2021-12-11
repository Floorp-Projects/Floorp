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
fn parse_out_of_chunk_range() {
    let mut file = std::fs::File::open("tests/chunk_out_of_range.mp4").expect("Unknown file");
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

        // its first chunk is out of range.
        // <SampleToChunkBox EntryCount="1">
        //     <BoxInfo Size="28" Type="stsc"/>
        //     <FullBoxInfo Version="0" Flags="0x0"/>
        //     <SampleToChunkEntry FirstChunk="16777217" SamplesPerChunk="17" SampleDescriptionIndex="1"/>
        //
        let mut indice = Mp4parseByteData::default();
        let rv = mp4parse_get_indice_table(parser, 1, &mut indice);
        assert_eq!(rv, Mp4parseStatus::Invalid);

        mp4parse_free(parser);
    }
}
