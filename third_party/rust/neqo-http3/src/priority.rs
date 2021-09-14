use crate::{Error, HFrame, Header, Res};
use sfv::{BareItem, Item, ListEntry, Parser};
use std::fmt;

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct Priority {
    urgency: u8,
    incremental: bool,
}

impl Default for Priority {
    fn default() -> Self {
        Priority {
            urgency: 3,
            incremental: false,
        }
    }
}

impl Priority {
    /// # Panics
    /// If an invalid urgency (>7 is given)
    pub fn new(urgency: u8, incremental: bool) -> Priority {
        assert!(urgency < 8);
        Priority {
            urgency,
            incremental,
        }
    }

    /// Returns a header if required to send
    pub fn header(self) -> Option<Header> {
        match self {
            Priority {
                urgency: 3,
                incremental: false,
            } => None,
            other => Some(Header::new("priority", format!("{}", other))),
        }
    }

    pub fn from_bytes(bytes: &[u8]) -> Res<Priority> {
        let dict = Parser::parse_dictionary(bytes).map_err(|_| Error::HttpFrame)?;
        let urgency = match dict.get("u") {
            Some(ListEntry::Item(Item {
                bare_item: BareItem::Integer(u),
                ..
            })) if (0i64..=7).contains(u) => *u as u8,
            _ => 3,
        };
        let incremental = match dict.get("i") {
            Some(ListEntry::Item(Item {
                bare_item: BareItem::Boolean(i),
                ..
            })) => *i,
            _ => false,
        };
        Ok(Priority {
            urgency,
            incremental,
        })
    }
}

impl fmt::Display for Priority {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Priority {
                urgency: 3,
                incremental: false,
            } => Ok(()),
            Priority {
                urgency: 3,
                incremental: true,
            } => write!(f, "i"),
            Priority {
                urgency,
                incremental: false,
            } => write!(f, "u={}", urgency),
            Priority {
                urgency,
                incremental: true,
            } => write!(f, "u={},i", urgency),
        }
    }
}

#[derive(Debug)]
pub struct PriorityHandler {
    push_stream: bool,
    priority: Priority,
    last_send_priority: Priority,
}

impl PriorityHandler {
    pub fn new(push_stream: bool, priority: Priority) -> PriorityHandler {
        PriorityHandler {
            push_stream,
            priority,
            last_send_priority: priority,
        }
    }

    pub fn priority(&self) -> Priority {
        self.priority
    }

    /// Returns if an priority update will be issued
    pub fn maybe_update_priority(&mut self, priority: Priority) -> bool {
        if priority != self.priority {
            self.priority = priority;
            true
        } else {
            false
        }
    }

    pub fn priority_update_sent(&mut self) {
        self.last_send_priority = self.priority
    }

    /// Returns HFrame if an priority update is outstanding
    pub fn maybe_encode_frame(&self, stream_id: u64) -> Option<HFrame> {
        if self.priority != self.last_send_priority {
            if self.push_stream {
                Some(HFrame::PriorityUpdatePush {
                    element_id: stream_id,
                    priority: self.priority,
                })
            } else {
                Some(HFrame::PriorityUpdateRequest {
                    element_id: stream_id,
                    priority: self.priority,
                })
            }
        } else {
            None
        }
    }
}

#[cfg(test)]
mod test {
    use crate::priority::PriorityHandler;
    use crate::{HFrame, Priority};

    #[test]
    fn priority_updates_ignore_same() {
        let mut p = PriorityHandler::new(false, Priority::new(5, false));
        assert!(!p.maybe_update_priority(Priority::new(5, false)));
        // updating with the same priority -> there should not be any priority frame sent
        assert!(p.maybe_encode_frame(4).is_none());
    }

    #[test]
    fn priority_updates_send_update() {
        let mut p = PriorityHandler::new(false, Priority::new(5, false));
        assert!(p.maybe_update_priority(Priority::new(6, false)));
        // updating with the a different priority -> there should be a priority frame sent
        assert!(p.maybe_encode_frame(4).is_some());
    }

    #[test]
    fn multiple_priority_updates_ignore_same() {
        let mut p = PriorityHandler::new(false, Priority::new(5, false));
        assert!(p.maybe_update_priority(Priority::new(6, false)));
        assert!(p.maybe_update_priority(Priority::new(5, false)));
        // initial and last priority same -> there should not be any priority frame sent
        assert!(p.maybe_encode_frame(4).is_none());
    }

    #[test]
    fn multiple_priority_updates_send_update() {
        let mut p = PriorityHandler::new(false, Priority::new(5, false));
        assert!(p.maybe_update_priority(Priority::new(6, false)));
        assert!(p.maybe_update_priority(Priority::new(7, false)));
        // updating two times with a different priority -> the last priority update should be in the next frame
        let expected = HFrame::PriorityUpdateRequest {
            element_id: 4,
            priority: Priority::new(7, false),
        };
        assert_eq!(p.maybe_encode_frame(4), Some(expected));
    }

    #[test]
    fn priority_updates_incremental() {
        let mut p = PriorityHandler::new(false, Priority::new(5, false));
        assert!(p.maybe_update_priority(Priority::new(5, true)));
        // updating the incremental parameter -> there should be a priority frame sent
        let expected = HFrame::PriorityUpdateRequest {
            element_id: 4,
            priority: Priority::new(5, true),
        };
        assert_eq!(p.maybe_encode_frame(4), Some(expected));
    }
}
