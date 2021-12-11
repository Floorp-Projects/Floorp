//! An implementation of the GNU make jobserver.
//!
//! This crate is an implementation, in Rust, of the GNU `make` jobserver for
//! CLI tools that are interoperating with make or otherwise require some form
//! of parallelism limiting across process boundaries. This was originally
//! written for usage in Cargo to both (a) work when `cargo` is invoked from
//! `make` (using `make`'s jobserver) and (b) work when `cargo` invokes build
//! scripts, exporting a jobserver implementation for `make` processes to
//! transitively use.
//!
//! The jobserver implementation can be found in [detail online][docs] but
//! basically boils down to a cross-process semaphore. On Unix this is
//! implemented with the `pipe` syscall and read/write ends of a pipe and on
//! Windows this is implemented literally with IPC semaphores.
//!
//! The jobserver protocol in `make` also dictates when tokens are acquired to
//! run child work, and clients using this crate should take care to implement
//! such details to ensure correct interoperation with `make` itself.
//!
//! ## Examples
//!
//! Connect to a jobserver that was set up by `make` or a different process:
//!
//! ```no_run
//! use jobserver::Client;
//!
//! // See API documentation for why this is `unsafe`
//! let client = match unsafe { Client::from_env() } {
//!     Some(client) => client,
//!     None => panic!("client not configured"),
//! };
//! ```
//!
//! Acquire and release token from a jobserver:
//!
//! ```no_run
//! use jobserver::Client;
//!
//! let client = unsafe { Client::from_env().unwrap() };
//! let token = client.acquire().unwrap(); // blocks until it is available
//! drop(token); // releases the token when the work is done
//! ```
//!
//! Create a new jobserver and configure a child process to have access:
//!
//! ```
//! use std::process::Command;
//! use jobserver::Client;
//!
//! let client = Client::new(4).expect("failed to create jobserver");
//! let mut cmd = Command::new("make");
//! client.configure(&mut cmd);
//! ```
//!
//! ## Caveats
//!
//! This crate makes no attempt to release tokens back to a jobserver on
//! abnormal exit of a process. If a process which acquires a token is killed
//! with ctrl-c or some similar signal then tokens will not be released and the
//! jobserver may be in a corrupt state.
//!
//! Note that this is typically ok as ctrl-c means that an entire build process
//! is being torn down, but it's worth being aware of at least!
//!
//! ## Windows caveats
//!
//! There appear to be two implementations of `make` on Windows. On MSYS2 one
//! typically comes as `mingw32-make` and the other as `make` itself. I'm not
//! personally too familiar with what's going on here, but for jobserver-related
//! information the `mingw32-make` implementation uses Windows semaphores
//! whereas the `make` program does not. The `make` program appears to use file
//! descriptors and I'm not really sure how it works, so this crate is not
//! compatible with `make` on Windows. It is, however, compatible with
//! `mingw32-make`.
//!
//! [docs]: http://make.mad-scientist.net/papers/jobserver-implementation/

#![deny(missing_docs, missing_debug_implementations)]
#![doc(html_root_url = "https://docs.rs/jobserver/0.1")]

use std::env;
use std::io;
use std::process::Command;
use std::sync::{Arc, Condvar, Mutex, MutexGuard};

#[cfg(unix)]
#[path = "unix.rs"]
mod imp;
#[cfg(windows)]
#[path = "windows.rs"]
mod imp;
#[cfg(not(any(unix, windows)))]
#[path = "wasm.rs"]
mod imp;

/// A client of a jobserver
///
/// This structure is the main type exposed by this library, and is where
/// interaction to a jobserver is configured through. Clients are either created
/// from scratch in which case the internal semphore is initialied on the spot,
/// or a client is created from the environment to connect to a jobserver
/// already created.
///
/// Some usage examples can be found in the crate documentation for using a
/// client.
///
/// Note that a `Client` implements the `Clone` trait, and all instances of a
/// `Client` refer to the same jobserver instance.
#[derive(Clone, Debug)]
pub struct Client {
    inner: Arc<imp::Client>,
}

/// An acquired token from a jobserver.
///
/// This token will be released back to the jobserver when it is dropped and
/// otherwise represents the ability to spawn off another thread of work.
#[derive(Debug)]
pub struct Acquired {
    client: Arc<imp::Client>,
    data: imp::Acquired,
    disabled: bool,
}

