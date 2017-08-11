#[cfg(unix)]
mod unix;

#[cfg(unix)]
pub use self::unix::{
    IoVec,
    MAX_LENGTH,
};

#[cfg(windows)]
mod windows;

#[cfg(windows)]
pub use self::windows::{
    IoVec,
    MAX_LENGTH,
};
