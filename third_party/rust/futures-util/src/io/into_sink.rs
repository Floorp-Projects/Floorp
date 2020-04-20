use futures_core::task::{Context, Poll};
use futures_io::AsyncWrite;
use futures_sink::Sink;
use std::io;
use std::pin::Pin;
use std::marker::Unpin;
use pin_utils::{unsafe_pinned, unsafe_unpinned};

#[derive(Debug)]
struct Block<Item> {
    offset: usize,
    bytes: Item,
}

/// Sink for the [`into_sink`](super::AsyncWriteExt::into_sink) method.
#[must_use = "sinks do nothing unless polled"]
#[derive(Debug)]
pub struct IntoSink<W, Item> {
    writer: W,
    /// An outstanding block for us to push into the underlying writer, along with an offset of how
    /// far into this block we have written already.
    buffer: Option<Block<Item>>,
}

impl<W: Unpin, Item> Unpin for IntoSink<W, Item> {}

impl<W: AsyncWrite, Item: AsRef<[u8]>> IntoSink<W, Item> {
    unsafe_pinned!(writer: W);
    unsafe_unpinned!(buffer: Option<Block<Item>>);

    pub(super) fn new(writer: W) -> Self {
        IntoSink { writer, buffer: None }
    }

    fn project(self: Pin<&mut Self>) -> (Pin<&mut W>, &mut Option<Block<Item>>) {
        unsafe {
            let this = self.get_unchecked_mut();
            (Pin::new_unchecked(&mut this.writer), &mut this.buffer)
        }
    }

    /// If we have an outstanding block in `buffer` attempt to push it into the writer, does _not_
    /// flush the writer after it succeeds in pushing the block into it.
    fn poll_flush_buffer(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), io::Error>>
    {
        let (mut writer, buffer) = self.project();
        if let Some(buffer) = buffer {
            loop {
                let bytes = buffer.bytes.as_ref();
                let written = ready!(writer.as_mut().poll_write(cx, &bytes[buffer.offset..]))?;
                buffer.offset += written;
                if buffer.offset == bytes.len() {
                    break;
                }
            }
        }
        *buffer = None;
        Poll::Ready(Ok(()))
    }

}

impl<W: AsyncWrite, Item: AsRef<[u8]>> Sink<Item> for IntoSink<W, Item> {
    type Error = io::Error;

    fn poll_ready(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), Self::Error>>
    {
        ready!(self.as_mut().poll_flush_buffer(cx))?;
        Poll::Ready(Ok(()))
    }

    #[allow(clippy::debug_assert_with_mut_call)]
    fn start_send(
        mut self: Pin<&mut Self>,
        item: Item,
    ) -> Result<(), Self::Error>
    {
        debug_assert!(self.as_mut().buffer().is_none());
        *self.as_mut().buffer() = Some(Block { offset: 0, bytes: item });
        Ok(())
    }

    fn poll_flush(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), Self::Error>>
    {
        ready!(self.as_mut().poll_flush_buffer(cx))?;
        ready!(self.as_mut().writer().poll_flush(cx))?;
        Poll::Ready(Ok(()))
    }

    fn poll_close(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Result<(), Self::Error>>
    {
        ready!(self.as_mut().poll_flush_buffer(cx))?;
        ready!(self.as_mut().writer().poll_close(cx))?;
        Poll::Ready(Ok(()))
    }
}
