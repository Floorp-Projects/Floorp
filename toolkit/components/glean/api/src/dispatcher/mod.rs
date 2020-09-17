// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! A global dispatcher queue.
//!
//! # Example - Local Dispatch queue
//!
//! ```rust
//! # use dispatcher::Dispatcher;
//! let mut dispatcher = Dispatcher::new(10);
//!
//! dispatcher.launch(|| {
//!     println!("An early task to be queued up");
//! });
//!
//! // Ensure the dispatcher queue is being worked on.
//! dispatcher.flush_init();
//!
//! dispatcher.launch(|| {
//!     println!("Executing expensive task");
//!     // Run your expensive task in a separate thread.
//! });
//!
//! dispatcher.launch(|| {
//!     println!("A second task that's executed sequentially, but off the main thread.");
//! });
//!
//! // Finally stop the dispatcher
//! dispatcher.try_shutdown().unwrap();
//! # dispatcher.join().unwrap();
//! ```
//!
//! # Example - Global Dispatch queue
//!
//! The global dispatch queue is pre-configured with a maximum queue size of 100 tasks.
//!
//! ```rust
//! // Ensure the dispatcher queue is being worked on.
//! dispatcher::flush_init();
//!
//! dispatcher::launch(|| {
//!     println!("Executing expensive task");
//!     // Run your expensive task in a separate thread.
//! });
//!
//! dispatcher::launch(|| {
//!     println!("A second task that's executed sequentially, but off the main thread.");
//! });
//! ```

use std::{
    mem,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
    thread,
};

use crossbeam_channel::{bounded, unbounded, SendError, Sender, TrySendError};
use thiserror::Error;

pub use global::*;

mod global;

/// The command a worker should execute.
enum Command {
    /// A task is a user-defined function to run.
    Task(Box<dyn FnOnce() + Send>),

    /// Swap the channel
    Swap(Sender<()>),

    /// Signal the worker to finish work and shut down.
    Shutdown,
}

/// The error returned from operations on the dispatcher
#[derive(Error, Debug, PartialEq)]
pub enum DispatchError {
    #[error("The worker panicked while running a task")]
    WorkerPanic,

    #[error("Maximum queue size reached")]
    QueueFull,

    #[error("Pre-init buffer was already flushed")]
    AlreadyFlushed,

    #[error("Failed to send command to worker thread")]
    SendError,

    #[error("Failed to receive from channel")]
    RecvError(#[from] crossbeam_channel::RecvError),
}

impl From<TrySendError<Command>> for DispatchError {
    fn from(err: TrySendError<Command>) -> Self {
        match err {
            TrySendError::Full(_) => DispatchError::QueueFull,
            _ => DispatchError::SendError,
        }
    }
}

impl<T> From<SendError<T>> for DispatchError {
    fn from(_: SendError<T>) -> Self {
        DispatchError::SendError
    }
}

/// A clonable guard for a dispatch queue.
#[derive(Clone)]
struct DispatchGuard {
    queue_preinit: Arc<AtomicBool>,
    preinit: Sender<Command>,
    queue: Sender<Command>,
}

impl DispatchGuard {
    pub fn launch(&self, task: impl FnOnce() + Send + 'static) -> Result<(), DispatchError> {
        let task = Command::Task(Box::new(task));
        self.send(task)
    }

    pub fn shutdown(&self) -> Result<(), DispatchError> {
        self.send(Command::Shutdown)
    }

    fn send(&self, task: Command) -> Result<(), DispatchError> {
        if self.queue_preinit.load(Ordering::SeqCst) {
            match self.preinit.try_send(task) {
                Ok(()) => Ok(()),
                Err(TrySendError::Full(_)) => Err(DispatchError::QueueFull),
                Err(TrySendError::Disconnected(_)) => Err(DispatchError::SendError),
            }
        } else {
            self.queue.send(task)?;
            Ok(())
        }
    }
}

/// A dispatcher.
///
/// Run expensive processing tasks sequentially off the main thread.
/// Tasks are processed in a single separate thread in the order they are submitted.
/// The dispatch queue will enqueue tasks while not flushed, up to the maximum queue size.
/// Processing will start after flushing once, processing already enqueued tasks first, then
/// waiting for further tasks to be enqueued.
///
/// # Example
///
/// ```rust
/// # use dispatcher::Dispatcher;
/// let mut dispatcher = Dispatcher::new(5);
/// dispatcher.flush_init();
///
/// dispatcher.launch(|| {
///     println!("A task of the main thread");
/// }).unwrap();
///
/// dispatcher.try_shutdown().unwrap();
/// dispatcher.join().unwrap();
/// ```
pub struct Dispatcher {
    /// Whether to queue on the preinit buffer or on the unbounded queue
    queue_preinit: Arc<AtomicBool>,

