// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! API definitions for the different metric types supported by the Glean SDK.
//!
//! Individual metric types implement this trait to expose the specific metrics API.
//! It can be used by wrapping implementations to guarantee API conformance.

mod boolean;
mod counter;
mod custom_distribution;
mod datetime;
mod event;
mod labeled;
mod memory_distribution;
mod numerator;
mod ping;
mod quantity;
mod rate;
mod string;
mod string_list;
mod text;
mod timespan;
mod timing_distribution;
mod url;
mod uuid;

pub use self::boolean::Boolean;
pub use self::counter::Counter;
pub use self::custom_distribution::CustomDistribution;
pub use self::datetime::Datetime;
pub use self::event::Event;
pub use self::event::EventRecordingError;
pub use self::event::ExtraKeys;
pub use self::event::NoExtraKeys;
pub use self::labeled::Labeled;
pub use self::memory_distribution::MemoryDistribution;
pub use self::numerator::Numerator;
pub use self::ping::Ping;
pub use self::quantity::Quantity;
pub use self::rate::Rate;
pub use self::string::String;
pub use self::string_list::StringList;
pub use self::text::Text;
pub use self::timespan::Timespan;
pub use self::timing_distribution::TimingDistribution;
pub use self::url::Url;
pub use self::uuid::Uuid;
pub use crate::histogram::HistogramType;
