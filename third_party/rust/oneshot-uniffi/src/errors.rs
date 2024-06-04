use super::{dealloc, Channel};
use core::fmt;
use core::mem;
use core::ptr::NonNull;

/// An error returned when trying to send on a closed channel. Returned from
/// [`Sender::send`](crate::Sender::send) if the corresponding [`Receiver`](crate::Receiver)
/// has already been dropped.
///
/// The message that could not be sent can be retreived again with [`SendError::into_inner`].
pub struct SendError<T> {
    channel_ptr: NonNull<Channel<T>>,
}

unsafe impl<T: Send> Send for SendError<T> {}
unsafe impl<T: Sync> Sync for SendError<T> {}

impl<T> SendError<T> {
    /// # Safety
    ///
    /// By calling this function, the caller semantically transfers ownership of the
    /// channel's resources to the created `SendError`. Thus the caller must ensure that the
    /// pointer is not used in a way which would violate this ownership transfer. Moreover,
    /// the caller must assert that the channel contains a valid, initialized message.
    pub(crate) const unsafe fn new(channel_ptr: NonNull<Channel<T>>) -> Self {
        Self { channel_ptr }
    }

    /// Consumes the error and returns the message that failed to be sent.
    #[inline]
    pub fn into_inner(self) -> T {
        let channel_ptr = self.channel_ptr;

        // Don't run destructor if we consumed ourselves. Freeing happens here.
        mem::forget(self);

        // SAFETY: we have ownership of the channel
        let channel: &Channel<T> = unsafe { channel_ptr.as_ref() };

        // SAFETY: we know that the message is initialized according to the safety requirements of
        // `new`
        let message = unsafe { channel.take_message() };

        // SAFETY: we own the channel
        unsafe { dealloc(channel_ptr) };

        message
    }

    /// Get a reference to the message that failed to be sent.
    #[inline]
    pub fn as_inner(&self) -> &T {
        unsafe { self.channel_ptr.as_ref().message().assume_init_ref() }
    }
}

impl<T> Drop for SendError<T> {
    fn drop(&mut self) {
        // SAFETY: we have ownership of the channel and require that the message is initialized
        // upon construction
        unsafe {
            self.channel_ptr.as_ref().drop_message();
            dealloc(self.channel_ptr);
        }
    }
}

impl<T> fmt::Display for SendError<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        "sending on a closed channel".fmt(f)
    }
}

impl<T> fmt::Debug for SendError<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "SendError<{}>(_)", stringify!(T))
    }
}

#[cfg(feature = "std")]
impl<T> std::error::Error for SendError<T> {}

/// An error returned from the blocking [`Receiver::recv`](crate::Receiver::recv) method.
///
/// The receive operation can only fail if the corresponding [`Sender`](crate::Sender) was dropped
/// before sending any message, or if a message has already been sent and received on the channel.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct RecvError;

impl fmt::Display for RecvError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        "receiving on a closed channel".fmt(f)
    }
}

#[cfg(feature = "std")]
impl std::error::Error for RecvError {}

/// An error returned when failing to receive a message in the non-blocking
/// [`Receiver::try_recv`](crate::Receiver::try_recv).
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum TryRecvError {
    /// The channel is still open, but there was no message present in it.
    Empty,

    /// The channel is closed. Either the sender was dropped before sending any message, or the
    /// message has already been extracted from the receiver.
    Disconnected,
}

impl fmt::Display for TryRecvError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let msg = match self {
            TryRecvError::Empty => "receiving on an empty channel",
            TryRecvError::Disconnected => "receiving on a closed channel",
        };
        msg.fmt(f)
    }
}

#[cfg(feature = "std")]
impl std::error::Error for TryRecvError {}

/// An error returned when failing to receive a message in
/// [`Receiver::recv_timeout`](crate::Receiver::recv_timeout).
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum RecvTimeoutError {
    /// No message arrived on the channel before the timeout was reached. The channel is still open.
    Timeout,

    /// The channel is closed. Either the sender was dropped before sending any message, or the
    /// message has already been extracted from the receiver.
    Disconnected,
}

impl fmt::Display for RecvTimeoutError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let msg = match self {
            RecvTimeoutError::Timeout => "timed out waiting on channel",
            RecvTimeoutError::Disconnected => "channel is empty and sending half is closed",
        };
        msg.fmt(f)
    }
}

#[cfg(feature = "std")]
impl std::error::Error for RecvTimeoutError {}
