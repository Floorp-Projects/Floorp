use std::fs::File;
use std::io;

pub struct MmapInner {
    // Private member to prevent external construction
    // (might be nice to change the type to ! once that's stable)
    __: (),
}

impl MmapInner {
    fn new() -> io::Result<MmapInner> {
        Err(io::Error::new(
            io::ErrorKind::Other,
            "platform not supported",
        ))
    }

    pub fn map(_: usize, _: &File, _: u64, _: bool) -> io::Result<MmapInner> {
        MmapInner::new()
    }

    pub fn map_exec(_: usize, _: &File, _: u64, _: bool) -> io::Result<MmapInner> {
        MmapInner::new()
    }

    pub fn map_mut(_: usize, _: &File, _: u64, _: bool) -> io::Result<MmapInner> {
        MmapInner::new()
    }

    pub fn map_copy(_: usize, _: &File, _: u64, _: bool) -> io::Result<MmapInner> {
        MmapInner::new()
    }

    pub fn map_copy_read_only(_: usize, _: &File, _: u64, _: bool) -> io::Result<MmapInner> {
        MmapInner::new()
    }

    pub fn map_anon(_: usize, _: bool) -> io::Result<MmapInner> {
        MmapInner::new()
    }

    pub fn flush(&self, _: usize, _: usize) -> io::Result<()> {
        unreachable!("self unconstructable");
    }

    pub fn flush_async(&self, _: usize, _: usize) -> io::Result<()> {
        unreachable!("self unconstructable");
    }

    pub fn make_read_only(&mut self) -> io::Result<()> {
        unreachable!("self unconstructable");
    }

    pub fn make_exec(&mut self) -> io::Result<()> {
        unreachable!("self unconstructable");
    }

    pub fn make_mut(&mut self) -> io::Result<()> {
        unreachable!("self unconstructable");
    }

    #[inline]
    pub fn ptr(&self) -> *const u8 {
        unreachable!("self unconstructable");
    }

    #[inline]
    pub fn mut_ptr(&mut self) -> *mut u8 {
        unreachable!("self unconstructable");
    }

    #[inline]
    pub fn len(&self) -> usize {
        unreachable!("self unconstructable");
    }
}

pub fn file_len(file: &File) -> io::Result<u64> {
    Ok(file.metadata()?.len())
}
