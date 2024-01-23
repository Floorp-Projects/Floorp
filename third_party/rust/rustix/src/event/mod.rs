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
#[cfg(not(any(windows, target_os = "redox", target_os = "wasi")))]
mod pause;
mod poll;
#[cfg(solarish)]
pub mod port;

#[cfg(linux_kernel)]
pub use crate::backend::event::epoll;
#[cfg(any(
    linux_kernel,
    target_os = "freebsd",
    target_os = "illumos",
    target_os = "espidf"
))]
pub use eventfd::{eventfd, EventfdFlags};
#[cfg(not(any(windows, target_os = "redox", target_os = "wasi")))]
pub use pause::*;
pub use poll::{poll, PollFd, PollFlags};
