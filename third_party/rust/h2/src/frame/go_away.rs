use frame::{self, Error, Head, Kind, Reason, StreamId};

use bytes::{BufMut};

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub struct GoAway {
    last_stream_id: StreamId,
    error_code: Reason,
}

impl GoAway {
    pub fn new(last_stream_id: StreamId, reason: Reason) -> Self {
        GoAway {
            last_stream_id,
            error_code: reason,
        }
    }

    pub fn last_stream_id(&self) -> StreamId {
        self.last_stream_id
    }

    pub fn reason(&self) -> Reason {
        self.error_code
    }

    pub fn load(payload: &[u8]) -> Result<GoAway, Error> {
        if payload.len() < 8 {
            return Err(Error::BadFrameSize);
        }

        let (last_stream_id, _) = StreamId::parse(&payload[..4]);
        let error_code = unpack_octets_4!(payload, 4, u32);

        Ok(GoAway {
            last_stream_id: last_stream_id,
            error_code: error_code.into(),
        })
    }

    pub fn encode<B: BufMut>(&self, dst: &mut B) {
        trace!("encoding GO_AWAY; code={:?}", self.error_code);
        let head = Head::new(Kind::GoAway, 0, StreamId::zero());
        head.encode(8, dst);
        dst.put_u32_be(self.last_stream_id.into());
        dst.put_u32_be(self.error_code.into());
    }
}

impl<B> From<GoAway> for frame::Frame<B> {
    fn from(src: GoAway) -> Self {
        frame::Frame::GoAway(src)
    }
}
