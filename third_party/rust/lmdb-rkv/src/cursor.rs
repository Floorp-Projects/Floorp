use libc::{c_void, size_t, c_uint};
use std::{fmt, ptr, result, slice};
use std::marker::PhantomData;

use database::Database;
use error::{Error, Result, lmdb_result};
use ffi;
use flags::WriteFlags;
use transaction::Transaction;

/// An LMDB cursor.
pub trait Cursor<'txn> {

    /// Returns a raw pointer to the underlying LMDB cursor.
    ///
    /// The caller **must** ensure that the pointer is not used after the
    /// lifetime of the cursor.
    fn cursor(&self) -> *mut ffi::MDB_cursor;

    /// Retrieves a key/data pair from the cursor. Depending on the cursor op,
    /// the current key may be returned.
    fn get(&self,
           key: Option<&[u8]>,
           data: Option<&[u8]>,
           op: c_uint)
           -> Result<(Option<&'txn [u8]>, &'txn [u8])> {
        unsafe {
            let mut key_val = slice_to_val(key);
            let mut data_val = slice_to_val(data);
            let key_ptr = key_val.mv_data;
            lmdb_result(ffi::mdb_cursor_get(self.cursor(), &mut key_val, &mut data_val, op))?;
            let key_out = if key_ptr != key_val.mv_data { Some(val_to_slice(key_val)) } else { None };
            let data_out = val_to_slice(data_val);
            Ok((key_out, data_out))
        }
    }

    /// Iterate over database items. The iterator will begin with item next
    /// after the cursor, and continue until the end of the database. For new
    /// cursors, the iterator will begin with the first item in the database.
    ///
    /// For databases with duplicate data items (`DatabaseFlags::DUP_SORT`), the
    /// duplicate data items of each key will be returned before moving on to
    /// the next key.
    fn iter(&mut self) -> Iter<'txn> {
        Iter::new(self.cursor(), ffi::MDB_NEXT, ffi::MDB_NEXT)
    }

    /// Iterate over database items starting from the beginning of the database.
    ///
    /// For databases with duplicate data items (`DatabaseFlags::DUP_SORT`), the
    /// duplicate data items of each key will be returned before moving on to
    /// the next key.
    fn iter_start(&mut self) -> Iter<'txn> {
        self.get(None, None, ffi::MDB_FIRST).unwrap();
        Iter::new(self.cursor(), ffi::MDB_GET_CURRENT, ffi::MDB_NEXT)
    }

    /// Iterate over database items starting from the given key.
    ///
    /// For databases with duplicate data items (`DatabaseFlags::DUP_SORT`), the
    /// duplicate data items of each key will be returned before moving on to
    /// the next key.
    fn iter_from<K>(&mut self, key: K) -> Iter<'txn> where K: AsRef<[u8]> {
        match self.get(Some(key.as_ref()), None, ffi::MDB_SET_RANGE) {
            Err(Error::NotFound) => Ok(()),
            Err(error) => Err(error),
            Ok(_) => Ok(()),
        }.unwrap();
        Iter::new(self.cursor(), ffi::MDB_GET_CURRENT, ffi::MDB_NEXT)
    }

    /// Iterate over duplicate database items. The iterator will begin with the
    /// item next after the cursor, and continue until the end of the database.
    /// Each item will be returned as an iterator of its duplicates.
    fn iter_dup(&mut self) -> IterDup<'txn> {
        IterDup::new(self.cursor(), ffi::MDB_NEXT)
    }

    /// Iterate over duplicate database items starting from the beginning of the
    /// database. Each item will be returned as an iterator of its duplicates.
    fn iter_dup_start(&mut self) -> IterDup<'txn> {
        self.get(None, None, ffi::MDB_FIRST).unwrap();
        IterDup::new(self.cursor(), ffi::MDB_GET_CURRENT)
    }

    /// Iterate over duplicate items in the database starting from the given
    /// key. Each item will be returned as an iterator of its duplicates.
    fn iter_dup_from<K>(&mut self, key: &K) -> IterDup<'txn> where K: AsRef<[u8]> {
        match self.get(Some(key.as_ref()), None, ffi::MDB_SET_RANGE) {
            Err(Error::NotFound) => Ok(()),
            Err(error) => Err(error),
            Ok(_) => Ok(()),
        }.unwrap();
        IterDup::new(self.cursor(), ffi::MDB_GET_CURRENT)
    }

    /// Iterate over the duplicates of the item in the database with the given
    /// key.
    fn iter_dup_of<K>(&mut self, key: &K) -> Result<Iter<'txn>> where K:
        AsRef<[u8]> {
        self.get(Some(key.as_ref()), None, ffi::MDB_SET)?;
        Ok(Iter::new(self.cursor(), ffi::MDB_GET_CURRENT, ffi::MDB_NEXT_DUP))
    }
}

