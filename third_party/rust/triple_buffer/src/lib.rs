//! Triple buffering in Rust
//!
//! In this crate, we propose a Rust implementation of triple buffering. This is
//! a non-blocking thread synchronization mechanism that can be used when a
//! single producer thread is frequently updating a shared data block, and a
//! single consumer thread wants to be able to read the latest available version
//! of the shared data whenever it feels like it.
//!
//! # Examples
//!
//! For many use cases, you can use the ergonomic write/read interface, where
//! the producer moves values into the buffer and the consumer accesses the
//! latest buffer by shared reference:
//!
//! ```
//! // Create a triple buffer
//! use triple_buffer::TripleBuffer;
//! let buf = TripleBuffer::new(0);
//!
//! // Split it into an input and output interface, to be respectively sent to
//! // the producer thread and the consumer thread
//! let (mut buf_input, mut buf_output) = buf.split();
//!
//! // The producer can move a value into the buffer at any time
//! buf_input.write(42);
//!
//! // The consumer can access the latest value from the producer at any time
//! let latest_value_ref = buf_output.read();
//! assert_eq!(*latest_value_ref, 42);
//! ```
//!
//! In situations where moving the original value away and being unable to
//! modify it on the consumer's side is too costly, such as if creating a new
//! value involves dynamic memory allocation, you can use a lower-level API
//! which allows you to access the producer and consumer's buffers in place
//! and to precisely control when updates are propagated:
//!
//! ```
//! // Create and split a triple buffer
//! use triple_buffer::TripleBuffer;
//! let buf = TripleBuffer::new(String::with_capacity(42));
//! let (mut buf_input, mut buf_output) = buf.split();
//!
//! // Mutate the input buffer in place
//! {
//!     // Acquire a reference to the input buffer
//!     let input = buf_input.input_buffer();
//!
//!     // In general, you don't know what's inside of the buffer, so you should
//!     // always reset the value before use (this is a type-specific process).
//!     input.clear();
//!
//!     // Perform an in-place update
//!     input.push_str("Hello, ");
//! }
//!
//! // Publish the above input buffer update
//! buf_input.publish();
//!
//! // Manually fetch the buffer update from the consumer interface
//! buf_output.update();
//!
//! // Acquire a mutable reference to the output buffer
//! let output = buf_output.output_buffer();
//!
//! // Post-process the output value before use
//! output.push_str("world!");
//! ```

#![deny(missing_debug_implementations, missing_docs)]

use cache_padded::CachePadded;

use std::{
    cell::UnsafeCell,
    sync::{
        atomic::{AtomicU8, Ordering},
        Arc,
    },
};

/// A triple buffer, useful for nonblocking and thread-safe data sharing
///
/// A triple buffer is a single-producer single-consumer nonblocking
/// communication channel which behaves like a shared variable: the producer
/// submits regular updates, and the consumer accesses the latest available
/// value whenever it feels like it.
///
#[derive(Debug)]
pub struct TripleBuffer<T: Send> {
    /// Input object used by producers to send updates
    input: Input<T>,

    /// Output object used by consumers to read the current value
    output: Output<T>,
}
//
impl<T: Clone + Send> TripleBuffer<T> {
    /// Construct a triple buffer with a certain initial value
    //
    // FIXME: After spending some time thinking about this further, I reached
    //        the conclusion that clippy was right after all. But since this is
    //        a breaking change, I'm keeping that for the next major release.
    //
    #[allow(clippy::needless_pass_by_value)]
    pub fn new(initial: T) -> Self {
        Self::new_impl(|| initial.clone())
    }
}
//
impl<T: Default + Send> Default for TripleBuffer<T> {
    /// Construct a triple buffer with a default-constructed value
    fn default() -> Self {
        Self::new_impl(T::default)
    }
}
//
impl<T: Send> TripleBuffer<T> {
    /// Construct a triple buffer, using a functor to generate initial values
    fn new_impl(mut generator: impl FnMut() -> T) -> Self {
        // Start with the shared state...
        let shared_state = Arc::new(SharedState::new(|_i| generator(), 0));

        // ...then construct the input and output structs
        TripleBuffer {
            input: Input {
                shared: shared_state.clone(),
                input_idx: 1,
            },
            output: Output {
                shared: shared_state,
                output_idx: 2,
            },
        }
    }