impl Acquired {
    /// This drops the `Acquired` token without releasing the associated token.
    ///
    /// This is not generally useful, but can be helpful if you do not have the
    /// ability to store an Acquired token but need to not yet release it.
    ///
    /// You'll typically want to follow this up with a call to `release_raw` or
    /// similar to actually release the token later on.
    pub fn drop_without_releasing(mut self) {
        self.disabled = true;
    }
}

#[derive(Default, Debug)]
struct HelperState {
    lock: Mutex<HelperInner>,
    cvar: Condvar,
}

#[derive(Default, Debug)]
struct HelperInner {
    requests: usize,
    producer_done: bool,
    consumer_done: bool,
}

impl Client {
    /// Creates a new jobserver initialized with the given parallelism limit.
    ///
    /// A client to the jobserver created will be returned. This client will
    /// allow at most `limit` tokens to be acquired from it in parallel. More
    /// calls to `acquire` will cause the calling thread to block.
    ///
    /// Note that the created `Client` is not automatically inherited into
    /// spawned child processes from this program. Manual usage of the
    /// `configure` function is required for a child process to have access to a
    /// job server.
    ///
    /// # Examples
    ///
    /// ```
    /// use jobserver::Client;
    ///
    /// let client = Client::new(4).expect("failed to create jobserver");
    /// ```
    ///
    /// # Errors
    ///
    /// Returns an error if any I/O error happens when attempting to create the
    /// jobserver client.
    pub fn new(limit: usize) -> io::Result<Client> {
        Ok(Client {
            inner: Arc::new(imp::Client::new(limit)?),
        })
    }

    /// Attempts to connect to the jobserver specified in this process's
    /// environment.
    ///
    /// When the a `make` executable calls a child process it will configure the
    /// environment of the child to ensure that it has handles to the jobserver
    /// it's passing down. This function will attempt to look for these details
    /// and connect to the jobserver.
    ///
    /// Note that the created `Client` is not automatically inherited into
    /// spawned child processes from this program. Manual usage of the
    /// `configure` function is required for a child process to have access to a
    /// job server.
    ///
    /// # Return value
    ///
    /// If a jobserver was found in the environment and it looks correct then
    /// `Some` of the connected client will be returned. If no jobserver was
    /// found then `None` will be returned.
    ///
    /// Note that on Unix the `Client` returned **takes ownership of the file
    /// descriptors specified in the environment**. Jobservers on Unix are
    /// implemented with `pipe` file descriptors, and they're inherited from
    /// parent processes. This `Client` returned takes ownership of the file
    /// descriptors for this process and will close the file descriptors after
    /// this value is dropped.
    ///
    /// Additionally on Unix this function will configure the file descriptors
    /// with `CLOEXEC` so they're not automatically inherited by spawned
    /// children.
    ///
    /// # Unsafety
    ///
    /// This function is `unsafe` to call on Unix specifically as it
    /// transitively requires usage of the `from_raw_fd` function, which is
    /// itself unsafe in some circumstances.
    ///
    /// It's recommended to call this function very early in the lifetime of a
    /// program before any other file descriptors are opened. That way you can
    /// make sure to take ownership properly of the file descriptors passed
    /// down, if any.
    ///
    /// It's generally unsafe to call this function twice in a program if the
    /// previous invocation returned `Some`.
    ///
    /// Note, though, that on Windows it should be safe to call this function
    /// any number of times.
    pub unsafe fn from_env() -> Option<Client> {
        let var = match env::var("CARGO_MAKEFLAGS")
            .or_else(|_| env::var("MAKEFLAGS"))
            .or_else(|_| env::var("MFLAGS"))
        {
            Ok(s) => s,
            Err(_) => return None,
        };
        let mut arg = "--jobserver-fds=";
        let pos = match var.find(arg) {
            Some(i) => i,
            None => {
                arg = "--jobserver-auth=";
                match var.find(arg) {
                    Some(i) => i,
                    None => return None,
                }
            }
        };

        let s = var[pos + arg.len()..].split(' ').next().unwrap();
        imp::Client::open(s).map(|c| Client { inner: Arc::new(c) })
    }