/// A read-only cursor for navigating the items within a database.
pub struct RoCursor<'txn> {
    cursor: *mut ffi::MDB_cursor,
    _marker: PhantomData<fn() -> &'txn ()>,
}

impl <'txn> Cursor<'txn> for RoCursor<'txn> {
    fn cursor(&self) -> *mut ffi::MDB_cursor {
        self.cursor
    }
}

impl <'txn> fmt::Debug for RoCursor<'txn> {
    fn fmt(&self, f: &mut fmt::Formatter) -> result::Result<(), fmt::Error> {
        f.debug_struct("RoCursor").finish()
    }
}

impl <'txn> Drop for RoCursor<'txn> {
    fn drop(&mut self) {
        unsafe { ffi::mdb_cursor_close(self.cursor) }
    }
}

impl <'txn> RoCursor<'txn> {

    /// Creates a new read-only cursor in the given database and transaction.
    /// Prefer using `Transaction::open_cursor`.
    pub(crate) fn new<T>(txn: &'txn T, db: Database) -> Result<RoCursor<'txn>> where T: Transaction {
        let mut cursor: *mut ffi::MDB_cursor = ptr::null_mut();
        unsafe { lmdb_result(ffi::mdb_cursor_open(txn.txn(), db.dbi(), &mut cursor))?; }
        Ok(RoCursor {
            cursor: cursor,
            _marker: PhantomData,
        })
    }
}

/// A read-write cursor for navigating items within a database.
pub struct RwCursor<'txn> {
    cursor: *mut ffi::MDB_cursor,
    _marker: PhantomData<fn() -> &'txn ()>,
}

impl <'txn> Cursor<'txn> for RwCursor<'txn> {
    fn cursor(&self) -> *mut ffi::MDB_cursor {
        self.cursor
    }
}

impl <'txn> fmt::Debug for RwCursor<'txn> {
    fn fmt(&self, f: &mut fmt::Formatter) -> result::Result<(), fmt::Error> {
        f.debug_struct("RwCursor").finish()
    }
}

impl <'txn> Drop for RwCursor<'txn> {
    fn drop(&mut self) {
        unsafe { ffi::mdb_cursor_close(self.cursor) }
    }
}

impl <'txn> RwCursor<'txn> {

