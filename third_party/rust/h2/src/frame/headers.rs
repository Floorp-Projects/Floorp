use super::{StreamDependency, StreamId};
use frame::{Error, Frame, Head, Kind};
use hpack;

use http::{uri, HeaderMap, Method, StatusCode, Uri};
use http::header::{self, HeaderName, HeaderValue};

use byteorder::{BigEndian, ByteOrder};
use bytes::{Bytes, BytesMut};
use string::String;

use std::fmt;
use std::io::Cursor;

/// Header frame
///
/// This could be either a request or a response.
#[derive(Eq, PartialEq)]
pub struct Headers {
    /// The ID of the stream with which this frame is associated.
    stream_id: StreamId,

    /// The stream dependency information, if any.
    stream_dep: Option<StreamDependency>,

    /// The header block fragment
    header_block: HeaderBlock,

    /// The associated flags
    flags: HeadersFlag,
}

#[derive(Copy, Clone, Eq, PartialEq)]
pub struct HeadersFlag(u8);

#[derive(Eq, PartialEq)]
pub struct PushPromise {
    /// The ID of the stream with which this frame is associated.
    stream_id: StreamId,

    /// The ID of the stream being reserved by this PushPromise.
    promised_id: StreamId,

    /// The header block fragment
    header_block: HeaderBlock,

    /// The associated flags
    flags: PushPromiseFlag,
}

#[derive(Copy, Clone, Eq, PartialEq)]
pub struct PushPromiseFlag(u8);

#[derive(Debug)]
pub struct Continuation {
    /// Stream ID of continuation frame
    stream_id: StreamId,

    header_block: EncodingHeaderBlock,
}

// TODO: These fields shouldn't be `pub`
#[derive(Debug, Default, Eq, PartialEq)]
pub struct Pseudo {
    // Request
    pub method: Option<Method>,
    pub scheme: Option<String<Bytes>>,
    pub authority: Option<String<Bytes>>,
    pub path: Option<String<Bytes>>,

    // Response
    pub status: Option<StatusCode>,
}

#[derive(Debug)]
pub struct Iter {
    /// Pseudo headers
    pseudo: Option<Pseudo>,

    /// Header fields
    fields: header::IntoIter<HeaderValue>,
}

#[derive(Debug, PartialEq, Eq)]
struct HeaderBlock {
    /// The decoded header fields
    fields: HeaderMap,

    /// Set to true if decoding went over the max header list size.
    is_over_size: bool,

    /// Pseudo headers, these are broken out as they must be sent as part of the
    /// headers frame.
    pseudo: Pseudo,
}

#[derive(Debug)]
struct EncodingHeaderBlock {
    /// Argument to pass to the HPACK encoder to resume encoding
    hpack: Option<hpack::EncodeState>,

    /// remaining headers to encode
    headers: Iter,
}

const END_STREAM: u8 = 0x1;
const END_HEADERS: u8 = 0x4;
const PADDED: u8 = 0x8;
const PRIORITY: u8 = 0x20;
const ALL: u8 = END_STREAM | END_HEADERS | PADDED | PRIORITY;

// ===== impl Headers =====

impl Headers {
    /// Create a new HEADERS frame
    pub fn new(stream_id: StreamId, pseudo: Pseudo, fields: HeaderMap) -> Self {
        Headers {
            stream_id: stream_id,
            stream_dep: None,
            header_block: HeaderBlock {
                fields: fields,
                is_over_size: false,
                pseudo: pseudo,
            },
            flags: HeadersFlag::default(),
        }
    }

    pub fn trailers(stream_id: StreamId, fields: HeaderMap) -> Self {
        let mut flags = HeadersFlag::default();
        flags.set_end_stream();

        Headers {
            stream_id,
            stream_dep: None,
            header_block: HeaderBlock {
                fields: fields,
                is_over_size: false,
                pseudo: Pseudo::default(),
            },
            flags: flags,
        }
    }