    /// Extract input and output of the triple buffer
    //
    // NOTE: Although it would be nicer to directly return `Input` and `Output`
    //       from `new()`, the `split()` design gives some API evolution
    //       headroom towards future allocation-free modes of operation where
    //       the SharedState is a static variable, or a stack-allocated variable
    //       used through scoped threads or other unsafe thread synchronization.
    //
    //       See https://github.com/HadrienG2/triple-buffer/issues/8 .
    //
    pub fn split(self) -> (Input<T>, Output<T>) {
        (self.input, self.output)
    }
}
//
// The Clone and PartialEq traits are used internally for testing and I don't
// want to commit to supporting them publicly for now.
//
#[doc(hidden)]
impl<T: Clone + Send> Clone for TripleBuffer<T> {
    fn clone(&self) -> Self {
        // Clone the shared state. This is safe because at this layer of the
        // interface, one needs an Input/Output &mut to mutate the shared state.
        let shared_state = Arc::new(unsafe { (*self.input.shared).clone() });

        // ...then the input and output structs
        TripleBuffer {
            input: Input {
                shared: shared_state.clone(),
                input_idx: self.input.input_idx,
            },
            output: Output {
                shared: shared_state,
                output_idx: self.output.output_idx,
            },
        }
    }
}
//
#[doc(hidden)]
impl<T: PartialEq + Send> PartialEq for TripleBuffer<T> {
    fn eq(&self, other: &Self) -> bool {
        // Compare the shared states. This is safe because at this layer of the
        // interface, one needs an Input/Output &mut to mutate the shared state.
        let shared_states_equal = unsafe { (*self.input.shared).eq(&*other.input.shared) };

        // Compare the rest of the triple buffer states
        shared_states_equal
            && (self.input.input_idx == other.input.input_idx)
            && (self.output.output_idx == other.output.output_idx)
    }
}

/// Producer interface to the triple buffer
///
/// The producer of data can use this struct to submit updates to the triple
/// buffer whenever he likes. These updates are nonblocking: a collision between
/// the producer and the consumer will result in cache contention, but deadlocks
/// and scheduling-induced slowdowns cannot happen.
///
#[derive(Debug)]
pub struct Input<T: Send> {
    /// Reference-counted shared state
    shared: Arc<SharedState<T>>,

    /// Index of the input buffer (which is private to the producer)
    input_idx: BufferIndex,
}
//
// Public interface
impl<T: Send> Input<T> {
    /// Write a new value into the triple buffer
    pub fn write(&mut self, value: T) {
        // Update the input buffer
        *self.input_buffer() = value;

        // Publish our update to the consumer
        self.publish();
    }

    /// Check if the consumer has fetched our last submission yet
    ///
    /// This method is only intended for diagnostics purposes. Please do not let
    /// it inform your decision of sending or not sending a value, as that would
    /// effectively be building a very poor spinlock-based double buffer
    /// implementation. If what you truly need is a double buffer, build
    /// yourself a proper blocking one instead of wasting CPU time.
    ///
    pub fn consumed(&self) -> bool {
        let back_info = self.shared.back_info.load(Ordering::Relaxed);
        back_info & BACK_DIRTY_BIT == 0
    }

    /// Access the input buffer directly
    ///
    /// This advanced interface allows you to update the input buffer in place,
    /// so that you can avoid creating values of type T repeatedy just to push
    /// them into the triple buffer when doing so is expensive.
    ///
    /// However, by using it, you force yourself to take into account some
    /// implementation subtleties that you could normally ignore.
    ///
    /// First, the buffer does not contain the last value that you published
    /// (which is now available to the consumer thread). In fact, what you get
    /// may not match _any_ value that you sent in the past, but rather be a new
    /// value that was written in there by the consumer thread. All you can
    /// safely assume is that the buffer contains a valid value of type T, which
    /// you may need to "clean up" before use using a type-specific process.
    ///
    /// Second, we do not send updates automatically. You need to call
    /// `publish()` in order to propagate a buffer update to the consumer.
    /// Alternative designs based on Drop were considered, but considered too
    /// magical for the target audience of this interface.
    ///
    pub fn input_buffer(&mut self) -> &mut T {
        // This is safe because the synchronization protocol ensures that we
        // have exclusive access to this buffer.
        let input_ptr = self.shared.buffers[self.input_idx as usize].get();
        unsafe { &mut *input_ptr }
    }