    /// Acquires a token from this jobserver client.
    ///
    /// This function will block the calling thread until a new token can be
    /// acquired from the jobserver.
    ///
    /// # Return value
    ///
    /// On successful acquisition of a token an instance of `Acquired` is
    /// returned. This structure, when dropped, will release the token back to
    /// the jobserver. It's recommended to avoid leaking this value.
    ///
    /// # Errors
    ///
    /// If an I/O error happens while acquiring a token then this function will
    /// return immediately with the error. If an error is returned then a token
    /// was not acquired.
    pub fn acquire(&self) -> io::Result<Acquired> {
        let data = self.inner.acquire()?;
        Ok(Acquired {
            client: self.inner.clone(),
            data,
            disabled: false,
        })
    }

    /// Configures a child process to have access to this client's jobserver as
    /// well.
    ///
    /// This function is required to be called to ensure that a jobserver is
    /// properly inherited to a child process. If this function is *not* called
    /// then this `Client` will not be accessible in the child process. In other
    /// words, if not called, then `Client::from_env` will return `None` in the
    /// child process (or the equivalent of `Child::from_env` that `make` uses).
    ///
    /// ## Platform-specific behavior
    ///
    /// On Unix and Windows this will clobber the `CARGO_MAKEFLAGS` environment
    /// variables for the child process, and on Unix this will also allow the
    /// two file descriptors for this client to be inherited to the child.
    ///
    /// On platforms other than Unix and Windows this panics.
    pub fn configure(&self, cmd: &mut Command) {
        let arg = self.inner.string_arg();
        // Older implementations of make use `--jobserver-fds` and newer
        // implementations use `--jobserver-auth`, pass both to try to catch
        // both implementations.
        let value = format!("-j --jobserver-fds={0} --jobserver-auth={0}", arg);
        cmd.env("CARGO_MAKEFLAGS", &value);
        self.inner.configure(cmd);
    }

    /// Converts this `Client` into a helper thread to deal with a blocking
    /// `acquire` function a little more easily.
    ///
    /// The fact that the `acquire` function on `Client` blocks isn't always
    /// the easiest to work with. Typically you're using a jobserver to
    /// manage running other events in parallel! This means that you need to
    /// either (a) wait for an existing job to finish or (b) wait for a
    /// new token to become available.
    ///
    /// Unfortunately the blocking in `acquire` happens at the implementation
    /// layer of jobservers. On Unix this requires a blocking call to `read`
    /// and on Windows this requires one of the `WaitFor*` functions. Both
    /// of these situations aren't the easiest to deal with:
    ///
    /// * On Unix there's basically only one way to wake up a `read` early, and
    ///   that's through a signal. This is what the `make` implementation
    ///   itself uses, relying on `SIGCHLD` to wake up a blocking acquisition
    ///   of a new job token. Unfortunately nonblocking I/O is not an option
    ///   here, so it means that "waiting for one of two events" means that
    ///   the latter event must generate a signal! This is not always the case
    ///   on unix for all jobservers.
    ///
    /// * On Windows you'd have to basically use the `WaitForMultipleObjects`
    ///   which means that you've got to canonicalize all your event sources
    ///   into a `HANDLE` which also isn't the easiest thing to do
    ///   unfortunately.
    ///
    /// This function essentially attempts to ease these limitations by
    /// converting this `Client` into a helper thread spawned into this
    /// process. The application can then request that the helper thread
    /// acquires tokens and the provided closure will be invoked for each token
    /// acquired.
    ///
    /// The intention is that this function can be used to translate the event
    /// of a token acquisition into an arbitrary user-defined event.
    ///
    /// # Arguments
    ///
    /// This function will consume the `Client` provided to be transferred to
    /// the helper thread that is spawned. Additionally a closure `f` is
    /// provided to be invoked whenever a token is acquired.
    ///
    /// This closure is only invoked after calls to
    /// `HelperThread::request_token` have been made and a token itself has
    /// been acquired. If an error happens while acquiring the token then
    /// an error will be yielded to the closure as well.
    ///
    /// # Return Value
    ///
    /// This function will return an instance of the `HelperThread` structure
    /// which is used to manage the helper thread associated with this client.
    /// Through the `HelperThread` you'll request that tokens are acquired.
    /// When acquired, the closure provided here is invoked.
    ///
    /// When the `HelperThread` structure is returned it will be gracefully
    /// torn down, and the calling thread will be blocked until the thread is
    /// torn down (which should be prompt).
    ///
    /// # Errors
    ///
    /// This function may fail due to creation of the helper thread or
    /// auxiliary I/O objects to manage the helper thread. In any of these
    /// situations the error is propagated upwards.
    ///
    /// # Platform-specific behavior
    ///
    /// On Windows this function behaves pretty normally as expected, but on
    /// Unix the implementation is... a little heinous. As mentioned above
    /// we're forced into blocking I/O for token acquisition, namely a blocking
    /// call to `read`. We must be able to unblock this, however, to tear down
    /// the helper thread gracefully!
    ///
    /// Essentially what happens is that we'll send a signal to the helper
    /// thread spawned and rely on `EINTR` being returned to wake up the helper
    /// thread. This involves installing a global `SIGUSR1` handler that does
    /// nothing along with sending signals to that thread. This may cause
    /// odd behavior in some applications, so it's recommended to review and
    /// test thoroughly before using this.
    pub fn into_helper_thread<F>(self, f: F) -> io::Result<HelperThread>
    where
        F: FnMut(io::Result<Acquired>) + Send + 'static,
    {
        let state = Arc::new(HelperState::default());
        Ok(HelperThread {
            inner: Some(imp::spawn_helper(self, state.clone(), Box::new(f))?),
            state,
        })
    }

