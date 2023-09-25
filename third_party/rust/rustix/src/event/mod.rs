//! Event operations.

#[cfg(any(
    linux_kernel,
    target_os = "freebsd",
    target_os = "illumos",
    target_os = "espidf"
))]
mod eventfd;
#[cfg(all(feature = "alloc", bsd))]
pub mod kqueue;
mod poll;
#[cfg(solarish)]
pub mod port;

#[cfg(all(feature = "alloc", linux_kernel))]
pub use crate::backend::event::epoll;
#[cfg(any(
    linux_kernel,
    target_os = "freebsd",
    target_os = "illumos",
    target_os = "espidf"
))]
pub use eventfd::{eventfd, EventfdFlags};
pub use poll::{poll, PollFd, PollFlags};