    /// Loads the header frame but doesn't actually do HPACK decoding.
    ///
    /// HPACK decoding is done in the `load_hpack` step.
    pub fn load(head: Head, mut src: BytesMut) -> Result<(Self, BytesMut), Error> {
        let flags = HeadersFlag(head.flag());
        let mut pad = 0;

        trace!("loading headers; flags={:?}", flags);

        // Read the padding length
        if flags.is_padded() {
            if src.len() < 1 {
                return Err(Error::MalformedMessage);
            }
            pad = src[0] as usize;

            // Drop the padding
            let _ = src.split_to(1);
        }

        // Read the stream dependency
        let stream_dep = if flags.is_priority() {
            if src.len() < 5 {
                return Err(Error::MalformedMessage);
            }
            let stream_dep = StreamDependency::load(&src[..5])?;

            if stream_dep.dependency_id() == head.stream_id() {
                return Err(Error::InvalidDependencyId);
            }

            // Drop the next 5 bytes
            let _ = src.split_to(5);

            Some(stream_dep)
        } else {
            None
        };

        if pad > 0 {
            if pad > src.len() {
                return Err(Error::TooMuchPadding);
            }

            let len = src.len() - pad;
            src.truncate(len);
        }

        let headers = Headers {
            stream_id: head.stream_id(),
            stream_dep: stream_dep,
            header_block: HeaderBlock {
                fields: HeaderMap::new(),
                is_over_size: false,
                pseudo: Pseudo::default(),
            },
            flags: flags,
        };

        Ok((headers, src))
    }

    pub fn load_hpack(&mut self, src: &mut BytesMut, max_header_list_size: usize, decoder: &mut hpack::Decoder) -> Result<(), Error> {
        self.header_block.load(src, max_header_list_size, decoder)
    }

    pub fn stream_id(&self) -> StreamId {
        self.stream_id
    }

    pub fn is_end_headers(&self) -> bool {
        self.flags.is_end_headers()
    }

    pub fn set_end_headers(&mut self) {
        self.flags.set_end_headers();
    }

    pub fn is_end_stream(&self) -> bool {
        self.flags.is_end_stream()
    }

    pub fn set_end_stream(&mut self) {
        self.flags.set_end_stream()
    }

    pub fn is_over_size(&self) -> bool {
        self.header_block.is_over_size
    }

    pub fn into_parts(self) -> (Pseudo, HeaderMap) {
        (self.header_block.pseudo, self.header_block.fields)
    }

    #[cfg(feature = "unstable")]
    pub fn pseudo_mut(&mut self) -> &mut Pseudo {
        &mut self.header_block.pseudo
    }

    pub fn fields(&self) -> &HeaderMap {
        &self.header_block.fields
    }

    pub fn into_fields(self) -> HeaderMap {
        self.header_block.fields
    }

    pub fn encode(self, encoder: &mut hpack::Encoder, dst: &mut BytesMut) -> Option<Continuation> {
        // At this point, the `is_end_headers` flag should always be set
        debug_assert!(self.flags.is_end_headers());

        // Get the HEADERS frame head
        let head = self.head();

        self.header_block.into_encoding()
            .encode(&head, encoder, dst, |_| {
            })
    }

    fn head(&self) -> Head {
        Head::new(Kind::Headers, self.flags.into(), self.stream_id)
    }
}

impl<T> From<Headers> for Frame<T> {
    fn from(src: Headers) -> Self {
        Frame::Headers(src)
    }
}

impl fmt::Debug for Headers {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Headers")
            .field("stream_id", &self.stream_id)
            .field("stream_dep", &self.stream_dep)
            .field("flags", &self.flags)
            // `fields` and `pseudo` purposefully not included
            .finish()
    }
}

// ===== impl PushPromise =====

impl PushPromise {
    /// Loads the push promise frame but doesn't actually do HPACK decoding.
    ///
    /// HPACK decoding is done in the `load_hpack` step.
    pub fn load(head: Head, mut src: BytesMut) -> Result<(Self, BytesMut), Error> {
        let flags = PushPromiseFlag(head.flag());
        let mut pad = 0;

        // Read the padding length
        if flags.is_padded() {
            if src.len() < 1 {
                return Err(Error::MalformedMessage);
            }

            // TODO: Ensure payload is sized correctly
            pad = src[0] as usize;

            // Drop the padding
            let _ = src.split_to(1);
        }

        if src.len() < 5 {
            return Err(Error::MalformedMessage);
        }

        let (promised_id, _) = StreamId::parse(&src[..4]);
        // Drop promised_id bytes
        let _ = src.split_to(5);

        if pad > 0 {
            if pad > src.len() {
                return Err(Error::TooMuchPadding);
            }

            let len = src.len() - pad;
            src.truncate(len);
        }

        let frame = PushPromise {
            flags: flags,
            header_block: HeaderBlock {
                fields: HeaderMap::new(),
                is_over_size: false,
                pseudo: Pseudo::default(),
            },
            promised_id: promised_id,
            stream_id: head.stream_id(),
        };
        Ok((frame, src))
    }

    pub fn load_hpack(&mut self, src: &mut BytesMut, max_header_list_size: usize, decoder: &mut hpack::Decoder) -> Result<(), Error> {
        self.header_block.load(src, max_header_list_size, decoder)
    }

