use crate::codec::RecvError;
use crate::frame::{self, Frame, Kind, Reason};
use crate::frame::{
    DEFAULT_MAX_FRAME_SIZE, DEFAULT_SETTINGS_HEADER_TABLE_SIZE, MAX_MAX_FRAME_SIZE,
};

use crate::hpack;

use futures_core::Stream;

use bytes::BytesMut;

use std::io;

use std::pin::Pin;
use std::task::{Context, Poll};
use tokio::io::AsyncRead;
use tokio_util::codec::FramedRead as InnerFramedRead;
use tokio_util::codec::{LengthDelimitedCodec, LengthDelimitedCodecError};

// 16 MB "sane default" taken from golang http2
const DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE: usize = 16 << 20;

#[derive(Debug)]
pub struct FramedRead<T> {
    inner: InnerFramedRead<T, LengthDelimitedCodec>,

    // hpack decoder state
    hpack: hpack::Decoder,

    max_header_list_size: usize,

    partial: Option<Partial>,
}

/// Partially loaded headers frame
#[derive(Debug)]
struct Partial {
    /// Empty frame
    frame: Continuable,

    /// Partial header payload
    buf: BytesMut,
}

#[derive(Debug)]
enum Continuable {
    Headers(frame::Headers),
    PushPromise(frame::PushPromise),
}

impl<T> FramedRead<T> {
    pub fn new(inner: InnerFramedRead<T, LengthDelimitedCodec>) -> FramedRead<T> {
        FramedRead {
            inner,
            hpack: hpack::Decoder::new(DEFAULT_SETTINGS_HEADER_TABLE_SIZE),
            max_header_list_size: DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE,
            partial: None,
        }
    }