    /// Blocks the current thread until a token is acquired.
    ///
    /// This is the same as `acquire`, except that it doesn't return an RAII
    /// helper. If successful the process will need to guarantee that
    /// `release_raw` is called in the future.
    pub fn acquire_raw(&self) -> io::Result<()> {
        self.inner.acquire()?;
        Ok(())
    }

    /// Releases a jobserver token back to the original jobserver.
    ///
    /// This is intended to be paired with `acquire_raw` if it was called, but
    /// in some situations it could also be called to relinquish a process's
    /// implicit token temporarily which is then re-acquired later.
    pub fn release_raw(&self) -> io::Result<()> {
        self.inner.release(None)?;
        Ok(())
    }
}

impl Drop for Acquired {
    fn drop(&mut self) {
        if !self.disabled {
            drop(self.client.release(Some(&self.data)));
        }
    }
}

/// Structure returned from `Client::into_helper_thread` to manage the lifetime
/// of the helper thread returned, see those associated docs for more info.
#[derive(Debug)]
pub struct HelperThread {
    inner: Option<imp::Helper>,
    state: Arc<HelperState>,
}

impl HelperThread {
    /// Request that the helper thread acquires a token, eventually calling the
    /// original closure with a token when it's available.
    ///
    /// For more information, see the docs on that function.
    pub fn request_token(&self) {
        // Indicate that there's one more request for a token and then wake up
        // the helper thread if it's sleeping.
        self.state.lock().requests += 1;
        self.state.cvar.notify_one();
    }
}

impl Drop for HelperThread {
    fn drop(&mut self) {
        // Flag that the producer half is done so the helper thread should exit
        // quickly if it's waiting. Wake it up if it's actually waiting
        self.state.lock().producer_done = true;
        self.state.cvar.notify_one();

        // ... and afterwards perform any thread cleanup logic
        self.inner.take().unwrap().join();
    }
}

impl HelperState {
    fn lock(&self) -> MutexGuard<'_, HelperInner> {
        self.lock.lock().unwrap_or_else(|e| e.into_inner())
    }

    /// Executes `f` for each request for a token, where `f` is expected to
    /// block and then provide the original closure with a token once it's
    /// acquired.
    ///
    /// This is an infinite loop until the helper thread is dropped, at which
    /// point everything should get interrupted.
    fn for_each_request(&self, mut f: impl FnMut(&HelperState)) {
        let mut lock = self.lock();

        // We only execute while we could receive requests, but as soon as
        // that's `false` we're out of here.
        while !lock.producer_done {
            // If no one's requested a token then we wait for someone to
            // request a token.
            if lock.requests == 0 {
                lock = self.cvar.wait(lock).unwrap_or_else(|e| e.into_inner());
                continue;
            }

            // Consume the request for a token, and then actually acquire a
            // token after unlocking our lock (not that acquisition happens in
            // `f`). This ensures that we don't actually hold the lock if we
            // wait for a long time for a token.
            lock.requests -= 1;
            drop(lock);
            f(self);
            lock = self.lock();
        }
        lock.consumer_done = true;
        self.cvar.notify_one();
    }

    fn producer_done(&self) -> bool {
        self.lock().producer_done
    }
}

#[test]
fn no_helper_deadlock() {
    let x = crate::Client::new(32).unwrap();
    let _y = x.clone();
    std::mem::drop(x.into_helper_thread(|_| {}).unwrap());
}