    pub fn stream_id(&self) -> StreamId {
        self.stream_id
    }

    pub fn promised_id(&self) -> StreamId {
        self.promised_id
    }

    pub fn is_end_headers(&self) -> bool {
        self.flags.is_end_headers()
    }

    pub fn set_end_headers(&mut self) {
        self.flags.set_end_headers();
    }

    pub fn is_over_size(&self) -> bool {
        self.header_block.is_over_size
    }

    pub fn encode(self, encoder: &mut hpack::Encoder, dst: &mut BytesMut) -> Option<Continuation> {
        use bytes::BufMut;

        // At this point, the `is_end_headers` flag should always be set
        debug_assert!(self.flags.is_end_headers());

        let head = self.head();
        let promised_id = self.promised_id;

        self.header_block.into_encoding()
            .encode(&head, encoder, dst, |dst| {
                dst.put_u32_be(promised_id.into());
            })
    }

    fn head(&self) -> Head {
        Head::new(Kind::PushPromise, self.flags.into(), self.stream_id)
    }
}

#[cfg(feature = "unstable")]
impl PushPromise {
    pub fn new(
        stream_id: StreamId,
        promised_id: StreamId,
        pseudo: Pseudo,
        fields: HeaderMap,
    ) -> Self {
        PushPromise {
            flags: PushPromiseFlag::default(),
            header_block: HeaderBlock {
                fields,
                is_over_size: false,
                pseudo,
            },
            promised_id,
            stream_id,
        }
    }

    pub fn into_parts(self) -> (Pseudo, HeaderMap) {
        (self.header_block.pseudo, self.header_block.fields)
    }

    pub fn fields(&self) -> &HeaderMap {
        &self.header_block.fields
    }

    pub fn into_fields(self) -> HeaderMap {
        self.header_block.fields
    }
}

impl<T> From<PushPromise> for Frame<T> {
    fn from(src: PushPromise) -> Self {
        Frame::PushPromise(src)
    }
}

impl fmt::Debug for PushPromise {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("PushPromise")
            .field("stream_id", &self.stream_id)
            .field("promised_id", &self.promised_id)
            .field("flags", &self.flags)
            // `fields` and `pseudo` purposefully not included
            .finish()
    }
}

// ===== impl Continuation =====

impl Continuation {
    fn head(&self) -> Head {
        Head::new(Kind::Continuation, END_HEADERS, self.stream_id)
    }

    pub fn encode(self, encoder: &mut hpack::Encoder, dst: &mut BytesMut) -> Option<Continuation> {
        // Get the CONTINUATION frame head
        let head = self.head();

        self.header_block
            .encode(&head, encoder, dst, |_| {
            })
    }
}

// ===== impl Pseudo =====

impl Pseudo {
    pub fn request(method: Method, uri: Uri) -> Self {
        let parts = uri::Parts::from(uri);

        let mut path = parts
            .path_and_query
            .map(|v| v.into())
            .unwrap_or_else(|| Bytes::new());

        if path.is_empty() && method != Method::OPTIONS {
            path = Bytes::from_static(b"/");
        }

        let mut pseudo = Pseudo {
            method: Some(method),
            scheme: None,
            authority: None,
            path: Some(to_string(path)),
            status: None,
        };

        // If the URI includes a scheme component, add it to the pseudo headers
        //
        // TODO: Scheme must be set...
        if let Some(scheme) = parts.scheme {
            pseudo.set_scheme(scheme);
        }

        // If the URI includes an authority component, add it to the pseudo
        // headers
        if let Some(authority) = parts.authority {
            pseudo.set_authority(to_string(authority.into()));
        }

        pseudo
    }

    pub fn response(status: StatusCode) -> Self {
        Pseudo {
            method: None,
            scheme: None,
            authority: None,
            path: None,
            status: Some(status),
        }
    }

    pub fn set_scheme(&mut self, scheme: uri::Scheme) {
        self.scheme = Some(to_string(scheme.into()));
    }

    pub fn set_authority(&mut self, authority: String<Bytes>) {
        self.authority = Some(authority);
    }
}

fn to_string(src: Bytes) -> String<Bytes> {
    unsafe { String::from_utf8_unchecked(src) }
}

// ===== impl EncodingHeaderBlock =====

