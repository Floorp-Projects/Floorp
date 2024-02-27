use std::{
    ffi::OsStr,
    fs::{self, File},
    io::{self, Read, Write},
    mem::ManuallyDrop,
    os::{raw::c_int, unix::prelude::*},
    path::Path,
};

use crate::parallel::stderr::{set_blocking, set_non_blocking};

pub(super) struct JobServerClient {
    read: File,
    write: Option<File>,
}

impl JobServerClient {
    pub(super) unsafe fn open(var: &[u8]) -> Option<Self> {
        if let Some(fifo) = var.strip_prefix(b"fifo:") {
            Self::from_fifo(Path::new(OsStr::from_bytes(fifo)))
        } else {
            Self::from_pipe(OsStr::from_bytes(var).to_str()?)
        }
    }

    /// `--jobserver-auth=fifo:PATH`
    fn from_fifo(path: &Path) -> Option<Self> {
        let file = fs::OpenOptions::new()
            .read(true)
            .write(true)
            .open(path)
            .ok()?;

        if is_pipe(&file)? {
            // File in Rust is always closed-on-exec as long as it's opened by
            // `File::open` or `fs::OpenOptions::open`.
            set_non_blocking(&file).ok()?;

            Some(Self {
                read: file,
                write: None,
            })
        } else {
            None
        }
    }

    /// `--jobserver-auth=fd-for-R,fd-for-W`
    unsafe fn from_pipe(s: &str) -> Option<Self> {
        let (read, write) = s.split_once(',')?;

        let read = read.parse().ok()?;
        let write = write.parse().ok()?;

        let read = ManuallyDrop::new(File::from_raw_fd(read));
        let write = ManuallyDrop::new(File::from_raw_fd(write));

        // Ok so we've got two integers that look like file descriptors, but
        // for extra sanity checking let's see if they actually look like
        // instances of a pipe before we return the client.
        //
        // If we're called from `make` *without* the leading + on our rule
        // then we'll have `MAKEFLAGS` env vars but won't actually have
        // access to the file descriptors.
        match (
            is_pipe(&read),
            is_pipe(&write),
            get_access_mode(&read),
            get_access_mode(&write),
        ) {
            (
                Some(true),
                Some(true),
                Some(libc::O_RDONLY) | Some(libc::O_RDWR),
                Some(libc::O_WRONLY) | Some(libc::O_RDWR),
            ) => {
                // Optimization: Try converting it to a fifo by using /dev/fd
                if let Some(jobserver) =
                    Self::from_fifo(Path::new(&format!("/dev/fd/{}", read.as_raw_fd())))
                {
                    return Some(jobserver);
                }

                let read = read.try_clone().ok()?;
                let write = write.try_clone().ok()?;

                Some(Self {
                    read,
                    write: Some(write),
                })
            }
            _ => None,
        }
    }

    pub(super) fn prepare_for_acquires(&self) -> Result<(), crate::Error> {
        if let Some(write) = self.write.as_ref() {
            set_non_blocking(&self.read)?;
            set_non_blocking(write)?;
        }

        Ok(())
    }

    pub(super) fn done_acquires(&self) {
        if let Some(write) = self.write.as_ref() {
            let _ = set_blocking(&self.read);
            let _ = set_blocking(write);
        }
    }

    /// Must call `prepare_for_acquire` before using it.
    pub(super) fn try_acquire(&self) -> io::Result<Option<()>> {
        let mut fds = [libc::pollfd {
            fd: self.read.as_raw_fd(),
            events: libc::POLLIN,
            revents: 0,
        }];

        let ret = cvt(unsafe { libc::poll(fds.as_mut_ptr(), 1, 0) })?;
        if ret == 1 {
            let mut buf = [0];
            match (&self.read).read(&mut buf) {
                Ok(1) => Ok(Some(())),
                Ok(_) => Ok(None), // 0, eof
                Err(e)
                    if e.kind() == io::ErrorKind::Interrupted
                        || e.kind() == io::ErrorKind::WouldBlock =>
                {
                    Ok(None)
                }
                Err(e) => Err(e),
            }
        } else {
            Ok(None)
        }
    }

    pub(super) fn release(&self) -> io::Result<()> {
        // For write to block, this would mean that pipe is full.
        // If all every release are pair with an acquire, then this cannot
        // happen.
        //
        // If it does happen, it is likely a bug in the program using this
        // crate or some other programs that use the same jobserver have a
        // bug in their code.
        //
        // If that turns out to not be the case we'll get an error anyway!
        let mut write = self.write.as_ref().unwrap_or(&self.read);
        match write.write(&[b'+'])? {
            1 => Ok(()),
            _ => Err(io::Error::from(io::ErrorKind::UnexpectedEof)),
        }
    }
}

fn cvt(t: c_int) -> io::Result<c_int> {
    if t == -1 {
        Err(io::Error::last_os_error())
    } else {
        Ok(t)
    }
}

fn is_pipe(file: &File) -> Option<bool> {
    Some(file.metadata().ok()?.file_type().is_fifo())
}

fn get_access_mode(file: &File) -> Option<c_int> {
    let ret = unsafe { libc::fcntl(file.as_raw_fd(), libc::F_GETFL) };
    if ret == -1 {
        return None;
    }

    Some(ret & libc::O_ACCMODE)
}
