extern crate mp4parse;
extern crate mp4parse_capi;

#[macro_use]
extern crate log;

extern crate env_logger;

use mp4parse_capi::*;
use std::env;
use std::fs::File;
use std::io::Read;

extern "C" fn buf_read(buf: *mut u8, size: usize, userdata: *mut std::os::raw::c_void) -> isize {
    let input: &mut std::fs::File = unsafe { &mut *(userdata as *mut _) };
    let mut buf = unsafe { std::slice::from_raw_parts_mut(buf, size) };
    match input.read(&mut buf) {
        Ok(n) => n as isize,
        Err(_) => -1,
    }
}

fn dump_file(filename: &str) {
    let mut file = File::open(filename).expect("Unknown file");
    let io = Mp4parseIo {
        read: Some(buf_read),
        userdata: &mut file as *mut _ as *mut std::os::raw::c_void,
    };

    unsafe {
        let mut parser = std::ptr::null_mut();
        let rv = mp4parse_new(&io, &mut parser);

        match rv {
            Mp4parseStatus::Ok => (),
            _ => {
                println!("-- fail to parse, '-v' for more info");
                return;
            }
        }

        let mut frag_info = Mp4parseFragmentInfo::default();
        match mp4parse_get_fragment_info(parser, &mut frag_info) {
            Mp4parseStatus::Ok => {
                println!("-- mp4parse_fragment_info {:?}", frag_info);
            }
            _ => {
                println!("-- mp4parse_fragment_info failed");
                return;
            }
        }

        let mut counts: u32 = 0;
        match mp4parse_get_track_count(parser, &mut counts) {
            Mp4parseStatus::Ok => (),
            _ => {
                println!("-- mp4parse_get_track_count failed");
                return;
            }
        }

        for i in 0..counts {
            let mut track_info = Mp4parseTrackInfo {
                track_type: Mp4parseTrackType::Audio,
                track_id: 0,
                duration: 0,
                media_time: 0,
            };
            match mp4parse_get_track_info(parser, i, &mut track_info) {
                Mp4parseStatus::Ok => {
                    println!("-- mp4parse_get_track_info {:?}", track_info);
                }
                _ => {
                    println!("-- mp4parse_get_track_info failed, track id: {}", i);
                    return;
                }
            }

            match track_info.track_type {
                Mp4parseTrackType::Audio => {
                    let mut audio_info = Mp4parseTrackAudioInfo::default();
                    match mp4parse_get_track_audio_info(parser, i, &mut audio_info) {
                        Mp4parseStatus::Ok => {
                            println!("-- mp4parse_get_track_audio_info {:?}", audio_info);
                            for i in 0..audio_info.sample_info_count as isize {
                                let sample_info = audio_info.sample_info.offset(i);
                                println!(
                                    "  -- mp4parse_get_track_audio_info sample_info[{:?}] {:?}",
                                    i, *sample_info
                                );
                            }
                        }
                        _ => {
                            println!("-- mp4parse_get_track_audio_info failed, track id: {}", i);
                            return;
                        }
                    }
                }
                Mp4parseTrackType::Video => {
                    let mut video_info = Mp4parseTrackVideoInfo::default();
                    match mp4parse_get_track_video_info(parser, i, &mut video_info) {
                        Mp4parseStatus::Ok => {
                            println!("-- mp4parse_get_track_video_info {:?}", video_info);
                            for i in 0..video_info.sample_info_count as isize {
                                let sample_info = video_info.sample_info.offset(i);
                                println!(
                                    "  -- mp4parse_get_track_video_info sample_info[{:?}] {:?}",
                                    i, *sample_info
                                );
                            }
                        }
                        _ => {
                            println!("-- mp4parse_get_track_video_info failed, track id: {}", i);
                            return;
                        }
                    }
                }
                Mp4parseTrackType::Metadata => {
                    println!("TODO metadata track");
                }
            }

            let mut indices = Mp4parseByteData::default();
            match mp4parse_get_indice_table(parser, track_info.track_id, &mut indices) {
                Mp4parseStatus::Ok => {
                    println!(
                        "-- mp4parse_get_indice_table track_id {} indices {:?}",
                        track_info.track_id, indices
                    );
                }
                _ => {
                    println!(
                        "-- mp4parse_get_indice_table failed, track_info.track_id: {}",
                        track_info.track_id
                    );
                    return;
                }
            }
        }
        mp4parse_free(parser);
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        return;
    }

    // Initialize logging, setting the log level if requested.
    let (skip, verbose) = if args[1] == "-v" {
        (2, true)
    } else {
        (1, false)
    };
    let env = env_logger::Env::default();
    let mut logger = env_logger::Builder::from_env(env);
    if verbose {
        logger.filter(None, log::LevelFilter::Debug);
    }
    logger.init();

    for filename in args.iter().skip(skip) {
        info!("-- dump of '{}' --", filename);
        dump_file(filename);
        info!("-- end of '{}' --", filename);
    }
}
