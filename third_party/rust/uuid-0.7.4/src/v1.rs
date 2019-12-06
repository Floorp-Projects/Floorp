//! The implementation for Version 1 [`Uuid`]s.
//!
//! Note that you need feature `v1` in order to use these features.
//!
//! [`Uuid`]: ../struct.Uuid.html

use core::sync::atomic;
use prelude::*;

/// A thread-safe, stateful context for the v1 generator to help ensure
/// process-wide uniqueness.
#[derive(Debug)]
pub struct Context {
    count: atomic::AtomicUsize,
}

/// A trait that abstracts over generation of Uuid v1 "Clock Sequence" values.
pub trait ClockSequence {
    /// Return a 16-bit number that will be used as the "clock sequence" in
    /// the Uuid. The number must be different if the time has changed since
    /// the last time a clock sequence was requested.
    fn generate_sequence(&self, seconds: u64, nano_seconds: u32) -> u16;
}

impl Uuid {
    /// Create a new [`Uuid`] (version 1) using a time value + sequence +
    /// *NodeId*.
    ///
    /// This expects two values representing a monotonically increasing value
    /// as well as a unique 6 byte NodeId, and an implementation of
    /// [`ClockSequence`]. This function is only guaranteed to produce
    /// unique values if the following conditions hold:
    ///
    /// 1. The *NodeId* is unique for this process,
    /// 2. The *Context* is shared across all threads which are generating v1
    ///    [`Uuid`]s,
    /// 3. The [`ClockSequence`] implementation reliably returns unique
    ///    clock sequences (this crate provides [`Context`] for this
    ///    purpose. However you can create your own [`ClockSequence`]
    ///    implementation, if [`Context`] does not meet your needs).
    ///
    /// The NodeID must be exactly 6 bytes long. If the NodeID is not a valid
    /// length this will return a [`ParseError`]`::InvalidLength`.
    ///
    /// The function is not guaranteed to produce monotonically increasing
    /// values however.  There is a slight possibility that two successive
    /// equal time values could be supplied and the sequence counter wraps back
    /// over to 0.
    ///
    /// If uniqueness and monotonicity is required, the user is responsible for
    /// ensuring that the time value always increases between calls (including
    /// between restarts of the process and device).
    ///
    /// Note that usage of this method requires the `v1` feature of this crate
    /// to be enabled.
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```rust
    /// use uuid::v1::Context;
    /// use uuid::Uuid;
    ///
    /// let context = Context::new(42);
    /// if let Ok(uuid) =
    ///     Uuid::new_v1(&context, 1497624119, 1234, &[1, 2, 3, 4, 5, 6])
    /// {
    ///     assert_eq!(
    ///         uuid.to_hyphenated().to_string(),
    ///         "f3b4958c-52a1-11e7-802a-010203040506"
    ///     )
    /// } else {
    ///     panic!()
    /// }
    /// ```
    ///
    /// [`ParseError`]: ../enum.ParseError.html
    /// [`Uuid`]: ../struct.Uuid.html
    /// [`ClockSequence`]: struct.ClockSequence.html
    /// [`Context`]: struct.Context.html
    pub fn new_v1<T>(
        context: &T,
        seconds: u64,
        nano_seconds: u32,
        node_id: &[u8],
    ) -> Result<Self, ::BytesError>
    where
        T: ClockSequence,
    {
        const NODE_ID_LEN: usize = 6;

        let len = node_id.len();
        if len != NODE_ID_LEN {
            return Err(::BytesError::new(NODE_ID_LEN, len));
        }

        let time_low;
        let time_mid;
        let time_high_and_version;

        {
            /// The number of 100 ns ticks between the UUID epoch
            /// `1582-10-15 00:00:00` and the Unix epoch `1970-01-01 00:00:00`.
            const UUID_TICKS_BETWEEN_EPOCHS: u64 = 0x01B2_1DD2_1381_4000;

            let timestamp =
                seconds * 10_000_000 + u64::from(nano_seconds / 100);
            let uuid_time = timestamp + UUID_TICKS_BETWEEN_EPOCHS;

            time_low = (uuid_time & 0xFFFF_FFFF) as u32;
            time_mid = ((uuid_time >> 32) & 0xFFFF) as u16;
            time_high_and_version =
                (((uuid_time >> 48) & 0x0FFF) as u16) | (1 << 12);
        }

        let mut d4 = [0; 8];

        {
            let count = context.generate_sequence(seconds, nano_seconds);
            d4[0] = (((count & 0x3F00) >> 8) as u8) | 0x80;
            d4[1] = (count & 0xFF) as u8;
        }

        d4[2..].copy_from_slice(node_id);

        Uuid::from_fields(time_low, time_mid, time_high_and_version, &d4)
    }
}

impl Context {
    /// Creates a thread-safe, internally mutable context to help ensure
    /// uniqueness.
    ///
    /// This is a context which can be shared across threads. It maintains an
    /// internal counter that is incremented at every request, the value ends
    /// up in the clock_seq portion of the [`Uuid`] (the fourth group). This
    /// will improve the probability that the [`Uuid`] is unique across the
    /// process.
    ///
    /// [`Uuid`]: ../struct.Uuid.html
    pub fn new(count: u16) -> Self {
        Self {
            count: atomic::AtomicUsize::new(count as usize),
        }
    }
}

impl ClockSequence for Context {
    fn generate_sequence(&self, _: u64, _: u32) -> u16 {
        (self.count.fetch_add(1, atomic::Ordering::SeqCst) & 0xffff) as u16
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_new_v1() {
        use super::Context;
        use prelude::*;

        let time: u64 = 1_496_854_535;
        let time_fraction: u32 = 812_946_000;
        let node = [1, 2, 3, 4, 5, 6];
        let context = Context::new(0);

        {
            let uuid =
                Uuid::new_v1(&context, time, time_fraction, &node).unwrap();

            assert_eq!(uuid.get_version(), Some(Version::Mac));
            assert_eq!(uuid.get_variant(), Some(Variant::RFC4122));
            assert_eq!(
                uuid.to_hyphenated().to_string(),
                "20616934-4ba2-11e7-8000-010203040506"
            );

            let ts = uuid.to_timestamp().unwrap();

            assert_eq!(ts.0 - 0x01B2_1DD2_1381_4000, 14_968_545_358_129_460);
            assert_eq!(ts.1, 0);
        };

        {
            let uuid2 =
                Uuid::new_v1(&context, time, time_fraction, &node).unwrap();

            assert_eq!(
                uuid2.to_hyphenated().to_string(),
                "20616934-4ba2-11e7-8001-010203040506"
            );
            assert_eq!(uuid2.to_timestamp().unwrap().1, 1)
        };
    }
}