    /// Creates a new read-only cursor in the given database and transaction.
    /// Prefer using `RwTransaction::open_rw_cursor`.
    pub(crate) fn new<T>(txn: &'txn T, db: Database) -> Result<RwCursor<'txn>> where T: Transaction {
        let mut cursor: *mut ffi::MDB_cursor = ptr::null_mut();
        unsafe { lmdb_result(ffi::mdb_cursor_open(txn.txn(), db.dbi(), &mut cursor))?; }
        Ok(RwCursor { cursor: cursor, _marker: PhantomData })
    }

    /// Puts a key/data pair into the database. The cursor will be positioned at
    /// the new data item, or on failure usually near it.
    pub fn put<K, D>(&mut self, key: &K, data: &D, flags: WriteFlags) -> Result<()>
    where K: AsRef<[u8]>, D: AsRef<[u8]> {
        let key = key.as_ref();
        let data = data.as_ref();
        let mut key_val: ffi::MDB_val = ffi::MDB_val { mv_size: key.len() as size_t,
                                                       mv_data: key.as_ptr() as *mut c_void };
        let mut data_val: ffi::MDB_val = ffi::MDB_val { mv_size: data.len() as size_t,
                                                        mv_data: data.as_ptr() as *mut c_void };
        unsafe {
            lmdb_result(ffi::mdb_cursor_put(self.cursor(),
                                            &mut key_val,
                                            &mut data_val,
                                            flags.bits()))
        }
    }

    /// Deletes the current key/data pair.
    ///
    /// ### Flags
    ///
    /// `WriteFlags::NO_DUP_DATA` may be used to delete all data items for the
    /// current key, if the database was opened with `DatabaseFlags::DUP_SORT`.
    pub fn del(&mut self, flags: WriteFlags) -> Result<()> {
        unsafe { lmdb_result(ffi::mdb_cursor_del(self.cursor(), flags.bits())) }
    }
}

unsafe fn slice_to_val(slice: Option<&[u8]>) -> ffi::MDB_val {
    match slice {
        Some(slice) =>
            ffi::MDB_val { mv_size: slice.len() as size_t,
                           mv_data: slice.as_ptr() as *mut c_void },
        None =>
            ffi::MDB_val { mv_size: 0,
                           mv_data: ptr::null_mut() },
    }
}

unsafe fn val_to_slice<'a>(val: ffi::MDB_val) -> &'a [u8] {
    slice::from_raw_parts(val.mv_data as *const u8, val.mv_size as usize)
}

/// An iterator over the values in an LMDB database.
pub struct Iter<'txn> {
    cursor: *mut ffi::MDB_cursor,
    op: c_uint,
    next_op: c_uint,
    _marker: PhantomData<fn(&'txn ())>,
}

impl <'txn> Iter<'txn> {

    /// Creates a new iterator backed by the given cursor.
    fn new<'t>(cursor: *mut ffi::MDB_cursor, op: c_uint, next_op: c_uint) -> Iter<'t> {
        Iter { cursor: cursor, op: op, next_op: next_op, _marker: PhantomData }
    }
}

impl <'txn> fmt::Debug for Iter<'txn> {
    fn fmt(&self, f: &mut fmt::Formatter) -> result::Result<(), fmt::Error> {
        f.debug_struct("Iter").finish()
    }
}

impl <'txn> Iterator for Iter<'txn> {

    type Item = (&'txn [u8], &'txn [u8]);

    fn next(&mut self) -> Option<(&'txn [u8], &'txn [u8])> {
        let mut key = ffi::MDB_val { mv_size: 0, mv_data: ptr::null_mut() };
        let mut data = ffi::MDB_val { mv_size: 0, mv_data: ptr::null_mut() };

        unsafe {
            let err_code = ffi::mdb_cursor_get(self.cursor, &mut key, &mut data, self.op);
            // Set the operation for the next get
            self.op = self.next_op;
            if err_code == ffi::MDB_SUCCESS {
                Some((val_to_slice(key), val_to_slice(data)))
            } else {
                // The documentation for mdb_cursor_get specifies that it may fail with MDB_NOTFOUND
                // and MDB_EINVAL (and we shouldn't be passing in invalid parameters).
                // TODO: validate that these are the only failures possible.
                debug_assert!(err_code == ffi::MDB_NOTFOUND,
                              "Unexpected LMDB error {:?}.", Error::from_err_code(err_code));
                None
            }
        }
    }
}

/// An iterator over the keys and duplicate values in an LMDB database.
///
/// The yielded items of the iterator are themselves iterators over the duplicate values for a
/// specific key.
pub struct IterDup<'txn> {
    cursor: *mut ffi::MDB_cursor,
    op: c_uint,
    _marker: PhantomData<fn(&'txn ())>,
}

impl <'txn> IterDup<'txn> {

