use DirEntry;

/// Unix-specific extension methods for `walkdir::DirEntry`
pub trait DirEntryExt {
    /// Returns the underlying `d_ino` field in the contained `dirent`
    /// structure.
    fn ino(&self) -> u64;
}

impl DirEntryExt for DirEntry {
    /// Returns the underlying `d_ino` field in the contained `dirent`
    /// structure.
    fn ino(&self) -> u64 {
        self.ino
    }
}
