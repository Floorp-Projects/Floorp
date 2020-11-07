#[derive(Debug)]
pub struct Buffer {
    /// Size of this buffer
    pub(crate) size: u64,
}

impl Buffer {
    pub fn new(size: u64) -> Self {
        Buffer { size }
    }
}