    /// Creates a new iterator backed by the given cursor.
    fn new<'t>(cursor: *mut ffi::MDB_cursor, op: c_uint) -> IterDup<'t> {
        IterDup { cursor: cursor, op: op, _marker: PhantomData }
    }
}

impl <'txn> fmt::Debug for IterDup<'txn> {
    fn fmt(&self, f: &mut fmt::Formatter) -> result::Result<(), fmt::Error> {
        f.debug_struct("IterDup").finish()
    }
}

impl <'txn> Iterator for IterDup<'txn> {

    type Item = Iter<'txn>;

    fn next(&mut self) -> Option<Iter<'txn>> {
        let mut key = ffi::MDB_val { mv_size: 0, mv_data: ptr::null_mut() };
        let mut data = ffi::MDB_val { mv_size: 0, mv_data: ptr::null_mut() };
        let err_code = unsafe {
            ffi::mdb_cursor_get(self.cursor, &mut key, &mut data, self.op)
        };

        if err_code == ffi::MDB_SUCCESS {
            self.op = ffi::MDB_NEXT;
            Some(Iter::new(self.cursor, ffi::MDB_GET_CURRENT, ffi::MDB_NEXT_DUP))
        } else {
            None
        }
    }
}

#[cfg(test)]
mod test {

    use std::ptr;
    use test::{Bencher, black_box};

    use tempdir::TempDir;

    use environment::*;
    use ffi::*;
    use flags::*;
    use super::*;
    use test_utils::*;

    #[test]
    fn test_get() {
        let dir = TempDir::new("test").unwrap();
        let env = Environment::new().open(dir.path()).unwrap();
        let db = env.open_db(None).unwrap();

        let mut txn = env.begin_rw_txn().unwrap();
        txn.put(db, b"key1", b"val1", WriteFlags::empty()).unwrap();
        txn.put(db, b"key2", b"val2", WriteFlags::empty()).unwrap();
        txn.put(db, b"key3", b"val3", WriteFlags::empty()).unwrap();

        let cursor = txn.open_ro_cursor(db).unwrap();
        assert_eq!((Some(&b"key1"[..]), &b"val1"[..]),
                   cursor.get(None, None, MDB_FIRST).unwrap());
        assert_eq!((Some(&b"key1"[..]), &b"val1"[..]),
                   cursor.get(None, None, MDB_GET_CURRENT).unwrap());
        assert_eq!((Some(&b"key2"[..]), &b"val2"[..]),
                   cursor.get(None, None, MDB_NEXT).unwrap());
        assert_eq!((Some(&b"key1"[..]), &b"val1"[..]),
                   cursor.get(None, None, MDB_PREV).unwrap());
        assert_eq!((Some(&b"key3"[..]), &b"val3"[..]),
                   cursor.get(None, None, MDB_LAST).unwrap());
        assert_eq!((None, &b"val2"[..]),
                   cursor.get(Some(b"key2"), None, MDB_SET).unwrap());
        assert_eq!((Some(&b"key3"[..]), &b"val3"[..]),
                   cursor.get(Some(&b"key3"[..]), None, MDB_SET_KEY).unwrap());
        assert_eq!((Some(&b"key3"[..]), &b"val3"[..]),
                   cursor.get(Some(&b"key2\0"[..]), None, MDB_SET_RANGE).unwrap());
    }

