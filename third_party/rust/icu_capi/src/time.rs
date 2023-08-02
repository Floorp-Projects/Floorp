// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;

    use icu_calendar::types::Time;

    use crate::errors::ffi::ICU4XError;

    #[diplomat::opaque]
    /// An ICU4X Time object representing a time in terms of hour, minute, second, nanosecond
    #[diplomat::rust_link(icu::calendar::types::Time, Struct)]
    pub struct ICU4XTime(pub Time);

    impl ICU4XTime {
        /// Creates a new [`ICU4XTime`] given field values
        #[diplomat::rust_link(icu::calendar::types::Time, Struct)]
        pub fn create(
            hour: u8,
            minute: u8,
            second: u8,
            nanosecond: u32,
        ) -> Result<Box<ICU4XTime>, ICU4XError> {
            let hour = hour.try_into()?;
            let minute = minute.try_into()?;
            let second = second.try_into()?;
            let nanosecond = nanosecond.try_into()?;
            let time = Time {
                hour,
                minute,
                second,
                nanosecond,
            };
            Ok(Box::new(ICU4XTime(time)))
        }

        /// Returns the hour in this time
        #[diplomat::rust_link(icu::calendar::types::Time::hour, StructField)]
        pub fn hour(&self) -> u8 {
            self.0.hour.into()
        }
        /// Returns the minute in this time
        #[diplomat::rust_link(icu::calendar::types::Time::minute, StructField)]
        pub fn minute(&self) -> u8 {
            self.0.minute.into()
        }
        /// Returns the second in this time
        #[diplomat::rust_link(icu::calendar::types::Time::second, StructField)]
        pub fn second(&self) -> u8 {
            self.0.second.into()
        }
        /// Returns the nanosecond in this time
        #[diplomat::rust_link(icu::calendar::types::Time::nanosecond, StructField)]
        pub fn nanosecond(&self) -> u32 {
            self.0.nanosecond.into()
        }
    }
}
