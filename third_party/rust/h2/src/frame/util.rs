use super::Error;
use bytes::Bytes;

/// Strip padding from the given payload.
///
/// It is assumed that the frame had the padded flag set. This means that the
/// first byte is the length of the padding with that many
/// 0 bytes expected to follow the actual payload.
///
/// # Returns
///
/// A slice of the given payload where the actual one is found and the length
/// of the padding.
///
/// If the padded payload is invalid (e.g. the length of the padding is equal
/// to the total length), returns `None`.
pub fn strip_padding(payload: &mut Bytes) -> Result<u8, Error> {
    if payload.len() == 0 {
        // If this is the case, the frame is invalid as no padding length can be
        // extracted, even though the frame should be padded.
        return Err(Error::TooMuchPadding);
    }

    let pad_len = payload[0] as usize;

    if pad_len >= payload.len() {
        // This is invalid: the padding length MUST be less than the
        // total frame size.
        return Err(Error::TooMuchPadding);
    }

    let _ = payload.split_to(1);
    let _ = payload.split_off(pad_len);

    Ok(pad_len as u8)
}