    /// Publish the current input buffer, checking for overwrites
    ///
    /// After updating the input buffer using `input_buffer()`, you can use this
    /// method to publish your updates to the consumer.
    ///
    /// This will replace the current input buffer with another one, as you
    /// cannot continue using the old one while the consumer is accessing it.
    ///
    /// It will also tell you whether you overwrote a value which was not read
    /// by the consumer thread.
    ///
    pub fn publish(&mut self) -> bool {
        // Swap the input buffer and the back buffer, setting the dirty bit
        //
        // The ordering must be AcqRel, because...
        //
        // - Our accesses to the old buffer must not be reordered after this
        //   operation (which mandates Release ordering), otherwise they could
        //   race with the consumer accessing the freshly published buffer.
        // - Our accesses from the buffer must not be reordered before this
        //   operation (which mandates Consume ordering, that is best
        //   approximated by Acquire in Rust), otherwise they would race with
        //   the consumer accessing the buffer as well before switching to
        //   another buffer.
        //   * This reordering may seem paradoxical, but could happen if the
        //     compiler or CPU correctly speculated the new buffer's index
        //     before that index is actually read, as well as on weird hardware
        //     with incoherent caches like GPUs or old DEC Alpha where keeping
        //     data in sync across cores requires manual action.
        //
        let former_back_info = self
            .shared
            .back_info
            .swap(self.input_idx | BACK_DIRTY_BIT, Ordering::AcqRel);

        // The old back buffer becomes our new input buffer
        self.input_idx = former_back_info & BACK_INDEX_MASK;

        // Tell whether we have overwritten unread data
        former_back_info & BACK_DIRTY_BIT != 0
    }

    /// Deprecated alias to `input_buffer()`, please use that method instead
    #[cfg(any(feature = "raw", test))]
    #[deprecated(
        since = "5.0.5",
        note = "The \"raw\" feature is deprecated as the performance \
                optimization that motivated it turned out to be incorrect. \
                All functionality is now available without using feature flags."
    )]
    pub fn raw_input_buffer(&mut self) -> &mut T {
        self.input_buffer()
    }

    /// Deprecated alias to `publish()`, please use that method instead
    #[cfg(any(feature = "raw", test))]
    #[deprecated(
        since = "5.0.5",
        note = "The \"raw\" feature is deprecated as the performance \
                optimization that motivated it turned out to be incorrect. \
                All functionality is now available without using feature flags."
    )]
    pub fn raw_publish(&mut self) -> bool {
        self.publish()
    }
}

/// Consumer interface to the triple buffer
///
/// The consumer of data can use this struct to access the latest published
/// update from the producer whenever he likes. Readout is nonblocking: a
/// collision between the producer and consumer will result in cache contention,
/// but deadlocks and scheduling-induced slowdowns cannot happen.
///
#[derive(Debug)]
pub struct Output<T: Send> {
    /// Reference-counted shared state
    shared: Arc<SharedState<T>>,

    /// Index of the output buffer (which is private to the consumer)
    output_idx: BufferIndex,
}
//
// Public interface
impl<T: Send> Output<T> {
    /// Access the latest value from the triple buffer
    pub fn read(&mut self) -> &T {
        // Fetch updates from the producer
        self.update();

        // Give access to the output buffer
        self.output_buffer()
    }

    /// Tell whether a buffer update is incoming from the producer
    ///
    /// This method is only intended for diagnostics purposes. Please do not let
    /// it inform your decision of reading a value or not, as that would
    /// effectively be building a very poor spinlock-based double buffer
    /// implementation. If what you truly need is a double buffer, build
    /// yourself a proper blocking one instead of wasting CPU time.
    ///
    pub fn updated(&self) -> bool {
        let back_info = self.shared.back_info.load(Ordering::Relaxed);
        back_info & BACK_DIRTY_BIT != 0
    }

    /// Access the output buffer directly
    ///
    /// This advanced interface allows you to modify the contents of the output
    /// buffer, so that you can avoid copying the output value when this is an
    /// expensive process. One possible application, for example, is to
    /// post-process values from the producer before use.
    ///
    /// However, by using it, you force yourself to take into account some
    /// implementation subtleties that you could normally ignore.
    ///
    /// First, keep in mind that you can lose access to the current output
    /// buffer any time `read()` or `update()` is called, as it may be replaced
    /// by an updated buffer from the producer automatically.
    ///
    /// Second, to reduce the potential for the aforementioned usage error, this
    /// method does not update the output buffer automatically. You need to call
    /// `update()` in order to fetch buffer updates from the producer.
    ///
    pub fn output_buffer(&mut self) -> &mut T {
        // This is safe because the synchronization protocol ensures that we
        // have exclusive access to this buffer.
        let output_ptr = self.shared.buffers[self.output_idx as usize].get();
        unsafe { &mut *output_ptr }
    }

