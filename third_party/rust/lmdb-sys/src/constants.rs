use libc::{c_int, c_uint};

////////////////////////////////////////////////////////////////////////////////////////////////////
//// Environment Flags
////////////////////////////////////////////////////////////////////////////////////////////////////

/// mmap at a fixed address (experimental)
pub const MDB_FIXEDMAP: c_uint = 0x01;
/// no environment directory
pub const MDB_NOSUBDIR: c_uint = 0x4000;
/// don't fsync after commit
pub const MDB_NOSYNC: c_uint = 0x10000;
/// read only
pub const MDB_RDONLY: c_uint = 0x20000;
/// don't fsync metapage after commit
pub const MDB_NOMETASYNC: c_uint = 0x40000;
/// use writable mmap
pub const MDB_WRITEMAP: c_uint = 0x80000;
/// use asynchronous msync when #MDB_WRITEMAP is used
pub const MDB_MAPASYNC: c_uint = 0x100000;
/// tie reader locktable slots to #MDB_txn objects instead of to threads
pub const MDB_NOTLS: c_uint = 0x200000;
/// don't do any locking, caller must manage their own locks
pub const MDB_NOLOCK: c_uint = 0x400000;
/// don't do readahead (no effect on Windows)
pub const MDB_NORDAHEAD: c_uint = 0x800000;
/// don't initialize malloc'd memory before writing to datafile
pub const MDB_NOMEMINIT: c_uint = 0x1000000;

////////////////////////////////////////////////////////////////////////////////////////////////////
//// Database Flags
////////////////////////////////////////////////////////////////////////////////////////////////////

/// use reverse string keys
pub const MDB_REVERSEKEY: c_uint = 0x02;
/// use sorted duplicates
pub const MDB_DUPSORT: c_uint = 0x04;
/// numeric keys in native byte order. The keys must all be of the same size.
pub const MDB_INTEGERKEY: c_uint = 0x08;
/// with `MDB_DUPSORT`, sorted dup items have fixed size.
pub const MDB_DUPFIXED: c_uint = 0x10;
/// with `MDB_DUPSORT`, dups are numeric in native byte order.
pub const MDB_INTEGERDUP: c_uint = 0x20;
/// with #MDB_DUPSORT, use reverse string dups.
pub const MDB_REVERSEDUP: c_uint = 0x40;
/// create DB if not already existing.
pub const MDB_CREATE: c_uint = 0x40000;

////////////////////////////////////////////////////////////////////////////////////////////////////
//// Write Flags
////////////////////////////////////////////////////////////////////////////////////////////////////

/// For put: Don't write if the key already exists.
pub const MDB_NOOVERWRITE: c_uint = 0x10;
/// Only for `MDB_DUPSORT`.
///
/// For put: don't write if the key and data pair already exist.
/// For `mdb_cursor_del`: remove all duplicate data items.
pub const MDB_NODUPDATA: c_uint = 0x20;
/// For `mdb_cursor_put`: overwrite the current key/data pair.
pub const MDB_CURRENT: c_uint = 0x40;
/// For put: Just reserve space for data, don't copy it. Return a pointer to the reserved space.
pub const MDB_RESERVE: c_uint = 0x10000;
/// Data is being appended, don't split full pages.
pub const MDB_APPEND: c_uint = 0x20000;
/// Duplicate data is being appended, don't split full pages.
pub const MDB_APPENDDUP: c_uint = 0x40000;
/// Store multiple data items in one call. Only for #MDB_DUPFIXED.
pub const MDB_MULTIPLE: c_uint = 0x80000;

////////////////////////////////////////////////////////////////////////////////////////////////////
//// Copy Flags
////////////////////////////////////////////////////////////////////////////////////////////////////

/// Compacting copy: Omit free space from copy, and renumber all pages sequentially.
pub const MDB_CP_COMPACT: c_uint = 0x01;

////////////////////////////////////////////////////////////////////////////////////////////////////
//// Return Codes
////////////////////////////////////////////////////////////////////////////////////////////////////

/// Successful result.
pub const MDB_SUCCESS: c_int = 0;
/// key/data pair already exists.
pub const MDB_KEYEXIST: c_int = -30799;
/// key/data pair not found (EOF).
pub const MDB_NOTFOUND: c_int = -30798;
/// Requested page not found - this usually indicates corruption.
pub const MDB_PAGE_NOTFOUND: c_int = -30797;
/// Located page was wrong type.
pub const MDB_CORRUPTED: c_int = -30796;
/// Update of meta page failed or environment had fatal error.
pub const MDB_PANIC: c_int = -30795;
/// Environment version mismatch.
pub const MDB_VERSION_MISMATCH: c_int = -30794;
/// File is not a valid LMDB file.
pub const MDB_INVALID: c_int = -30793;
/// Environment mapsize reached.
pub const MDB_MAP_FULL: c_int = -30792;
/// Environment maxdbs reached.
pub const MDB_DBS_FULL: c_int = -30791;
/// Environment maxreaders reached.
pub const MDB_READERS_FULL: c_int = -30790;
/// Too many TLS keys in use - Windows only.
pub const MDB_TLS_FULL: c_int = -30789;
/// Txn has too many dirty pages.
pub const MDB_TXN_FULL: c_int = -30788;
/// Cursor stack too deep - internal error.
pub const MDB_CURSOR_FULL: c_int = -30787;
/// Page has not enough space - internal error.
pub const MDB_PAGE_FULL: c_int = -30786;
/// Database contents grew beyond environment mapsize.
pub const MDB_MAP_RESIZED: c_int = -30785;
/// MDB_INCOMPATIBLE: Operation and DB incompatible, or DB flags changed.
pub const MDB_INCOMPATIBLE: c_int = -30784;
/// Invalid reuse of reader locktable slot.
pub const MDB_BAD_RSLOT: c_int = -30783;
/// Transaction cannot recover - it must be aborted.
pub const MDB_BAD_TXN: c_int = -30782;
/// Unsupported size of key/DB name/data, or wrong DUPFIXED size.
pub const MDB_BAD_VALSIZE: c_int = -30781;
/// The specified DBI was changed unexpectedly.
pub const MDB_BAD_DBI: c_int = -30780;
/// The last defined error code.
pub const MDB_LAST_ERRCODE: c_int = MDB_BAD_DBI;