    #[test]
    fn test_get_dup() {
        let dir = TempDir::new("test").unwrap();
        let env = Environment::new().open(dir.path()).unwrap();
        let db = env.create_db(None, DatabaseFlags::DUP_SORT).unwrap();

        let mut txn = env.begin_rw_txn().unwrap();
        txn.put(db, b"key1", b"val1", WriteFlags::empty()).unwrap();
        txn.put(db, b"key1", b"val2", WriteFlags::empty()).unwrap();
        txn.put(db, b"key1", b"val3", WriteFlags::empty()).unwrap();
        txn.put(db, b"key2", b"val1", WriteFlags::empty()).unwrap();
        txn.put(db, b"key2", b"val2", WriteFlags::empty()).unwrap();
        txn.put(db, b"key2", b"val3", WriteFlags::empty()).unwrap();

        let cursor = txn.open_ro_cursor(db).unwrap();
        assert_eq!((Some(&b"key1"[..]), &b"val1"[..]),
                   cursor.get(None, None, MDB_FIRST).unwrap());
        assert_eq!((None, &b"val1"[..]),
                   cursor.get(None, None, MDB_FIRST_DUP).unwrap());
        assert_eq!((Some(&b"key1"[..]), &b"val1"[..]),
                   cursor.get(None, None, MDB_GET_CURRENT).unwrap());
        assert_eq!((Some(&b"key2"[..]), &b"val1"[..]),
                   cursor.get(None, None, MDB_NEXT_NODUP).unwrap());
        assert_eq!((Some(&b"key2"[..]), &b"val2"[..]),
                   cursor.get(None, None, MDB_NEXT_DUP).unwrap());
        assert_eq!((Some(&b"key2"[..]), &b"val3"[..]),
                   cursor.get(None, None, MDB_NEXT_DUP).unwrap());
        assert!(cursor.get(None, None, MDB_NEXT_DUP).is_err());
        assert_eq!((Some(&b"key2"[..]), &b"val2"[..]),
                   cursor.get(None, None, MDB_PREV_DUP).unwrap());
        assert_eq!((None, &b"val3"[..]),
                   cursor.get(None, None, MDB_LAST_DUP).unwrap());
        assert_eq!((Some(&b"key1"[..]), &b"val3"[..]),
                   cursor.get(None, None, MDB_PREV_NODUP).unwrap());
        assert_eq!((None, &b"val1"[..]),
                   cursor.get(Some(&b"key1"[..]), None, MDB_SET).unwrap());
        assert_eq!((Some(&b"key2"[..]), &b"val1"[..]),
                   cursor.get(Some(&b"key2"[..]), None, MDB_SET_KEY).unwrap());
        assert_eq!((Some(&b"key2"[..]), &b"val1"[..]),
                   cursor.get(Some(&b"key1\0"[..]), None, MDB_SET_RANGE).unwrap());
        assert_eq!((None, &b"val3"[..]),
                   cursor.get(Some(&b"key1"[..]), Some(&b"val3"[..]), MDB_GET_BOTH).unwrap());
        assert_eq!((None, &b"val1"[..]),
                   cursor.get(Some(&b"key2"[..]), Some(&b"val"[..]), MDB_GET_BOTH_RANGE).unwrap());
    }

    #[test]
    fn test_get_dupfixed() {
        let dir = TempDir::new("test").unwrap();
        let env = Environment::new().open(dir.path()).unwrap();
        let db = env.create_db(None, DatabaseFlags::DUP_SORT | DatabaseFlags::DUP_FIXED).unwrap();

        let mut txn = env.begin_rw_txn().unwrap();
        txn.put(db, b"key1", b"val1", WriteFlags::empty()).unwrap();
        txn.put(db, b"key1", b"val2", WriteFlags::empty()).unwrap();
        txn.put(db, b"key1", b"val3", WriteFlags::empty()).unwrap();
        txn.put(db, b"key2", b"val4", WriteFlags::empty()).unwrap();
        txn.put(db, b"key2", b"val5", WriteFlags::empty()).unwrap();
        txn.put(db, b"key2", b"val6", WriteFlags::empty()).unwrap();

        let cursor = txn.open_ro_cursor(db).unwrap();
        assert_eq!((Some(&b"key1"[..]), &b"val1"[..]),
                   cursor.get(None, None, MDB_FIRST).unwrap());
        assert_eq!((None, &b"val1val2val3"[..]),
                   cursor.get(None, None, MDB_GET_MULTIPLE).unwrap());
        assert!(cursor.get(None, None, MDB_NEXT_MULTIPLE).is_err());
    }