    fn decode_frame(&mut self, mut bytes: BytesMut) -> Result<Option<Frame>, RecvError> {
        use self::RecvError::*;

        log::trace!("decoding frame from {}B", bytes.len());

        // Parse the head
        let head = frame::Head::parse(&bytes);

        if self.partial.is_some() && head.kind() != Kind::Continuation {
            proto_err!(conn: "expected CONTINUATION, got {:?}", head.kind());
            return Err(Connection(Reason::PROTOCOL_ERROR));
        }

        let kind = head.kind();

        log::trace!("    -> kind={:?}", kind);

        macro_rules! header_block {
            ($frame:ident, $head:ident, $bytes:ident) => ({
                // Drop the frame header
                // TODO: Change to drain: carllerche/bytes#130
                let _ = $bytes.split_to(frame::HEADER_LEN);

                // Parse the header frame w/o parsing the payload
                let (mut frame, mut payload) = match frame::$frame::load($head, $bytes) {
                    Ok(res) => res,
                    Err(frame::Error::InvalidDependencyId) => {
                        proto_err!(stream: "invalid HEADERS dependency ID");
                        // A stream cannot depend on itself. An endpoint MUST
                        // treat this as a stream error (Section 5.4.2) of type
                        // `PROTOCOL_ERROR`.
                        return Err(Stream {
                            id: $head.stream_id(),
                            reason: Reason::PROTOCOL_ERROR,
                        });
                    },
                    Err(e) => {
                        proto_err!(conn: "failed to load frame; err={:?}", e);
                        return Err(Connection(Reason::PROTOCOL_ERROR));
                    }
                };

                let is_end_headers = frame.is_end_headers();

                // Load the HPACK encoded headers
                match frame.load_hpack(&mut payload, self.max_header_list_size, &mut self.hpack) {
                    Ok(_) => {},
                    Err(frame::Error::Hpack(hpack::DecoderError::NeedMore(_))) if !is_end_headers => {},
                    Err(frame::Error::MalformedMessage) => {
                        let id = $head.stream_id();
                        proto_err!(stream: "malformed header block; stream={:?}", id);
                        return Err(Stream {
                            id,
                            reason: Reason::PROTOCOL_ERROR,
                        });
                    },
                    Err(e) => {
                        proto_err!(conn: "failed HPACK decoding; err={:?}", e);
                        return Err(Connection(Reason::PROTOCOL_ERROR));
                    }
                }

                if is_end_headers {
                    frame.into()
                } else {
                    log::trace!("loaded partial header block");
                    // Defer returning the frame
                    self.partial = Some(Partial {
                        frame: Continuable::$frame(frame),
                        buf: payload,
                    });

                    return Ok(None);
                }
            });
        }

        let frame = match kind {
            Kind::Settings => {
                let res = frame::Settings::load(head, &bytes[frame::HEADER_LEN..]);

                res.map_err(|e| {
                    proto_err!(conn: "failed to load SETTINGS frame; err={:?}", e);
                    Connection(Reason::PROTOCOL_ERROR)
                })?
                .into()
            }
            Kind::Ping => {
                let res = frame::Ping::load(head, &bytes[frame::HEADER_LEN..]);

                res.map_err(|e| {
                    proto_err!(conn: "failed to load PING frame; err={:?}", e);
                    Connection(Reason::PROTOCOL_ERROR)
                })?
                .into()
            }
            Kind::WindowUpdate => {
                let res = frame::WindowUpdate::load(head, &bytes[frame::HEADER_LEN..]);

                res.map_err(|e| {
                    proto_err!(conn: "failed to load WINDOW_UPDATE frame; err={:?}", e);
                    Connection(Reason::PROTOCOL_ERROR)
                })?
                .into()
            }
            Kind::Data => {
                let _ = bytes.split_to(frame::HEADER_LEN);
                let res = frame::Data::load(head, bytes.freeze());

                // TODO: Should this always be connection level? Probably not...
                res.map_err(|e| {
                    proto_err!(conn: "failed to load DATA frame; err={:?}", e);
                    Connection(Reason::PROTOCOL_ERROR)
                })?
                .into()
            }
            Kind::Headers => header_block!(Headers, head, bytes),
            Kind::Reset => {
                let res = frame::Reset::load(head, &bytes[frame::HEADER_LEN..]);
                res.map_err(|e| {
                    proto_err!(conn: "failed to load RESET frame; err={:?}", e);
                    Connection(Reason::PROTOCOL_ERROR)
                })?
                .into()
            }
            Kind::GoAway => {
                let res = frame::GoAway::load(&bytes[frame::HEADER_LEN..]);
                res.map_err(|e| {
                    proto_err!(conn: "failed to load GO_AWAY frame; err={:?}", e);
                    Connection(Reason::PROTOCOL_ERROR)
                })?
                .into()
            }
            Kind::PushPromise => header_block!(PushPromise, head, bytes),
            Kind::Priority => {
                if head.stream_id() == 0 {
                    // Invalid stream identifier
                    proto_err!(conn: "invalid stream ID 0");
                    return Err(Connection(Reason::PROTOCOL_ERROR));
                }

                match frame::Priority::load(head, &bytes[frame::HEADER_LEN..]) {
                    Ok(frame) => frame.into(),
                    Err(frame::Error::InvalidDependencyId) => {
                        // A stream cannot depend on itself. An endpoint MUST
                        // treat this as a stream error (Section 5.4.2) of type
                        // `PROTOCOL_ERROR`.
                        let id = head.stream_id();
                        proto_err!(stream: "PRIORITY invalid dependency ID; stream={:?}", id);
                        return Err(Stream {
                            id,
                            reason: Reason::PROTOCOL_ERROR,
                        });
                    }
                    Err(e) => {
                        proto_err!(conn: "failed to load PRIORITY frame; err={:?};", e);
                        return Err(Connection(Reason::PROTOCOL_ERROR));
                    }
                }
            }
            Kind::Continuation => {
                let is_end_headers = (head.flag() & 0x4) == 0x4;

                let mut partial = match self.partial.take() {
                    Some(partial) => partial,
                    None => {
                        proto_err!(conn: "received unexpected CONTINUATION frame");
                        return Err(Connection(Reason::PROTOCOL_ERROR));
                    }
                };

                // The stream identifiers must match
                if partial.frame.stream_id() != head.stream_id() {
                    proto_err!(conn: "CONTINUATION frame stream ID does not match previous frame stream ID");
                    return Err(Connection(Reason::PROTOCOL_ERROR));
                }

                // Extend the buf
                if partial.buf.is_empty() {
                    partial.buf = bytes.split_off(frame::HEADER_LEN);
                } else {
                    if partial.frame.is_over_size() {
                        // If there was left over bytes previously, they may be
                        // needed to continue decoding, even though we will
                        // be ignoring this frame. This is done to keep the HPACK
                        // decoder state up-to-date.
                        //
                        // Still, we need to be careful, because if a malicious
                        // attacker were to try to send a gigantic string, such
                        // that it fits over multiple header blocks, we could
                        // grow memory uncontrollably again, and that'd be a shame.
                        //
                        // Instead, we use a simple heuristic to determine if
                        // we should continue to ignore decoding, or to tell
                        // the attacker to go away.
                        if partial.buf.len() + bytes.len() > self.max_header_list_size {
                            proto_err!(conn: "CONTINUATION frame header block size over ignorable limit");
                            return Err(Connection(Reason::COMPRESSION_ERROR));
                        }
                    }
                    partial.buf.extend_from_slice(&bytes[frame::HEADER_LEN..]);
                }

                match partial.frame.load_hpack(
                    &mut partial.buf,
                    self.max_header_list_size,
                    &mut self.hpack,
                ) {
                    Ok(_) => {}
                    Err(frame::Error::Hpack(hpack::DecoderError::NeedMore(_)))
                        if !is_end_headers => {}
                    Err(frame::Error::MalformedMessage) => {
                        let id = head.stream_id();
                        proto_err!(stream: "malformed CONTINUATION frame; stream={:?}", id);
                        return Err(Stream {
                            id,
                            reason: Reason::PROTOCOL_ERROR,
                        });
                    }
                    Err(e) => {
                        proto_err!(conn: "failed HPACK decoding; err={:?}", e);
                        return Err(Connection(Reason::PROTOCOL_ERROR));
                    }
                }

                if is_end_headers {
                    partial.frame.into()
                } else {
                    self.partial = Some(partial);
                    return Ok(None);
                }
            }
            Kind::Unknown => {
                // Unknown frames are ignored
                return Ok(None);
            }
        };

        Ok(Some(frame))
    }

