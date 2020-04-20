use crate::lock::BiLock;
use futures_core::task::{Context, Poll};
use futures_io::{AsyncRead, AsyncWrite, IoSlice, IoSliceMut};
use std::io;
use std::pin::Pin;

/// The readable half of an object returned from `AsyncRead::split`.
#[derive(Debug)]
pub struct ReadHalf<T> {
    handle: BiLock<T>,
}

/// The writable half of an object returned from `AsyncRead::split`.
#[derive(Debug)]
pub struct WriteHalf<T> {
    handle: BiLock<T>,
}

fn lock_and_then<T, U, E, F>(
    lock: &BiLock<T>,
    cx: &mut Context<'_>,
    f: F
) -> Poll<Result<U, E>>
    where F: FnOnce(Pin<&mut T>, &mut Context<'_>) -> Poll<Result<U, E>>
{
    let mut l = ready!(lock.poll_lock(cx));
    f(l.as_pin_mut(), cx)
}

pub(super) fn split<T: AsyncRead + AsyncWrite>(t: T) -> (ReadHalf<T>, WriteHalf<T>) {
    let (a, b) = BiLock::new(t);
    (ReadHalf { handle: a }, WriteHalf { handle: b })
}

impl<R: AsyncRead> AsyncRead for ReadHalf<R> {
    fn poll_read(self: Pin<&mut Self>, cx: &mut Context<'_>, buf: &mut [u8])
        -> Poll<io::Result<usize>>
    {
        lock_and_then(&self.handle, cx, |l, cx| l.poll_read(cx, buf))
    }

    fn poll_read_vectored(self: Pin<&mut Self>, cx: &mut Context<'_>, bufs: &mut [IoSliceMut<'_>])
        -> Poll<io::Result<usize>>
    {
        lock_and_then(&self.handle, cx, |l, cx| l.poll_read_vectored(cx, bufs))
    }
}

impl<W: AsyncWrite> AsyncWrite for WriteHalf<W> {
    fn poll_write(self: Pin<&mut Self>, cx: &mut Context<'_>, buf: &[u8])
        -> Poll<io::Result<usize>>
    {
        lock_and_then(&self.handle, cx, |l, cx| l.poll_write(cx, buf))
    }

    fn poll_write_vectored(self: Pin<&mut Self>, cx: &mut Context<'_>, bufs: &[IoSlice<'_>])
        -> Poll<io::Result<usize>>
    {
        lock_and_then(&self.handle, cx, |l, cx| l.poll_write_vectored(cx, bufs))
    }

    fn poll_flush(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<io::Result<()>> {
        lock_and_then(&self.handle, cx, |l, cx| l.poll_flush(cx))
    }

    fn poll_close(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<io::Result<()>> {
        lock_and_then(&self.handle, cx, |l, cx| l.poll_close(cx))
    }
}