    #[test]
    fn test_iter() {
        let dir = TempDir::new("test").unwrap();
        let env = Environment::new().open(dir.path()).unwrap();
        let db = env.open_db(None).unwrap();

        let items: Vec<(&[u8], &[u8])> = vec!((b"key1", b"val1"),
                                              (b"key2", b"val2"),
                                              (b"key3", b"val3"),
                                              (b"key5", b"val5"));

        {
            let mut txn = env.begin_rw_txn().unwrap();
            for &(ref key, ref data) in &items {
                txn.put(db, key, data, WriteFlags::empty()).unwrap();
            }
            txn.commit().unwrap();
        }

        let txn = env.begin_ro_txn().unwrap();
        let mut cursor = txn.open_ro_cursor(db).unwrap();
        assert_eq!(items, cursor.iter().collect::<Vec<_>>());

        cursor.get(Some(b"key2"), None, MDB_SET).unwrap();
        assert_eq!(items.clone().into_iter().skip(2).collect::<Vec<_>>(),
                   cursor.iter().collect::<Vec<_>>());

        assert_eq!(items, cursor.iter_start().collect::<Vec<_>>());

        assert_eq!(items.clone().into_iter().skip(1).collect::<Vec<_>>(),
                   cursor.iter_from(b"key2").collect::<Vec<_>>());

        assert_eq!(items.clone().into_iter().skip(3).collect::<Vec<_>>(),
                   cursor.iter_from(b"key4").collect::<Vec<_>>());

        assert_eq!(vec!().into_iter().collect::<Vec<(&[u8], &[u8])>>(),
                   cursor.iter_from(b"key6").collect::<Vec<_>>());
    }

    #[test]
    fn test_iter_dup() {
        let dir = TempDir::new("test").unwrap();
        let env = Environment::new().open(dir.path()).unwrap();
        let db = env.create_db(None, DatabaseFlags::DUP_SORT).unwrap();

        let items: Vec<(&[u8], &[u8])> = vec!((b"a", b"1"),
                                              (b"a", b"2"),
                                              (b"a", b"3"),
                                              (b"b", b"1"),
                                              (b"b", b"2"),
                                              (b"b", b"3"),
                                              (b"c", b"1"),
                                              (b"c", b"2"),
                                              (b"c", b"3"),
                                              (b"e", b"1"),
                                              (b"e", b"2"),
                                              (b"e", b"3"));

        {
            let mut txn = env.begin_rw_txn().unwrap();
            for &(ref key, ref data) in &items {
                txn.put(db, key, data, WriteFlags::empty()).unwrap();
            }
            txn.commit().unwrap();
        }

        let txn = env.begin_ro_txn().unwrap();
        let mut cursor = txn.open_ro_cursor(db).unwrap();
        assert_eq!(items, cursor.iter_dup().flat_map(|x| x).collect::<Vec<_>>());

        cursor.get(Some(b"b"), None, MDB_SET).unwrap();
        assert_eq!(items.clone().into_iter().skip(4).collect::<Vec<(&[u8], &[u8])>>(),
                   cursor.iter_dup().flat_map(|x| x).collect::<Vec<_>>());

        assert_eq!(items,
                   cursor.iter_dup_start().flat_map(|x| x).collect::<Vec<(&[u8], &[u8])>>());

        assert_eq!(items.clone().into_iter().skip(3).collect::<Vec<(&[u8], &[u8])>>(),
                   cursor.iter_dup_from(b"b").flat_map(|x| x).collect::<Vec<_>>());

        assert_eq!(items.clone().into_iter().skip(3).collect::<Vec<(&[u8], &[u8])>>(),
                   cursor.iter_dup_from(b"ab").flat_map(|x| x).collect::<Vec<_>>());

        assert_eq!(items.clone().into_iter().skip(9).collect::<Vec<(&[u8], &[u8])>>(),
                   cursor.iter_dup_from(b"d").flat_map(|x| x).collect::<Vec<_>>());

        assert_eq!(vec!().into_iter().collect::<Vec<(&[u8], &[u8])>>(),
                   cursor.iter_dup_from(b"f").flat_map(|x| x).collect::<Vec<_>>());

        assert_eq!(items.clone().into_iter().skip(3).take(3).collect::<Vec<(&[u8], &[u8])>>(),
                   cursor.iter_dup_of(b"b").unwrap().collect::<Vec<_>>());

        assert!(cursor.iter_dup_of(b"foo").is_err());
    }

