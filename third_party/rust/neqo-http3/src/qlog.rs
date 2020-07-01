// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::convert::TryFrom;

use qlog::{self, event::Event, H3DataRecipient};

use neqo_common::qlog::{handle_qlog_result, NeqoQlog};

pub fn h3_data_moved_up(maybe_qlog: &mut Option<NeqoQlog>, stream_id: u64, amount: usize) {
    if let Some(qlog) = maybe_qlog {
        let res = qlog.stream().add_event(Event::h3_data_moved(
            stream_id.to_string(),
            None,
            Some(u64::try_from(amount).unwrap()),
            Some(H3DataRecipient::Transport),
            Some(H3DataRecipient::Application),
            None,
        ));
        handle_qlog_result(maybe_qlog, res)
    }
}

pub fn h3_data_moved_down(maybe_qlog: &mut Option<NeqoQlog>, stream_id: u64, amount: usize) {
    if let Some(qlog) = maybe_qlog {
        let res = qlog.stream().add_event(Event::h3_data_moved(
            stream_id.to_string(),
            None,
            Some(u64::try_from(amount).unwrap()),
            Some(H3DataRecipient::Application),
            Some(H3DataRecipient::Transport),
            None,
        ));
        handle_qlog_result(maybe_qlog, res)
    }
}