impl EncodingHeaderBlock {
    fn encode<F>(mut self,
                 head: &Head,
                 encoder: &mut hpack::Encoder,
                 dst: &mut BytesMut,
                 f: F)
        -> Option<Continuation>
    where F: FnOnce(&mut BytesMut),
    {
        let head_pos = dst.len();

        // At this point, we don't know how big the h2 frame will be.
        // So, we write the head with length 0, then write the body, and
        // finally write the length once we know the size.
        head.encode(0, dst);

        let payload_pos = dst.len();

        f(dst);

        // Now, encode the header payload
        let continuation = match encoder.encode(self.hpack, &mut self.headers, dst) {
            hpack::Encode::Full => None,
            hpack::Encode::Partial(state) => Some(Continuation {
                stream_id: head.stream_id(),
                header_block: EncodingHeaderBlock {
                    hpack: Some(state),
                    headers: self.headers,
                },
            }),
        };

        // Compute the header block length
        let payload_len = (dst.len() - payload_pos) as u64;

        // Write the frame length
        BigEndian::write_uint(&mut dst[head_pos..head_pos + 3], payload_len, 3);

        if continuation.is_some() {
            // There will be continuation frames, so the `is_end_headers` flag
            // must be unset
            debug_assert!(dst[head_pos + 4] & END_HEADERS == END_HEADERS);

            dst[head_pos + 4] -= END_HEADERS;
        }

        continuation
    }
}

// ===== impl Iter =====

impl Iterator for Iter {
    type Item = hpack::Header<Option<HeaderName>>;

    fn next(&mut self) -> Option<Self::Item> {
        use hpack::Header::*;

        if let Some(ref mut pseudo) = self.pseudo {
            if let Some(method) = pseudo.method.take() {
                return Some(Method(method));
            }

            if let Some(scheme) = pseudo.scheme.take() {
                return Some(Scheme(scheme));
            }

            if let Some(authority) = pseudo.authority.take() {
                return Some(Authority(authority));
            }

            if let Some(path) = pseudo.path.take() {
                return Some(Path(path));
            }

            if let Some(status) = pseudo.status.take() {
                return Some(Status(status));
            }
        }

        self.pseudo = None;

        self.fields.next().map(|(name, value)| {
            Field {
                name: name,
                value: value,
            }
        })
    }
}

// ===== impl HeadersFlag =====

impl HeadersFlag {
    pub fn empty() -> HeadersFlag {
        HeadersFlag(0)
    }

    pub fn load(bits: u8) -> HeadersFlag {
        HeadersFlag(bits & ALL)
    }

    pub fn is_end_stream(&self) -> bool {
        self.0 & END_STREAM == END_STREAM
    }

    pub fn set_end_stream(&mut self) {
        self.0 |= END_STREAM;
    }

    pub fn is_end_headers(&self) -> bool {
        self.0 & END_HEADERS == END_HEADERS
    }

    pub fn set_end_headers(&mut self) {
        self.0 |= END_HEADERS;
    }

    pub fn is_padded(&self) -> bool {
        self.0 & PADDED == PADDED
    }

    pub fn is_priority(&self) -> bool {
        self.0 & PRIORITY == PRIORITY
    }
}

impl Default for HeadersFlag {
    /// Returns a `HeadersFlag` value with `END_HEADERS` set.
    fn default() -> Self {
        HeadersFlag(END_HEADERS)
    }
}

impl From<HeadersFlag> for u8 {
    fn from(src: HeadersFlag) -> u8 {
        src.0
    }
}

impl fmt::Debug for HeadersFlag {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("HeadersFlag")
            .field("end_stream", &self.is_end_stream())
            .field("end_headers", &self.is_end_headers())
            .field("padded", &self.is_padded())
            .field("priority", &self.is_priority())
            .finish()
    }
}

// ===== impl PushPromiseFlag =====

impl PushPromiseFlag {
    pub fn empty() -> PushPromiseFlag {
        PushPromiseFlag(0)
    }

    pub fn load(bits: u8) -> PushPromiseFlag {
        PushPromiseFlag(bits & ALL)
    }

    pub fn is_end_headers(&self) -> bool {
        self.0 & END_HEADERS == END_HEADERS
    }

    pub fn set_end_headers(&mut self) {
        self.0 |= END_HEADERS;
    }

    pub fn is_padded(&self) -> bool {
        self.0 & PADDED == PADDED
    }
}

impl Default for PushPromiseFlag {
    /// Returns a `PushPromiseFlag` value with `END_HEADERS` set.
    fn default() -> Self {
        PushPromiseFlag(END_HEADERS)
    }
}

impl From<PushPromiseFlag> for u8 {
    fn from(src: PushPromiseFlag) -> u8 {
        src.0
    }
}

