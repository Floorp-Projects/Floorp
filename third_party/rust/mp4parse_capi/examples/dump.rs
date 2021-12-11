extern crate mp4parse;
extern crate mp4parse_capi;

#[macro_use]
extern crate log;

extern crate env_logger;

use mp4parse::ParseStrictness;
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

fn dump_avif(filename: &str, strictness: ParseStrictness) {
    let mut file = File::open(filename).expect("Unknown file");
    let io = Mp4parseIo {
        read: Some(buf_read),
        userdata: &mut file as *mut _ as *mut std::os::raw::c_void,
    };

    unsafe {
        let mut parser = std::ptr::null_mut();
        let rv = mp4parse_avif_new(&io, strictness, &mut parser);
        println!("mp4parse_avif_new -> {:?}", rv);
        if rv == Mp4parseStatus::Ok {
            println!(
                "mp4parse_avif_get_image_safe -> {:?}",
                mp4parse_avif_get_image_safe(&*parser)
            );
        }
    }
}

fn dump_file(filename: &str, strictness: ParseStrictness) {
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
            Mp4parseStatus::Invalid => {
                println!("-- failed to parse as mp4 video, trying AVIF");
                dump_avif(filename, strictness);
            }
            _ => {
                println!("-- fail to parse: {:?}, '-v' for more info", rv);
                return;
            }
        }

        let mut frag_info = Mp4parseFragmentInfo::default();
        match mp4parse_get_fragment_info(parser, &mut frag_info) {
            Mp4parseStatus::Ok => {
                println!("-- mp4parse_fragment_info {:?}", frag_info);
            }
            rv => {
                println!("-- mp4parse_fragment_info failed with {:?}", rv);
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
                ..Default::default()
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
    let mut dump_func: fn(&str, ParseStrictness) = dump_file;
    let mut verbose = false;
    let mut strictness = ParseStrictness::Normal;
    let mut filenames = Vec::new();
    for arg in env::args().skip(1) {
        match arg.as_str() {
            "--avif" => dump_func = dump_avif,
            "--strict" => strictness = ParseStrictness::Strict,
            "--permissive" => strictness = ParseStrictness::Permissive,
            "-v" | "--verbose" => verbose = true,
            _ => {
                if let Some("-") = arg.get(0..1) {
                    eprintln!("Ignoring unknown switch {:?}", arg);
                } else {
                    filenames.push(arg)
                }
            }
        }
    }

    if filenames.is_empty() {
        eprintln!("No files to dump, exiting...");
        return;
    }

    let env = env_logger::Env::default();
    let mut logger = env_logger::Builder::from_env(env);
    if verbose {
        logger.filter(None, log::LevelFilter::Debug);
    }
    logger.init();

    for filename in filenames {
        info!("-- dump of '{}' --", filename);
        dump_func(filename.as_str(), strictness);
        info!("-- end of '{}' --", filename);
    }
}
