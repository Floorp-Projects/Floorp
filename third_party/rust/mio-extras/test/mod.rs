extern crate mio;
extern crate mio_extras;

use mio::event::Event;
use mio::{Events, Poll};
use std::time::Duration;

mod test_poll_channel;
mod test_timer;

pub fn expect_events(
    poll: &Poll,
    event_buffer: &mut Events,
    poll_try_count: usize,
    mut expected: Vec<Event>,
) {
    const MS: u64 = 1_000;

    for _ in 0..poll_try_count {
        poll.poll(event_buffer, Some(Duration::from_millis(MS)))
            .unwrap();
        for event in event_buffer.iter() {
            let pos_opt = match expected.iter().position(|exp_event| {
                (event.token() == exp_event.token())
                    && event.readiness().contains(exp_event.readiness())
            }) {
                Some(x) => Some(x),
                None => None,
            };
            if let Some(pos) = pos_opt {
                expected.remove(pos);
            }
        }

        if expected.is_empty() {
            break;
        }
    }

    assert!(
        expected.is_empty(),
        "The following expected events were not found: {:?}",
        expected
    );
}