    /// Update the output buffer
    ///
    /// Check if the producer submitted a new data version, and if one is
    /// available, update our output buffer to use it. Return a flag that tells
    /// you whether such an update was carried out.
    ///
    /// Bear in mind that when this happens, you will lose any change that you
    /// performed to the output buffer via the `output_buffer()` interface.
    ///
    pub fn update(&mut self) -> bool {
        // Access the shared state
        let shared_state = &(*self.shared);

        // Check if an update is present in the back-buffer
        let updated = self.updated();
        if updated {
            // If so, exchange our output buffer with the back-buffer, thusly
            // acquiring exclusive access to the old back buffer while giving
            // the producer a new back-buffer to write to.
            //
            // The ordering must be AcqRel, because...
            //
            // - Our accesses to the previous buffer must not be reordered after
            //   this operation (which mandates Release ordering), otherwise
            //   they could race with the producer accessing the freshly
            //   liberated buffer.
            // - Our accesses from the buffer must not be reordered before this
            //   operation (which mandates Consume ordering, that is best
            //   approximated by Acquire in Rust), otherwise they would race
            //   with the producer writing into the buffer before publishing it.
            //   * This reordering may seem paradoxical, but could happen if the
            //     compiler or CPU correctly speculated the new buffer's index
            //     before that index is actually read, as well as on weird hardware
            //     like GPUs where CPU caches require manual synchronization.
            //
            let former_back_info = shared_state
                .back_info
                .swap(self.output_idx, Ordering::AcqRel);

            // Make the old back-buffer our new output buffer
            self.output_idx = former_back_info & BACK_INDEX_MASK;
        }

        // Tell whether an update was carried out
        updated
    }

    /// Deprecated alias to `output_buffer()`, please use that method instead
    #[cfg(any(feature = "raw", test))]
    #[deprecated(
        since = "5.0.5",
        note = "The \"raw\" feature is deprecated as the performance \
                optimization that motivated it turned out to be incorrect. \
                All functionality is now available without using feature flags."
    )]
    pub fn raw_output_buffer(&mut self) -> &mut T {
        self.output_buffer()
    }
    /// Deprecated alias to `update()`, please use that method instead
    #[cfg(any(feature = "raw", test))]
    #[deprecated(
        since = "5.0.5",
        note = "The \"raw\" feature is deprecated as the performance \
                optimization that motivated it turned out to be incorrect. \
                All functionality is now available without using feature flags."
    )]
    #[cfg(any(feature = "raw", test))]
    pub fn raw_update(&mut self) -> bool {
        self.update()
    }
}

/// Triple buffer shared state
///
/// In a triple buffering communication protocol, the producer and consumer
/// share the following storage:
///
/// - Three memory buffers suitable for storing the data at hand
/// - Information about the back-buffer: which buffer is the current back-buffer
///   and whether an update was published since the last readout.
///
#[derive(Debug)]
struct SharedState<T: Send> {
    /// Data storage buffers
    buffers: [CachePadded<UnsafeCell<T>>; 3],

    /// Information about the current back-buffer state
    back_info: CachePadded<AtomicBackBufferInfo>,
}
//
#[doc(hidden)]
impl<T: Send> SharedState<T> {
    /// Given (a way to generate) buffer contents and the back info, build the shared state
    fn new(mut gen_buf_data: impl FnMut(usize) -> T, back_info: BackBufferInfo) -> Self {
        let mut make_buf = |i| -> CachePadded<UnsafeCell<T>> {
            CachePadded::new(UnsafeCell::new(gen_buf_data(i)))
        };
        Self {
            buffers: [make_buf(0), make_buf(1), make_buf(2)],
            back_info: CachePadded::new(AtomicBackBufferInfo::new(back_info)),
        }
    }
}
//
#[doc(hidden)]
impl<T: Clone + Send> SharedState<T> {
    /// Cloning the shared state is unsafe because you must ensure that no one
    /// is concurrently accessing it, since &self is enough for writing.
    unsafe fn clone(&self) -> Self {
        Self::new(
            |i| (*self.buffers[i].get()).clone(),
            self.back_info.load(Ordering::Relaxed),
        )
    }
}
//
#[doc(hidden)]
impl<T: PartialEq + Send> SharedState<T> {
    /// Equality is unsafe for the same reason as cloning: you must ensure that
    /// no one is concurrently accessing the triple buffer to avoid data races.
    unsafe fn eq(&self, other: &Self) -> bool {
        // Check whether the contents of all buffers are equal...
        let buffers_equal = self
            .buffers
            .iter()
            .zip(other.buffers.iter())
            .all(|tuple| -> bool {
                let (cell1, cell2) = tuple;
                *cell1.get() == *cell2.get()
            });

        // ...then check whether the rest of the shared state is equal
        buffers_equal
            && (self.back_info.load(Ordering::Relaxed) == other.back_info.load(Ordering::Relaxed))
    }
}
//
unsafe impl<T: Send> Sync for SharedState<T> {}