    /// Used to unblock the worker thread initially.
    block_sender: Sender<()>,

    /// Sender for the preinit queue.
    preinit_sender: Sender<Command>,

    /// Sender for the unbounded queue.
    sender: Sender<Command>,
}

impl Dispatcher {
    /// Creates a new dispatcher with a maximum queue size.
    ///
    /// Launched tasks won't run until [`flush_init`] is called.
    ///
    /// [`flush_init`]: #method.flush_init
    pub fn new(max_queue_size: usize) -> Self {
        let (block_sender, block_receiver) = bounded(0);
        let (preinit_sender, preinit_receiver) = bounded(max_queue_size);
        let (sender, mut unbounded_receiver) = unbounded();

        let queue_preinit = Arc::new(AtomicBool::new(true));

        thread::spawn(move || {
            if let Err(_) = block_receiver.recv() {
                // The other side was disconnected.
                // There's nothing the worker thread can do.
                log::error!("The task producer was disconnected. Worker thread will exit.");
                return;
            }

            let mut receiver = preinit_receiver;
            loop {
                use Command::*;

                match receiver.recv() {
                    Ok(Shutdown) => {
                        break;
                    }

                    Ok(Task(f)) => {
                        (f)();
                    }

                    Ok(Swap(swap_done)) => {
                        // A swap should only occur exactly once.
                        // This is upheld by `flush_init`, which errors out if the preinit buffer
                        // was already flushed.

                        // We swap the channels we listen on for new tasks.
                        // The next iteration will continue with the unbounded queue.
                        mem::swap(&mut receiver, &mut unbounded_receiver);

                        // The swap command MUST be the last one received on the preinit buffer,
                        // so by the time we run this we know all preinit tasks were processed.
                        // We can notify the other side.
                        swap_done
                            .send(())
                            .expect("The caller of `flush_init` has gone missing");
                    }

                    // Other side was disconnected.
                    Err(_) => {
                        log::error!("The task producer was disconnected. Worker thread will exit.");
                        return;
                    }
                }
            }
        });

        Dispatcher {
            queue_preinit,
            block_sender,
            preinit_sender,
            sender,
        }
    }

    fn guard(&self) -> DispatchGuard {
        DispatchGuard {
            queue_preinit: Arc::clone(&self.queue_preinit),
            preinit: self.preinit_sender.clone(),
            queue: self.sender.clone(),
        }
    }

    /// Flushes the pre-init buffer.
    ///
    /// This function blocks until tasks queued prior to this call are finished.
    /// Once the initial queue is empty the dispatcher will wait for new tasks to be launched.
    ///
    /// Returns an error if called multiple times.
    pub fn flush_init(&mut self) -> Result<(), DispatchError> {
        // We immediately stop queueing in the pre-init buffer.
        let old_val = self.queue_preinit.swap(false, Ordering::SeqCst);
        if !old_val {
            return Err(DispatchError::AlreadyFlushed);
        }

        // Unblock the worker thread exactly once.
        self.block_sender.send(())?;

        // Single-use channel to communicate with the worker thread.
        let (swap_sender, swap_receiver) = bounded(0);

        // Send final command and block until it is sent.
        self.preinit_sender
            .send(Command::Swap(swap_sender))
            .map_err(|_| DispatchError::SendError)?;

        // Now wait for the worker thread to do the swap and inform us.
        // This blocks until all tasks in the preinit buffer have been processed.
        swap_receiver.recv()?;
        Ok(())
    }
}
