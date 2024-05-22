// Copyright © 2017-2023 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::os::raw::{c_char, c_int, c_void};
use stream::cubeb_stream_params;

pub enum cubeb_audio_dump_stream {}
pub enum cubeb_audio_dump_session {}
pub type cubeb_audio_dump_stream_t = *mut cubeb_audio_dump_stream;
pub type cubeb_audio_dump_session_t = *mut cubeb_audio_dump_session;

extern "C" {
    pub fn cubeb_audio_dump_init(session: *mut cubeb_audio_dump_session_t) -> c_int;
    pub fn cubeb_audio_dump_shutdown(session: cubeb_audio_dump_session_t) -> c_int;
    pub fn cubeb_audio_dump_stream_init(
        session: cubeb_audio_dump_session_t,
        stream: *mut cubeb_audio_dump_stream_t,
        stream_params: cubeb_stream_params,
        name: *const c_char,
    ) -> c_int;
    pub fn cubeb_audio_dump_stream_shutdown(
        session: cubeb_audio_dump_session_t,
        stream: cubeb_audio_dump_stream_t,
    ) -> c_int;
    pub fn cubeb_audio_dump_start(session: cubeb_audio_dump_session_t) -> c_int;
    pub fn cubeb_audio_dump_stop(session: cubeb_audio_dump_session_t) -> c_int;
    pub fn cubeb_audio_dump_write(
        stream: cubeb_audio_dump_stream_t,
        audio_samples: *mut c_void,
        count: u32,
    ) -> c_int;

}
