use alloc::boxed::Box;
use core::fmt::Debug;
use core::usize;

use bytes::{Buf, BufMut};

use crate::encoding::{
    decode_key, encode_varint, encoded_len_varint, message, DecodeContext, WireType,
};
use crate::DecodeError;
use crate::EncodeError;

/// A Protocol Buffers message.
pub trait Message: Debug + Send + Sync {
    /// Encodes the message to a buffer.
    ///
    /// This method will panic if the buffer has insufficient capacity.
    ///
    /// Meant to be used only by `Message` implementations.
    #[doc(hidden)]
    fn encode_raw<B>(&self, buf: &mut B)
    where
        B: BufMut,
        Self: Sized;

    /// Decodes a field from a buffer, and merges it into `self`.
    ///
    /// Meant to be used only by `Message` implementations.
    #[doc(hidden)]
    fn merge_field<B>(
        &mut self,
        tag: u32,
        wire_type: WireType,
        buf: &mut B,
        ctx: DecodeContext,
    ) -> Result<(), DecodeError>
    where
        B: Buf,
        Self: Sized;

    /// Returns the encoded length of the message without a length delimiter.
    fn encoded_len(&self) -> usize;

    /// Encodes the message to a buffer.
    ///
    /// An error will be returned if the buffer does not have sufficient capacity.
    fn encode<B>(&self, buf: &mut B) -> Result<(), EncodeError>
    where
        B: BufMut,
        Self: Sized,
    {
        let required = self.encoded_len();
        let remaining = buf.remaining_mut();
        if required > buf.remaining_mut() {
            return Err(EncodeError::new(required, remaining));
        }

        self.encode_raw(buf);
        Ok(())
    }

    #[cfg(feature = "std")]
    /// Encodes the message to a newly allocated buffer.
    fn encode_to_vec(&self) -> Vec<u8>
    where
        Self: Sized,
    {
        let mut buf = Vec::with_capacity(self.encoded_len());

        self.encode_raw(&mut buf);
        buf
    }

    /// Encodes the message with a length-delimiter to a buffer.
    ///
    /// An error will be returned if the buffer does not have sufficient capacity.
    fn encode_length_delimited<B>(&self, buf: &mut B) -> Result<(), EncodeError>
    where
        B: BufMut,
        Self: Sized,
    {
        let len = self.encoded_len();
        let required = len + encoded_len_varint(len as u64);
        let remaining = buf.remaining_mut();
        if required > remaining {
            return Err(EncodeError::new(required, remaining));
        }
        encode_varint(len as u64, buf);
        self.encode_raw(buf);
        Ok(())
    }

    #[cfg(feature = "std")]
    /// Encodes the message with a length-delimiter to a newly allocated buffer.
    fn encode_length_delimited_to_vec(&self) -> Vec<u8>
    where
        Self: Sized,
    {
        let len = self.encoded_len();
        let mut buf = Vec::with_capacity(len + encoded_len_varint(len as u64));

        encode_varint(len as u64, &mut buf);
        self.encode_raw(&mut buf);
        buf
    }

    /// Decodes an instance of the message from a buffer.
    ///
    /// The entire buffer will be consumed.
    fn decode<B>(mut buf: B) -> Result<Self, DecodeError>
    where
        B: Buf,
        Self: Default,
    {
        let mut message = Self::default();
        Self::merge(&mut message, &mut buf).map(|_| message)
    }

    /// Decodes a length-delimited instance of the message from the buffer.
    fn decode_length_delimited<B>(buf: B) -> Result<Self, DecodeError>
    where
        B: Buf,
        Self: Default,
    {
        let mut message = Self::default();
        message.merge_length_delimited(buf)?;
        Ok(message)
    }

    /// Decodes an instance of the message from a buffer, and merges it into `self`.
    ///
    /// The entire buffer will be consumed.
    fn merge<B>(&mut self, mut buf: B) -> Result<(), DecodeError>
    where
        B: Buf,
        Self: Sized,
    {
        let ctx = DecodeContext::default();
        while buf.has_remaining() {
            let (tag, wire_type) = decode_key(&mut buf)?;
            self.merge_field(tag, wire_type, &mut buf, ctx.clone())?;
        }
        Ok(())
    }

    /// Decodes a length-delimited instance of the message from buffer, and
    /// merges it into `self`.
    fn merge_length_delimited<B>(&mut self, mut buf: B) -> Result<(), DecodeError>
    where
        B: Buf,
        Self: Sized,
    {
        message::merge(
            WireType::LengthDelimited,
            self,
            &mut buf,
            DecodeContext::default(),
        )
    }

    /// Clears the message, resetting all fields to their default.
    fn clear(&mut self);
}

impl<M> Message for Box<M>
where
    M: Message,
{
    fn encode_raw<B>(&self, buf: &mut B)
    where
        B: BufMut,
    {
        (**self).encode_raw(buf)
    }
    fn merge_field<B>(
        &mut self,
        tag: u32,
        wire_type: WireType,
        buf: &mut B,
        ctx: DecodeContext,
    ) -> Result<(), DecodeError>
    where
        B: Buf,
    {
        (**self).merge_field(tag, wire_type, buf, ctx)
    }
    fn encoded_len(&self) -> usize {
        (**self).encoded_len()
    }
    fn clear(&mut self) {
        (**self).clear()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const _MESSAGE_IS_OBJECT_SAFE: Option<&dyn Message> = None;
}