// Index types used for triple buffering
//
// These types are used to index into triple buffers. In addition, the
// BackBufferInfo type is actually a bitfield, whose third bit (numerical
// value: 4) is set to 1 to indicate that the producer published an update into
// the back-buffer, and reset to 0 when the consumer fetches the update.
//
type BufferIndex = u8;
type BackBufferInfo = BufferIndex;
//
type AtomicBackBufferInfo = AtomicU8;
const BACK_INDEX_MASK: u8 = 0b11; // Mask used to extract back-buffer index
const BACK_DIRTY_BIT: u8 = 0b100; // Bit set by producer to signal updates

/// Unit tests
#[cfg(test)]
mod tests {
    use super::{BufferIndex, SharedState, TripleBuffer, BACK_DIRTY_BIT, BACK_INDEX_MASK};

    use std::{fmt::Debug, ops::Deref, sync::atomic::Ordering, thread, time::Duration};

    use testbench::{
        self,
        race_cell::{RaceCell, Racey},
    };

    /// Check that triple buffers are properly initialized
    #[test]
    fn initial_state() {
        // Let's create a triple buffer
        let mut buf = TripleBuffer::new(42);
        check_buf_state(&mut buf, false);
        assert_eq!(*buf.output.read(), 42);
    }

    /// Check that the shared state's unsafe equality operator works
    #[test]
    fn partial_eq_shared() {
        // Let's create some dummy shared state
        let dummy_state = SharedState::<u16>::new(|i| [111, 222, 333][i], 0b10);

        // Check that the dummy state is equal to itself
        assert!(unsafe { dummy_state.eq(&dummy_state) });

        // Check that it's not equal to a state where buffer contents differ
        assert!(unsafe { !dummy_state.eq(&SharedState::<u16>::new(|i| [114, 222, 333][i], 0b10)) });
        assert!(unsafe { !dummy_state.eq(&SharedState::<u16>::new(|i| [111, 225, 333][i], 0b10)) });
        assert!(unsafe { !dummy_state.eq(&SharedState::<u16>::new(|i| [111, 222, 336][i], 0b10)) });

        // Check that it's not equal to a state where the back info differs
        assert!(unsafe {
            !dummy_state.eq(&SharedState::<u16>::new(
                |i| [111, 222, 333][i],
                BACK_DIRTY_BIT & 0b10,
            ))
        });
        assert!(unsafe { !dummy_state.eq(&SharedState::<u16>::new(|i| [111, 222, 333][i], 0b01)) });
    }

    /// Check that TripleBuffer's PartialEq impl works
    #[test]
    fn partial_eq() {
        // Create a triple buffer
        let buf = TripleBuffer::new("test");

        // Check that it is equal to itself
        assert_eq!(buf, buf);

        // Make another buffer with different contents. As buffer creation is
        // deterministic, this should only have an impact on the shared state,
        // but the buffers should nevertheless be considered different.
        let buf2 = TripleBuffer::new("taste");
        assert_eq!(buf.input.input_idx, buf2.input.input_idx);
        assert_eq!(buf.output.output_idx, buf2.output.output_idx);
        assert!(buf != buf2);

        // Check that changing either the input or output buffer index will
        // also lead two TripleBuffers to be considered different (this test
        // technically creates an invalid TripleBuffer state, but it's the only
        // way to check that the PartialEq impl is exhaustive)
        let mut buf3 = TripleBuffer::new("test");
        assert_eq!(buf, buf3);
        let old_input_idx = buf3.input.input_idx;
        buf3.input.input_idx = buf3.output.output_idx;
        assert!(buf != buf3);
        buf3.input.input_idx = old_input_idx;
        buf3.output.output_idx = old_input_idx;
        assert!(buf != buf3);
    }

