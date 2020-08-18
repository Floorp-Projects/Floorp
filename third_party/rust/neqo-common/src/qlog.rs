// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::cell::RefCell;
use std::fmt;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::time::SystemTime;

use chrono::{DateTime, Utc};
use qlog::{
    self, CommonFields, Configuration, QlogStreamer, TimeUnits, Trace, VantagePoint,
    VantagePointType,
};

use crate::Role;

#[allow(clippy::module_name_repetitions)]
#[derive(Debug, Clone)]
pub struct NeqoQlog {
    inner: Rc<RefCell<Option<NeqoQlogShared>>>,
}

pub struct NeqoQlogShared {
    qlog_path: PathBuf,
    streamer: QlogStreamer,
}

impl NeqoQlog {
    /// Create an enabled `NeqoQlog` configuration.
    /// # Errors
    ///
    /// Will return `qlog::Error` if cannot write to the new log.
    pub fn enabled(
        mut streamer: QlogStreamer,
        qlog_path: impl AsRef<Path>,
    ) -> Result<Self, qlog::Error> {
        streamer.start_log()?;

        Ok(Self {
            inner: Rc::new(RefCell::new(Some(NeqoQlogShared {
                streamer,
                qlog_path: qlog_path.as_ref().to_owned(),
            }))),
        })
    }

    /// Create a disabled `NeqoQlog` configuration.
    #[must_use]
    pub fn disabled() -> Self {
        Self {
            inner: Rc::new(RefCell::new(None)),
        }
    }

    /// If logging enabled, closure may generate an event to be logged.
    pub fn add_event<F>(&mut self, f: F)
    where
        F: FnOnce() -> Option<qlog::event::Event>,
    {
        self.add_event_with_stream(|s| {
            if let Some(evt) = f() {
                s.add_event(evt)?;
            }
            Ok(())
        });
    }

    /// If logging enabled, closure is given the Qlog stream to write events and
    /// frames to.
    pub fn add_event_with_stream<F>(&mut self, f: F)
    where
        F: FnOnce(&mut QlogStreamer) -> Result<(), qlog::Error>,
    {
        if let Some(inner) = self.inner.borrow_mut().as_mut() {
            if let Err(e) = f(&mut inner.streamer) {
                crate::do_log!(
                    ::log::Level::Error,
                    "Qlog event generation failed with error {}; closing qlog.",
                    e
                );
                *self.inner.borrow_mut() = None;
            }
        }
    }
}

impl Default for NeqoQlog {
    fn default() -> Self {
        Self::disabled()
    }
}

impl fmt::Debug for NeqoQlogShared {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "NeqoQlog writing to {}", self.qlog_path.display())
    }
}

impl Drop for NeqoQlogShared {
    fn drop(&mut self) {
        if let Err(e) = self.streamer.finish_log() {
            crate::do_log!(::log::Level::Error, "Error dropping NeqoQlog: {}", e)
        }
    }
}

#[must_use]
pub fn new_trace(role: Role) -> qlog::Trace {
    Trace {
        vantage_point: VantagePoint {
            name: Some(format!("neqo-{}", role)),
            ty: match role {
                Role::Client => VantagePointType::Client,
                Role::Server => VantagePointType::Server,
            },
            flow: None,
        },
        title: Some(format!("neqo-{} trace", role)),
        description: Some("Example qlog trace description".to_string()),
        configuration: Some(Configuration {
            time_offset: Some("0".into()),
            time_units: Some(TimeUnits::Us),
            original_uris: None,
        }),
        common_fields: Some(CommonFields {
            group_id: None,
            protocol_type: None,
            reference_time: Some({
                let system_time = SystemTime::now();
                let datetime: DateTime<Utc> = system_time.into();
                datetime.to_rfc3339()
            }),
        }),
        event_fields: vec![
            "relative_time".to_string(),
            "category".to_string(),
            "event".to_string(),
            "data".to_string(),
        ],
        events: Vec::new(),
    }
}
