use std::thread::Thread;
use thiserror::Error;

#[derive(Error, Debug)]
#[error("thread: {thread}")]
pub struct Error {
    thread: Thread,
}

fn main() {}