    /// Check that the shared state's unsafe clone operator works
    #[test]
    fn clone_shared() {
        // Let's create some dummy shared state
        let dummy_state = SharedState::<u8>::new(|i| [123, 231, 132][i], BACK_DIRTY_BIT & 0b01);

        // Now, try to clone it
        let dummy_state_copy = unsafe { dummy_state.clone() };

        // Check that the contents of the original state did not change
        assert!(unsafe {
            dummy_state.eq(&SharedState::<u8>::new(
                |i| [123, 231, 132][i],
                BACK_DIRTY_BIT & 0b01,
            ))
        });

        // Check that the contents of the original and final state are identical
        assert!(unsafe { dummy_state.eq(&dummy_state_copy) });
    }

    /// Check that TripleBuffer's Clone impl works
    #[test]
    fn clone() {
        // Create a triple buffer
        let mut buf = TripleBuffer::new(4.2);

        // Put it in a nontrivial state
        unsafe {
            *buf.input.shared.buffers[0].get() = 1.2;
            *buf.input.shared.buffers[1].get() = 3.4;
            *buf.input.shared.buffers[2].get() = 5.6;
        }
        buf.input
            .shared
            .back_info
            .store(BACK_DIRTY_BIT & 0b01, Ordering::Relaxed);
        buf.input.input_idx = 0b10;
        buf.output.output_idx = 0b00;

        // Now clone it
        let buf_clone = buf.clone();

        // Check that the clone uses its own, separate shared data storage
        assert_eq!(
            as_ptr(&buf_clone.output.shared),
            as_ptr(&buf_clone.output.shared)
        );
        assert!(as_ptr(&buf_clone.input.shared) != as_ptr(&buf.input.shared));

        // Check that it is identical from PartialEq's point of view
        assert_eq!(buf, buf_clone);

        // Check that the contents of the original buffer did not change
        unsafe {
            assert_eq!(*buf.input.shared.buffers[0].get(), 1.2);
            assert_eq!(*buf.input.shared.buffers[1].get(), 3.4);
            assert_eq!(*buf.input.shared.buffers[2].get(), 5.6);
        }
        assert_eq!(
            buf.input.shared.back_info.load(Ordering::Relaxed),
            BACK_DIRTY_BIT & 0b01
        );
        assert_eq!(buf.input.input_idx, 0b10);
        assert_eq!(buf.output.output_idx, 0b00);
    }

    /// Check that the low-level publish/update primitives work
    #[test]
    fn swaps() {
        // Create a new buffer, and a way to track any changes to it
        let mut buf = TripleBuffer::new([123, 456]);
        let old_buf = buf.clone();
        let old_input_idx = old_buf.input.input_idx;
        let old_shared = &old_buf.input.shared;
        let old_back_info = old_shared.back_info.load(Ordering::Relaxed);
        let old_back_idx = old_back_info & BACK_INDEX_MASK;
        let old_output_idx = old_buf.output.output_idx;

        // Check that updating from a clean state works
        assert!(!buf.output.update());
        assert_eq!(buf, old_buf);
        check_buf_state(&mut buf, false);

        // Check that publishing from a clean state works
        assert!(!buf.input.publish());
        let mut expected_buf = old_buf.clone();
        expected_buf.input.input_idx = old_back_idx;
        expected_buf
            .input
            .shared
            .back_info
            .store(old_input_idx | BACK_DIRTY_BIT, Ordering::Relaxed);
        assert_eq!(buf, expected_buf);
        check_buf_state(&mut buf, true);

        // Check that overwriting a dirty state works
        assert!(buf.input.publish());
        let mut expected_buf = old_buf.clone();
        expected_buf.input.input_idx = old_input_idx;
        expected_buf
            .input
            .shared
            .back_info
            .store(old_back_idx | BACK_DIRTY_BIT, Ordering::Relaxed);
        assert_eq!(buf, expected_buf);
        check_buf_state(&mut buf, true);

        // Check that updating from a dirty state works
        assert!(buf.output.update());
        expected_buf.output.output_idx = old_back_idx;
        expected_buf
            .output
            .shared
            .back_info
            .store(old_output_idx, Ordering::Relaxed);
        assert_eq!(buf, expected_buf);
        check_buf_state(&mut buf, false);
    }

