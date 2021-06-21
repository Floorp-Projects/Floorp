// Copyright 2018 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#ifndef shift_or_euc_h
#define shift_or_euc_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "encoding_rs.h"

#ifndef SHIFT_OR_EUC_DETECTOR
#define SHIFT_OR_EUC_DETECTOR Detector
#ifndef __cplusplus
typedef struct Detector_ Detector;
#endif
#endif

/// Instantiates the detector. If `allow_2022` is `true` the possible
/// guesses are Shift_JIS, EUC-JP, ISO-2022-JP, and undecided. If
/// `allow_2022` is `false`, the possible guesses are Shift_JIS, EUC-JP,
/// and undecided.
///
/// The instantiated detector must be freed after use using
/// `shift_or_euc_detector_free`.
SHIFT_OR_EUC_DETECTOR* shift_or_euc_detector_new(bool allow_2022);

/// Deallocates a detector obtained from `shift_or_euc_detector_new`.
void shift_or_euc_detector_free(SHIFT_OR_EUC_DETECTOR* detector);

/// Feeds bytes to the detector. If `last` is `true` the end of the stream
/// is considered to occur immediately after the end of `buffer`.
/// Otherwise, the stream is expected to continue. `buffer_len` may be zero.
/// `buffer` must not be `NULL` but may be undereferencable when
/// `buffer_len` is zero.
///
/// If you're running the detector only on a prefix of a complete
/// document, _do not_ pass `last` as `true` after the prefix if the
/// stream as a whole still contains more content.
///
/// Returns `SHIFT_JIS_ENCODING` if the detector guessed
/// Shift_JIS. Returns `EUC_JP_ENCODING` if the detector
/// guessed EUC-JP. Returns `ISO_2022_JP_ENCODING` if the
/// detector guessed ISO-2022-JP (only possible if `true` was passed as
/// `allow_2022` when instantiating the detector). Returns `NULL` if the
/// detector is undecided. If `NULL` is returned even when passing `true`
/// as `last`, falling back to Shift_JIS is the best guess for Web
/// purposes.
///
/// Do not call again after the function has returned non-`NULL` or after
/// the function has been called with `true` as `last`.
///
/// # Panics
///
/// If called after the function has returned non-`NULL` or after the
/// function has been called with `true` as `last`.
///
/// # Undefined Behavior
///
/// UB ensues if
///
/// * `detector` does not point to a detector obtained from
///   `shift_or_euc_detector_new` but not yet freed with
///   `shift_or_euc_detector_free`.
/// * `buffer` is `NULL`.
/// * `buffer` and `buffer_len` don't designate a range of memory
///   valid for reading.
ENCODING_RS_ENCODING const* shift_or_euc_detector_feed(
    SHIFT_OR_EUC_DETECTOR* detector,
    uint8_t const* buffer,
    size_t buffer_len,
    bool last
);

#ifdef __cplusplus
}
#endif

#endif // shift_or_euc_h