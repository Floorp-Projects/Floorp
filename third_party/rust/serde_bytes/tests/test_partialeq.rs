use serde_bytes::{Bytes, ByteBuf};

fn _bytes_eq_slice(bytes: &Bytes, slice: &[u8]) -> bool {
    bytes == slice
}

fn _bytebuf_eq_vec(bytebuf: ByteBuf, vec: Vec<u8>) -> bool {
    bytebuf == vec
}

fn _bytes_eq_bytestring(bytes: &Bytes) -> bool {
    bytes == b"..."
}