    /// Check that (sequentially) writing to a triple buffer works
    #[test]
    fn sequential_write() {
        // Let's create a triple buffer
        let mut buf = TripleBuffer::new(false);

        // Back up the initial buffer state
        let old_buf = buf.clone();

        // Perform a write
        buf.input.write(true);

        // Check new implementation state
        {
            // Starting from the old buffer state...
            let mut expected_buf = old_buf.clone();

            // ...write the new value in and swap...
            *expected_buf.input.input_buffer() = true;
            expected_buf.input.publish();

            // Nothing else should have changed
            assert_eq!(buf, expected_buf);
            check_buf_state(&mut buf, true);
        }
    }

    /// Check that (sequentially) reading from a triple buffer works
    #[test]
    fn sequential_read() {
        // Let's create a triple buffer and write into it
        let mut buf = TripleBuffer::new(1.0);
        buf.input.write(4.2);

        // Test readout from dirty (freshly written) triple buffer
        {
            // Back up the initial buffer state
            let old_buf = buf.clone();

            // Read from the buffer
            let result = *buf.output.read();

            // Output value should be correct
            assert_eq!(result, 4.2);

            // Result should be equivalent to carrying out an update
            let mut expected_buf = old_buf.clone();
            assert!(expected_buf.output.update());
            assert_eq!(buf, expected_buf);
            check_buf_state(&mut buf, false);
        }

        // Test readout from clean (unchanged) triple buffer
        {
            // Back up the initial buffer state
            let old_buf = buf.clone();

            // Read from the buffer
            let result = *buf.output.read();

            // Output value should be correct
            assert_eq!(result, 4.2);

            // Buffer state should be unchanged
            assert_eq!(buf, old_buf);
            check_buf_state(&mut buf, false);
        }
    }

    /// Check that contended concurrent reads and writes work
    #[test]
    #[ignore]
    fn contended_concurrent_read_write() {
        // We will stress the infrastructure by performing this many writes
        // as a reader continuously reads the latest value
        const TEST_WRITE_COUNT: usize = 100_000_000;

        // This is the buffer that our reader and writer will share
        let buf = TripleBuffer::new(RaceCell::new(0));
        let (mut buf_input, mut buf_output) = buf.split();

        // Concurrently run a writer which increments a shared value in a loop,
        // and a reader which makes sure that no unexpected value slips in.
        let mut last_value = 0usize;
        testbench::concurrent_test_2(
            move || {
                for value in 1..=TEST_WRITE_COUNT {
                    buf_input.write(RaceCell::new(value));
                }
            },
            move || {
                while last_value < TEST_WRITE_COUNT {
                    let new_racey_value = buf_output.read().get();
                    match new_racey_value {
                        Racey::Consistent(new_value) => {
                            assert!((new_value >= last_value) && (new_value <= TEST_WRITE_COUNT));
                            last_value = new_value;
                        }
                        Racey::Inconsistent => {
                            panic!("Inconsistent state exposed by the buffer!");
                        }
                    }
                }
            },
        );
    }

    /// Check that uncontended concurrent reads and writes work
    ///
    /// **WARNING:** This test unfortunately needs to have timing-dependent
    /// behaviour to do its job. If it fails for you, try the following:
    ///
    /// - Close running applications in the background
    /// - Re-run the tests with only one OS thread (--test-threads=1)
    /// - Increase the writer sleep period
    ///
    #[test]
    #[ignore]
    fn uncontended_concurrent_read_write() {
        // We will stress the infrastructure by performing this many writes
        // as a reader continuously reads the latest value
        const TEST_WRITE_COUNT: usize = 625;

        // This is the buffer that our reader and writer will share
        let buf = TripleBuffer::new(RaceCell::new(0));
        let (mut buf_input, mut buf_output) = buf.split();

        // Concurrently run a writer which slowly increments a shared value,
        // and a reader which checks that it can receive every update
        let mut last_value = 0usize;
        testbench::concurrent_test_2(
            move || {
                for value in 1..=TEST_WRITE_COUNT {
                    buf_input.write(RaceCell::new(value));
                    thread::yield_now();
                    thread::sleep(Duration::from_millis(32));
                }
            },
            move || {
                while last_value < TEST_WRITE_COUNT {
                    let new_racey_value = buf_output.read().get();
                    match new_racey_value {
                        Racey::Consistent(new_value) => {
                            assert!((new_value >= last_value) && (new_value - last_value <= 1));
                            last_value = new_value;
                        }
                        Racey::Inconsistent => {
                            panic!("Inconsistent state exposed by the buffer!");
                        }
                    }
                }
            },
        );
    }