    pub fn get_ref(&self) -> &T {
        self.inner.get_ref()
    }

    pub fn get_mut(&mut self) -> &mut T {
        self.inner.get_mut()
    }

    /// Returns the current max frame size setting
    #[cfg(feature = "unstable")]
    #[inline]
    pub fn max_frame_size(&self) -> usize {
        self.inner.decoder().max_frame_length()
    }

    /// Updates the max frame size setting.
    ///
    /// Must be within 16,384 and 16,777,215.
    #[inline]
    pub fn set_max_frame_size(&mut self, val: usize) {
        assert!(DEFAULT_MAX_FRAME_SIZE as usize <= val && val <= MAX_MAX_FRAME_SIZE as usize);
        self.inner.decoder_mut().set_max_frame_length(val)
    }

    /// Update the max header list size setting.
    #[inline]
    pub fn set_max_header_list_size(&mut self, val: usize) {
        self.max_header_list_size = val;
    }
}

impl<T> Stream for FramedRead<T>
where
    T: AsyncRead + Unpin,
{
    type Item = Result<Frame, RecvError>;

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        loop {
            log::trace!("poll");
            let bytes = match ready!(Pin::new(&mut self.inner).poll_next(cx)) {
                Some(Ok(bytes)) => bytes,
                Some(Err(e)) => return Poll::Ready(Some(Err(map_err(e)))),
                None => return Poll::Ready(None),
            };

            log::trace!("poll; bytes={}B", bytes.len());
            if let Some(frame) = self.decode_frame(bytes)? {
                log::debug!("received; frame={:?}", frame);
                return Poll::Ready(Some(Ok(frame)));
            }
        }
    }
}

fn map_err(err: io::Error) -> RecvError {
    if let io::ErrorKind::InvalidData = err.kind() {
        if let Some(custom) = err.get_ref() {
            if custom.is::<LengthDelimitedCodecError>() {
                return RecvError::Connection(Reason::FRAME_SIZE_ERROR);
            }
        }
    }
    err.into()
}

// ===== impl Continuable =====

impl Continuable {
    fn stream_id(&self) -> frame::StreamId {
        match *self {
            Continuable::Headers(ref h) => h.stream_id(),
            Continuable::PushPromise(ref p) => p.stream_id(),
        }
    }

    fn is_over_size(&self) -> bool {
        match *self {
            Continuable::Headers(ref h) => h.is_over_size(),
            Continuable::PushPromise(ref p) => p.is_over_size(),
        }
    }

    fn load_hpack(
        &mut self,
        src: &mut BytesMut,
        max_header_list_size: usize,
        decoder: &mut hpack::Decoder,
    ) -> Result<(), frame::Error> {
        match *self {
            Continuable::Headers(ref mut h) => h.load_hpack(src, max_header_list_size, decoder),
            Continuable::PushPromise(ref mut p) => p.load_hpack(src, max_header_list_size, decoder),
        }
    }
}

impl<T> From<Continuable> for Frame<T> {
    fn from(cont: Continuable) -> Self {
        match cont {
            Continuable::Headers(mut headers) => {
                headers.set_end_headers();
                headers.into()
            }
            Continuable::PushPromise(mut push) => {
                push.set_end_headers();
                push.into()
            }
        }
    }
}