impl fmt::Debug for PushPromiseFlag {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("PushPromiseFlag")
            .field("end_headers", &self.is_end_headers())
            .field("padded", &self.is_padded())
            .finish()
    }
}

// ===== HeaderBlock =====


impl HeaderBlock {
    fn load(&mut self, src: &mut BytesMut, max_header_list_size: usize, decoder: &mut hpack::Decoder) -> Result<(), Error> {
        let mut reg = !self.fields.is_empty();
        let mut malformed = false;
        let mut headers_size = self.calculate_header_list_size();

        macro_rules! set_pseudo {
            ($field:ident, $val:expr) => {{
                if reg {
                    trace!("load_hpack; header malformed -- pseudo not at head of block");
                    malformed = true;
                } else if self.pseudo.$field.is_some() {
                    trace!("load_hpack; header malformed -- repeated pseudo");
                    malformed = true;
                } else {
                    let __val = $val;
                    headers_size += decoded_header_size(stringify!($ident).len() + 1, __val.as_str().len());
                    if headers_size < max_header_list_size {
                        self.pseudo.$field = Some(__val);
                    } else if !self.is_over_size {
                        trace!("load_hpack; header list size over max");
                        self.is_over_size = true;
                    }
                }
            }}
        }

        let mut cursor = Cursor::new(src);

        // If the header frame is malformed, we still have to continue decoding
        // the headers. A malformed header frame is a stream level error, but
        // the hpack state is connection level. In order to maintain correct
        // state for other streams, the hpack decoding process must complete.
        let res = decoder.decode(&mut cursor, |header| {
            use hpack::Header::*;

            match header {
                Field {
                    name,
                    value,
                } => {
                    // Connection level header fields are not supported and must
                    // result in a protocol error.

                    if name == header::CONNECTION
                        || name == header::TRANSFER_ENCODING
                        || name == header::UPGRADE
                        || name == "keep-alive"
                        || name == "proxy-connection"
                    {
                        trace!("load_hpack; connection level header");
                        malformed = true;
                    } else if name == header::TE && value != "trailers" {
                        trace!("load_hpack; TE header not set to trailers; val={:?}", value);
                        malformed = true;
                    } else {
                        reg = true;

                        headers_size += decoded_header_size(name.as_str().len(), value.len());
                        if headers_size < max_header_list_size {
                            self.fields.append(name, value);
                        } else if !self.is_over_size {
                            trace!("load_hpack; header list size over max");
                            self.is_over_size = true;
                        }
                    }
                },
                Authority(v) => set_pseudo!(authority, v),
                Method(v) => set_pseudo!(method, v),
                Scheme(v) => set_pseudo!(scheme, v),
                Path(v) => set_pseudo!(path, v),
                Status(v) => set_pseudo!(status, v),
            }
        });

        if let Err(e) = res {
            trace!("hpack decoding error; err={:?}", e);
            return Err(e.into());
        }

        if malformed {
            trace!("malformed message");
            return Err(Error::MalformedMessage.into());
        }

        Ok(())
    }

    fn into_encoding(self) -> EncodingHeaderBlock {
        EncodingHeaderBlock {
            hpack: None,
            headers: Iter {
                pseudo: Some(self.pseudo),
                fields: self.fields.into_iter(),
            },
        }
    }

    /// Calculates the size of the currently decoded header list.
    ///
    /// According to http://httpwg.org/specs/rfc7540.html#SETTINGS_MAX_HEADER_LIST_SIZE
    ///
    /// > The value is based on the uncompressed size of header fields,
    /// > including the length of the name and value in octets plus an
    /// > overhead of 32 octets for each header field.
    fn calculate_header_list_size(&self) -> usize {
        macro_rules! pseudo_size {
            ($name:ident) => ({
                self.pseudo
                    .$name
                    .as_ref()
                    .map(|m| decoded_header_size(stringify!($name).len() + 1, m.as_str().len()))
                    .unwrap_or(0)
            });
        }

        pseudo_size!(method) +
        pseudo_size!(scheme) +
        pseudo_size!(status) +
        pseudo_size!(authority) +
        pseudo_size!(path) +
        self.fields.iter()
            .map(|(name, value)| decoded_header_size(name.as_str().len(), value.len()))
            .sum::<usize>()
    }
}

fn decoded_header_size(name: usize, value: usize) -> usize {
    name + value + 32
}

// Stupid hack to make the set_pseudo! macro happy, since all other values
// have a method `as_str` except for `String<Bytes>`.
trait AsStr {
    fn as_str(&self) -> &str;
}

impl AsStr for String<Bytes> {
    fn as_str(&self) -> &str {
        self
    }
}
