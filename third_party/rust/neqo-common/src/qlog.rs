// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::fmt;
use std::path::{Path, PathBuf};
use std::time::SystemTime;

use chrono::{DateTime, Utc};
use qlog::{
    self, CommonFields, Configuration, QlogStreamer, TimeUnits, Trace, VantagePoint,
    VantagePointType,
};

use crate::Role;

#[allow(clippy::module_name_repetitions)]
pub struct NeqoQlog {
    qlog_path: PathBuf,
    streamer: QlogStreamer,
}

impl NeqoQlog {
    /// # Errors
    ///
    /// Will return `qlog::Error` if cannot write to the new log.
    pub fn new(
        mut streamer: QlogStreamer,
        qlog_path: impl AsRef<Path>,
    ) -> Result<Self, qlog::Error> {
        streamer.start_log()?;

        Ok(Self {
            streamer,
            qlog_path: qlog_path.as_ref().to_owned(),
        })
    }

    pub fn stream(&mut self) -> &mut QlogStreamer {
        &mut self.streamer
    }
}

impl fmt::Debug for NeqoQlog {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "NeqoQlog writing to {}", self.qlog_path.display())
    }
}

impl Drop for NeqoQlog {
    fn drop(&mut self) {
        if let Err(e) = self.streamer.finish_log() {
            crate::do_log!(::log::Level::Error, "Error dropping NeqoQlog: {}", e)
        }
    }
}

pub fn handle_qlog_result<T>(qlog: &mut Option<NeqoQlog>, result: Result<T, qlog::Error>) {
    if let Err(e) = result {
        crate::do_log!(
            ::log::Level::Error,
            "Qlog streaming failed with error {}; closing qlog.",
            e
        );
        *qlog = None;
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
