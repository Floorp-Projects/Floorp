// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::convert::TryFrom;

use qlog::{self, event::Event, H3DataRecipient};

use neqo_common::qlog::NeqoQlog;
use neqo_transport::StreamId;

pub fn h3_data_moved_up(qlog: &mut NeqoQlog, stream_id: StreamId, amount: usize) {
    qlog.add_event(|| {
        Some(Event::h3_data_moved(
            stream_id.to_string(),
            None,
            Some(u64::try_from(amount).unwrap()),
            Some(H3DataRecipient::Transport),
            Some(H3DataRecipient::Application),
            None,
        ))
    });
}

pub fn h3_data_moved_down(qlog: &mut NeqoQlog, stream_id: StreamId, amount: usize) {
    qlog.add_event(|| {
        Some(Event::h3_data_moved(
            stream_id.to_string(),
            None,
            Some(u64::try_from(amount).unwrap()),
            Some(H3DataRecipient::Application),
            Some(H3DataRecipient::Transport),
            None,
        ))
    });
}
