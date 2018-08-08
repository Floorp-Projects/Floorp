use libc::c_uint;

use ffi::*;

bitflags! {
    #[doc="Environment options."]
    #[derive(Default)]
    pub struct EnvironmentFlags: c_uint {

        #[doc="Use a fixed address for the mmap region. This flag must be specified"]
        #[doc="when creating the environment, and is stored persistently in the environment."]
        #[doc="If successful, the memory map will always reside at the same virtual address"]
        #[doc="and pointers used to reference data items in the database will be constant"]
        #[doc="across multiple invocations. This option may not always work, depending on"]
        #[doc="how the operating system has allocated memory to shared libraries and other uses."]
        #[doc="The feature is highly experimental."]
        const FIXED_MAP = MDB_FIXEDMAP;

        #[doc="By default, LMDB creates its environment in a directory whose pathname is given in"]
        #[doc="`path`, and creates its data and lock files under that directory. With this option,"]
        #[doc="`path` is used as-is for the database main data file. The database lock file is the"]
        #[doc="`path` with `-lock` appended."]
        const NO_SUB_DIR = MDB_NOSUBDIR;

        #[doc="Use a writeable memory map unless `READ_ONLY` is set. This is faster and uses"]
        #[doc="fewer mallocs, but loses protection from application bugs like wild pointer writes"]
        #[doc="and other bad updates into the database. Incompatible with nested transactions."]
        #[doc="Processes with and without `WRITE_MAP` on the same environment do not cooperate"]
        #[doc="well."]
        const WRITE_MAP = MDB_WRITEMAP;

        #[doc="Open the environment in read-only mode. No write operations will be allowed."]
        #[doc="When opening an environment, LMDB will still modify the lock file - except on"]
        #[doc="read-only filesystems, where LMDB does not use locks."]
        const READ_ONLY = MDB_RDONLY;

        #[doc="Flush system buffers to disk only once per transaction, omit the metadata flush."]
        #[doc="Defer that until the system flushes files to disk, or next non-`READ_ONLY` commit"]
        #[doc="or `Environment::sync`. This optimization maintains database integrity, but a"]
        #[doc="system crash may undo the last committed transaction. I.e. it preserves the ACI"]
        #[doc="(atomicity, consistency, isolation) but not D (durability) database property."]
        #[doc="\n\nThis flag may be changed at any time using `Environment::set_flags`."]
        const NO_META_SYNC = MDB_NOMETASYNC;

        #[doc="Don't flush system buffers to disk when committing a transaction. This optimization"]
        #[doc="means a system crash can corrupt the database or lose the last transactions if"]
        #[doc="buffers are not yet flushed to disk. The risk is governed by how often the system"]
        #[doc="flushes dirty buffers to disk and how often `Environment::sync` is called. However,"]
        #[doc="if the filesystem preserves write order and the `WRITE_MAP` flag is not used,"]
        #[doc="transactions exhibit ACI (atomicity, consistency, isolation) properties and only"]
        #[doc="lose D (durability). I.e. database integrity is maintained, but a system"]
        #[doc="crash may undo the final transactions. Note that (`NO_SYNC | WRITE_MAP`) leaves the"]
        #[doc="system with no hint for when to write transactions to disk, unless"]
        #[doc="`Environment::sync` is called. (`MAP_ASYNC | WRITE_MAP`) may be preferable."]
        #[doc="\n\nThis flag may be changed at any time using `Environment::set_flags`."]
        const NO_SYNC = MDB_NOSYNC;

        #[doc="When using `WRITE_MAP`, use asynchronous flushes to disk. As with `NO_SYNC`, a"]
        #[doc="system crash can then corrupt the database or lose the last transactions. Calling"]
        #[doc="`Environment::sync` ensures on-disk database integrity until next commit."]
        #[doc="\n\nThis flag may be changed at any time using `Environment::set_flags`."]
        const MAP_ASYNC = MDB_MAPASYNC;

        #[doc="Don't use thread-local storage. Tie reader locktable slots to transaction objects"]
        #[doc="instead of to threads. I.e. `RoTransaction::reset` keeps the slot reserved for the"]
        #[doc="transaction object. A thread may use parallel read-only transactions. A read-only"]
        #[doc="transaction may span threads if the user synchronizes its use. Applications that"]
        #[doc="multiplex many the user synchronizes its use. Applications that multiplex many user"]
        #[doc="threads over individual OS threads need this option. Such an application must also"]
        #[doc="serialize the write transactions in an OS thread, since LMDB's write locking is"]
        #[doc="unaware of the user threads."]
        const NO_TLS = MDB_NOTLS;

        #[doc="Do not do any locking. If concurrent access is anticipated, the caller must manage"]
        #[doc="all concurrency themself. For proper operation the caller must enforce"]
        #[doc="single-writer semantics, and must ensure that no readers are using old"]
        #[doc="transactions while a writer is active. The simplest approach is to use an exclusive"]
        #[doc="lock so that no readers may be active at all when a writer begins."]
        const NO_LOCK = MDB_NOLOCK;

        #[doc="Turn off readahead. Most operating systems perform readahead on read requests by"]
        #[doc="default. This option turns it off if the OS supports it. Turning it off may help"]
        #[doc="random read performance when the DB is larger than RAM and system RAM is full."]
        #[doc="The option is not implemented on Windows."]
        const NO_READAHEAD = MDB_NORDAHEAD;

        #[doc="Do not initialize malloc'd memory before writing to unused spaces in the data file."]
        #[doc="By default, memory for pages written to the data file is obtained using malloc."]
        #[doc="While these pages may be reused in subsequent transactions, freshly malloc'd pages"]
        #[doc="will be initialized to zeroes before use. This avoids persisting leftover data from"]
        #[doc="other code (that used the heap and subsequently freed the memory) into the data"]
        #[doc="file. Note that many other system libraries may allocate and free memory from the"]
        #[doc="heap for arbitrary uses. E.g., stdio may use the heap for file I/O buffers. This"]
        #[doc="initialization step has a modest performance cost so some applications may want to"]
        #[doc="disable it using this flag. This option can be a problem for applications which"]
        #[doc="handle sensitive data like passwords, and it makes memory checkers like Valgrind"]
        #[doc="noisy. This flag is not needed with `WRITE_MAP`, which writes directly to the mmap"]
        #[doc="instead of using malloc for pages. The initialization is also skipped if writing"]
        #[doc="with reserve; the caller is expected to overwrite all of the memory that was"]
        #[doc="reserved in that case."]
        #[doc="\n\nThis flag may be changed at any time using `Environment::set_flags`."]
        const NO_MEM_INIT = MDB_NOMEMINIT;
    }
}