    /// Through the low-level API, the consumer is allowed to modify its
    /// bufffer, which means that it will unknowingly send back data to the
    /// producer. This creates new correctness requirements for the
    /// synchronization protocol, which must be checked as well.
    #[test]
    #[ignore]
    fn concurrent_bidirectional_exchange() {
        // We will stress the infrastructure by performing this many writes
        // as a reader continuously reads the latest value
        const TEST_WRITE_COUNT: usize = 100_000_000;

        // This is the buffer that our reader and writer will share
        let buf = TripleBuffer::new(RaceCell::new(0));
        let (mut buf_input, mut buf_output) = buf.split();

        // Concurrently run a writer which increments a shared value in a loop,
        // and a reader which makes sure that no unexpected value slips in.
        testbench::concurrent_test_2(
            move || {
                for new_value in 1..=TEST_WRITE_COUNT {
                    match buf_input.input_buffer().get() {
                        Racey::Consistent(curr_value) => {
                            assert!(curr_value <= new_value);
                        }
                        Racey::Inconsistent => {
                            panic!("Inconsistent state exposed by the buffer!");
                        }
                    }
                    buf_input.write(RaceCell::new(new_value));
                }
            },
            move || {
                let mut last_value = 0usize;
                while last_value < TEST_WRITE_COUNT {
                    match buf_output.output_buffer().get() {
                        Racey::Consistent(new_value) => {
                            assert!((new_value >= last_value) && (new_value <= TEST_WRITE_COUNT));
                            last_value = new_value;
                        }
                        Racey::Inconsistent => {
                            panic!("Inconsistent state exposed by the buffer!");
                        }
                    }
                    if buf_output.updated() {
                        buf_output.output_buffer().set(last_value / 2);
                        buf_output.update();
                    }
                }
            },
        );
    }

    /// Range check for triple buffer indexes
    #[allow(unused_comparisons)]
    fn index_in_range(idx: BufferIndex) -> bool {
        (idx >= 0) & (idx <= 2)
    }

    /// Get a pointer to the target of some reference (e.g. an &, an Arc...)
    fn as_ptr<P: Deref>(ref_like: &P) -> *const P::Target {
        &(**ref_like) as *const _
    }

    /// Check the state of a buffer, and the effect of queries on it
    fn check_buf_state<T>(buf: &mut TripleBuffer<T>, expected_dirty_bit: bool)
    where
        T: Clone + Debug + PartialEq + Send,
    {
        // Make a backup of the buffer's initial state
        let initial_buf = buf.clone();

        // Check that the input and output point to the same shared state
        assert_eq!(as_ptr(&buf.input.shared), as_ptr(&buf.output.shared));

        // Access the shared state and decode back-buffer information
        let back_info = buf.input.shared.back_info.load(Ordering::Relaxed);
        let back_idx = back_info & BACK_INDEX_MASK;
        let back_buffer_dirty = back_info & BACK_DIRTY_BIT != 0;

        // Input-/output-/back-buffer indexes must be in range
        assert!(index_in_range(buf.input.input_idx));
        assert!(index_in_range(buf.output.output_idx));
        assert!(index_in_range(back_idx));

        // Input-/output-/back-buffer indexes must be distinct
        assert!(buf.input.input_idx != buf.output.output_idx);
        assert!(buf.input.input_idx != back_idx);
        assert!(buf.output.output_idx != back_idx);

        // Back-buffer must have the expected dirty bit
        assert_eq!(back_buffer_dirty, expected_dirty_bit);

        // Check that the "input buffer" query behaves as expected
        assert_eq!(
            as_ptr(&buf.input.input_buffer()),
            buf.input.shared.buffers[buf.input.input_idx as usize].get()
        );
        assert_eq!(*buf, initial_buf);

        // Check that the "consumed" query behaves as expected
        assert_eq!(!buf.input.consumed(), expected_dirty_bit);
        assert_eq!(*buf, initial_buf);

        // Check that the output_buffer query works in the initial state
        assert_eq!(
            as_ptr(&buf.output.output_buffer()),
            buf.output.shared.buffers[buf.output.output_idx as usize].get()
        );
        assert_eq!(*buf, initial_buf);

        // Check that the output buffer query works in the initial state
        assert_eq!(buf.output.updated(), expected_dirty_bit);
        assert_eq!(*buf, initial_buf);
    }
}
