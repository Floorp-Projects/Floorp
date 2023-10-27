// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Functions that handle capturing QLOG traces.

use std::convert::TryFrom;

use qlog::{
    self,
    events::{DataRecipient, EventData},
};

use neqo_common::qlog::NeqoQlog;
use neqo_transport::StreamId;

pub fn h3_data_moved_up(qlog: &mut NeqoQlog, stream_id: StreamId, amount: usize) {
    qlog.add_event_data(|| {
        let ev_data = EventData::DataMoved(qlog::events::quic::DataMoved {
            stream_id: Some(stream_id.as_u64()),
            offset: None,
            length: Some(u64::try_from(amount).unwrap()),
            from: Some(DataRecipient::Transport),
            to: Some(DataRecipient::Application),
            raw: None,
        });

        Some(ev_data)
    });
}

pub fn h3_data_moved_down(qlog: &mut NeqoQlog, stream_id: StreamId, amount: usize) {
    qlog.add_event_data(|| {
        let ev_data = EventData::DataMoved(qlog::events::quic::DataMoved {
            stream_id: Some(stream_id.as_u64()),
            offset: None,
            length: Some(u64::try_from(amount).unwrap()),
            from: Some(DataRecipient::Application),
            to: Some(DataRecipient::Transport),
            raw: None,
        });

        Some(ev_data)
    });
}
