pub mod integer;
pub mod integermulti;
pub mod multi;
pub mod single;

use lmdb::DatabaseFlags;

#[derive(Default, Debug, Copy, Clone)]
pub struct Options {
    pub create: bool,
    pub flags: DatabaseFlags,
}

impl Options {
    pub fn create() -> Options {
        Options {
            create: true,
            flags: DatabaseFlags::empty(),
        }
    }
}