    #[test]
    fn test_put_del() {
        let dir = TempDir::new("test").unwrap();
        let env = Environment::new().open(dir.path()).unwrap();
        let db = env.open_db(None).unwrap();

        let mut txn = env.begin_rw_txn().unwrap();
        let mut cursor = txn.open_rw_cursor(db).unwrap();

        cursor.put(b"key1", b"val1", WriteFlags::empty()).unwrap();
        cursor.put(b"key2", b"val2", WriteFlags::empty()).unwrap();
        cursor.put(b"key3", b"val3", WriteFlags::empty()).unwrap();

        assert_eq!((Some(&b"key3"[..]), &b"val3"[..]),
                   cursor.get(None, None, MDB_GET_CURRENT).unwrap());

        cursor.del(WriteFlags::empty()).unwrap();
        assert_eq!((Some(&b"key2"[..]), &b"val2"[..]),
                   cursor.get(None, None, MDB_LAST).unwrap());
    }

    /// Benchmark of iterator sequential read performance.
    #[bench]
    fn bench_get_seq_iter(b: &mut Bencher) {
        let n = 100;
        let (_dir, env) = setup_bench_db(n);
        let db = env.open_db(None).unwrap();
        let txn = env.begin_ro_txn().unwrap();

        b.iter(|| {
            let mut cursor = txn.open_ro_cursor(db).unwrap();
            let mut i = 0;
            let mut count = 0u32;

            for (key, data) in cursor.iter() {
                i = i + key.len() + data.len();
                count = count + 1;
            }

            black_box(i);
            assert_eq!(count, n);
        });
    }

    /// Benchmark of cursor sequential read performance.
    #[bench]
    fn bench_get_seq_cursor(b: &mut Bencher) {
        let n = 100;
        let (_dir, env) = setup_bench_db(n);
        let db = env.open_db(None).unwrap();
        let txn = env.begin_ro_txn().unwrap();

        b.iter(|| {
            let cursor = txn.open_ro_cursor(db).unwrap();
            let mut i = 0;
            let mut count = 0u32;

            while let Ok((key_opt, val)) = cursor.get(None, None, MDB_NEXT) {
                i += key_opt.map(|key| key.len()).unwrap_or(0) + val.len();
                count += 1;
            }

            black_box(i);
            assert_eq!(count, n);
        });
    }

    /// Benchmark of raw LMDB sequential read performance (control).
    #[bench]
    fn bench_get_seq_raw(b: &mut Bencher) {
        let n = 100;
        let (_dir, env) = setup_bench_db(n);
        let db = env.open_db(None).unwrap();

        let dbi: MDB_dbi = db.dbi();
        let _txn = env.begin_ro_txn().unwrap();
        let txn = _txn.txn();

        let mut key = MDB_val { mv_size: 0, mv_data: ptr::null_mut() };
        let mut data = MDB_val { mv_size: 0, mv_data: ptr::null_mut() };
        let mut cursor: *mut MDB_cursor = ptr::null_mut();

        b.iter(|| unsafe {
            mdb_cursor_open(txn, dbi, &mut cursor);
            let mut i = 0;
            let mut count = 0u32;

            while mdb_cursor_get(cursor, &mut key, &mut data, MDB_NEXT) == 0 {
                i += key.mv_size + data.mv_size;
                count += 1;
            };

            black_box(i);
            assert_eq!(count, n);
            mdb_cursor_close(cursor);
        });
    }
}