bitflags! {
    #[doc="Database options."]
    #[derive(Default)]
    pub struct DatabaseFlags: c_uint {

        #[doc="Keys are strings to be compared in reverse order, from the end of the strings"]
        #[doc="to the beginning. By default, Keys are treated as strings and compared from"]
        #[doc="beginning to end."]
        const REVERSE_KEY = MDB_REVERSEKEY;

        #[doc="Duplicate keys may be used in the database. (Or, from another perspective,"]
        #[doc="keys may have multiple data items, stored in sorted order.) By default"]
        #[doc="keys must be unique and may have only a single data item."]
        const DUP_SORT = MDB_DUPSORT;

        #[doc="Keys are binary integers in native byte order. Setting this option requires all"]
        #[doc="keys to be the same size, typically 32 or 64 bits."]
        const INTEGER_KEY = MDB_INTEGERKEY;

        #[doc="This flag may only be used in combination with `DUP_SORT`. This option tells"]
        #[doc="the library that the data items for this database are all the same size, which"]
        #[doc="allows further optimizations in storage and retrieval. When all data items are"]
        #[doc="the same size, the `GET_MULTIPLE` and `NEXT_MULTIPLE` cursor operations may be"]
        #[doc="used to retrieve multiple items at once."]
        const DUP_FIXED = MDB_DUPFIXED;

        #[doc="This option specifies that duplicate data items are also integers, and"]
        #[doc="should be sorted as such."]
        const INTEGER_DUP = MDB_INTEGERDUP;

        #[doc="This option specifies that duplicate data items should be compared as strings"]
        #[doc="in reverse order."]
        const REVERSE_DUP = MDB_REVERSEDUP;
    }
}

bitflags! {
    #[doc="Write options."]
    #[derive(Default)]
    pub struct WriteFlags: c_uint {

        #[doc="Insert the new item only if the key does not already appear in the database."]
        #[doc="The function will return `LmdbError::KeyExist` if the key already appears in the"]
        #[doc="database, even if the database supports duplicates (`DUP_SORT`)."]
        const NO_OVERWRITE = MDB_NOOVERWRITE;

        #[doc="Insert the new item only if it does not already appear in the database."]
        #[doc="This flag may only be specified if the database was opened with `DUP_SORT`."]
        #[doc="The function will return `LmdbError::KeyExist` if the item already appears in the"]
        #[doc="database."]
        const NO_DUP_DATA = MDB_NODUPDATA;

        #[doc="For `Cursor::put`. Replace the item at the current cursor position. The key"]
        #[doc="parameter must match the current position. If using sorted duplicates (`DUP_SORT`)"]
        #[doc="the data item must still sort into the same position. This is intended to be used"]
        #[doc="when the new data is the same size as the old. Otherwise it will simply perform a"]
        #[doc="delete of the old record followed by an insert."]
        const CURRENT = MDB_CURRENT;

        #[doc="Append the given item to the end of the database. No key comparisons are performed."]
        #[doc="This option allows fast bulk loading when keys are already known to be in the"]
        #[doc="correct order. Loading unsorted keys with this flag will cause data corruption."]
        const APPEND = MDB_APPEND;

        #[doc="Same as `APPEND`, but for sorted dup data."]
        const APPEND_DUP = MDB_APPENDDUP;
    }
}
