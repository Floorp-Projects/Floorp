use std::sync::{ Arc, Mutex, Condvar};

// LockState can be in several possible states:

// 1. 0 readers, false upgradeable_reader, false writer. No-one has any locks, anyone can acquire anything (initial state)
// 2. 1+ readers, false upgradeable_reader, false writer - readers. writer cannot be acquired, but upgradeable_reader can be
// 3. 1+ readers, true upgradeable_reader, false writer - bunch of readers, and one thread that could upgrade to writer, but not yet
// 4. 0 readers, true upgradeable_reader, false writer - upgradeable_reader thread can upgrade to writer
// 5. 0 readers, false upgradeable_reader, true writer - writer only. Nothing else can be acquired.

struct LockState {
    readers: u32,
    upgradeable_reader: bool,
    writer: bool
}

struct Locks(Arc<(Mutex<LockState>, Condvar)>);

impl Locks {
    pub fn new() -> Locks {
        Locks(Arc::new((Mutex::new(
        LockState {
            readers: 0,
            upgradeable_reader: false,
            writer: false
        }), Condvar::new())))
    }

    pub fn read() -> 
}