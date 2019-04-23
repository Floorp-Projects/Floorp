/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate xpcom;

use crossbeam_utils::atomic::AtomicCell;
use error::KeyValueError;
use moz_task::Task;
use nserror::{nsresult, NS_ERROR_FAILURE};
use nsstring::nsCString;
use owned_value::owned_to_variant;
use rkv::{OwnedValue, Rkv, SingleStore, StoreError, StoreOptions, Value};
use std::{
    path::Path,
    str,
    sync::{Arc, RwLock},
};
use storage_variant::VariantType;
use xpcom::{
    interfaces::{
        nsIKeyValueDatabaseCallback, nsIKeyValueEnumeratorCallback, nsIKeyValueVariantCallback,
        nsIKeyValueVoidCallback, nsIThread, nsIVariant,
    },
    RefPtr, ThreadBoundRefPtr,
};
use KeyValueDatabase;
use KeyValueEnumerator;
use KeyValuePairResult;
use manager::Manager;

/// A macro to generate a done() implementation for a Task.
/// Takes one argument that specifies the type of the Task's callback function:
///   value: a callback function that takes a value
///   void: the callback function doesn't take a value
///
/// The "value" variant calls self.convert() to convert a successful result
/// into the value to pass to the callback function.  So if you generate done()
/// for a callback that takes a value, ensure you also implement convert()!
macro_rules! task_done {
    (value) => {
        fn done(&self) -> Result<(), nsresult> {
            // If TaskRunnable.run() calls Task.done() to return a result
            // on the main thread before TaskRunnable.run() returns on the database
            // thread, then the Task will get dropped on the database thread.
            //
            // But the callback is an nsXPCWrappedJS that isn't safe to release
            // on the database thread.  So we move it out of the Task here to ensure
            // it gets released on the main thread.
            let threadbound = self.callback.swap(None).ok_or(NS_ERROR_FAILURE)?;
            let callback = threadbound.get_ref().ok_or(NS_ERROR_FAILURE)?;

            match self.result.swap(None) {
                Some(Ok(value)) => unsafe { callback.Resolve(self.convert(value)?.coerce()) },
                Some(Err(err)) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
                None => unsafe { callback.Reject(&*nsCString::from("unexpected")) },
            }.to_result()
        }
    };

    (void) => {
        fn done(&self) -> Result<(), nsresult> {
            // If TaskRunnable.run() calls Task.done() to return a result
            // on the main thread before TaskRunnable.run() returns on the database
            // thread, then the Task will get dropped on the database thread.
            //
            // But the callback is an nsXPCWrappedJS that isn't safe to release
            // on the database thread.  So we move it out of the Task here to ensure
            // it gets released on the main thread.
            let threadbound = self.callback.swap(None).ok_or(NS_ERROR_FAILURE)?;
            let callback = threadbound.get_ref().ok_or(NS_ERROR_FAILURE)?;

            match self.result.swap(None) {
                Some(Ok(())) => unsafe { callback.Resolve() },
                Some(Err(err)) => unsafe { callback.Reject(&*nsCString::from(err.to_string())) },
                None => unsafe { callback.Reject(&*nsCString::from("unexpected")) },
            }.to_result()
        }
    };
}

/// A tuple comprising an Arc<RwLock<Rkv>> and a SingleStore, which is
/// the result of GetOrCreateTask.  We declare this type because otherwise
/// Clippy complains "error: very complex type used. Consider factoring
/// parts into `type` definitions" (i.e. clippy::type-complexity) when we
/// declare the type of `GetOrCreateTask::result`.
type RkvStoreTuple = (Arc<RwLock<Rkv>>, SingleStore);

pub struct GetOrCreateTask {
    callback: AtomicCell<Option<ThreadBoundRefPtr<nsIKeyValueDatabaseCallback>>>,
    thread: AtomicCell<Option<ThreadBoundRefPtr<nsIThread>>>,
    path: nsCString,
    name: nsCString,
    result: AtomicCell<Option<Result<RkvStoreTuple, KeyValueError>>>,
}

impl GetOrCreateTask {
    pub fn new(
        callback: RefPtr<nsIKeyValueDatabaseCallback>,
        thread: RefPtr<nsIThread>,
        path: nsCString,
        name: nsCString,
    ) -> GetOrCreateTask {
        GetOrCreateTask {
            callback: AtomicCell::new(Some(ThreadBoundRefPtr::new(callback))),
            thread: AtomicCell::new(Some(ThreadBoundRefPtr::new(thread))),
            path,
            name,
            result: AtomicCell::default(),
        }
    }

    fn convert(&self, result: RkvStoreTuple) -> Result<RefPtr<KeyValueDatabase>, KeyValueError> {
        let thread = self.thread.swap(None).ok_or(NS_ERROR_FAILURE)?;
        Ok(KeyValueDatabase::new(result.0, result.1, thread))
    }
}

impl Task for GetOrCreateTask {
    fn run(&self) {
        // We do the work within a closure that returns a Result so we can
        // use the ? operator to simplify the implementation.
        self.result
            .store(Some(|| -> Result<RkvStoreTuple, KeyValueError> {
                let mut writer = Manager::singleton().write()?;
                let rkv = writer.get_or_create(Path::new(str::from_utf8(&self.path)?), Rkv::new)?;
                let store = rkv
                    .write()?
                    .open_single(str::from_utf8(&self.name)?, StoreOptions::create())?;
                Ok((rkv, store))
            }()));
    }

    task_done!(value);
}

pub struct PutTask {
    callback: AtomicCell<Option<ThreadBoundRefPtr<nsIKeyValueVoidCallback>>>,
    rkv: Arc<RwLock<Rkv>>,
    store: SingleStore,
    key: nsCString,
    value: OwnedValue,
    result: AtomicCell<Option<Result<(), KeyValueError>>>,
}

impl PutTask {
    pub fn new(
        callback: RefPtr<nsIKeyValueVoidCallback>,
        rkv: Arc<RwLock<Rkv>>,
        store: SingleStore,
        key: nsCString,
        value: OwnedValue,
    ) -> PutTask {
        PutTask {
            callback: AtomicCell::new(Some(ThreadBoundRefPtr::new(callback))),
            rkv,
            store,
            key,
            value,
            result: AtomicCell::default(),
        }
    }
}

impl Task for PutTask {
    fn run(&self) {
        // We do the work within a closure that returns a Result so we can
        // use the ? operator to simplify the implementation.
        self.result.store(Some(|| -> Result<(), KeyValueError> {
            let key = str::from_utf8(&self.key)?;
            let env = self.rkv.read()?;
            let mut writer = env.write()?;

            self.store
                .put(&mut writer, key, &Value::from(&self.value))?;
            writer.commit()?;

            Ok(())
        }()));
    }

    task_done!(void);
}

pub struct WriteManyTask {
    callback: AtomicCell<Option<ThreadBoundRefPtr<nsIKeyValueVoidCallback>>>,
    rkv: Arc<RwLock<Rkv>>,
    store: SingleStore,
    pairs: Vec<(nsCString, Option<OwnedValue>)>,
    result: AtomicCell<Option<Result<(), KeyValueError>>>,
}

impl WriteManyTask {
    pub fn new(
        callback: RefPtr<nsIKeyValueVoidCallback>,
        rkv: Arc<RwLock<Rkv>>,
        store: SingleStore,
        pairs: Vec<(nsCString, Option<OwnedValue>)>,
    ) -> WriteManyTask {
        WriteManyTask {
            callback: AtomicCell::new(Some(ThreadBoundRefPtr::new(callback))),
            rkv,
            store,
            pairs,
            result: AtomicCell::default(),
        }
    }
}

impl Task for WriteManyTask {
    fn run(&self) {
        // We do the work within a closure that returns a Result so we can
        // use the ? operator to simplify the implementation.
        self.result.store(Some(|| -> Result<(), KeyValueError> {
            let env = self.rkv.read()?;
            let mut writer = env.write()?;

            for (key, value) in self.pairs.iter() {
                let key = str::from_utf8(key)?;
                match value {
                    Some(val) => self.store.put(&mut writer, key, &Value::from(val))?,
                    None => {
                        match self.store.delete(&mut writer, key) {
                            Ok(_) => (),

                            // LMDB fails with an error if the key to delete wasn't found,
                            // and Rkv returns that error, but we ignore it, as we expect most
                            // of our consumers to want this behavior.
                            Err(StoreError::LmdbError(lmdb::Error::NotFound)) => (),

                            Err(err) => return Err(KeyValueError::StoreError(err)),
                        };
                    }
                }
            }
            writer.commit()?;

            Ok(())
        }()));
    }

    task_done!(void);
}

pub struct GetTask {
    callback: AtomicCell<Option<ThreadBoundRefPtr<nsIKeyValueVariantCallback>>>,
    rkv: Arc<RwLock<Rkv>>,
    store: SingleStore,
    key: nsCString,
    default_value: Option<OwnedValue>,
    result: AtomicCell<Option<Result<Option<OwnedValue>, KeyValueError>>>,
}

impl GetTask {
    pub fn new(
        callback: RefPtr<nsIKeyValueVariantCallback>,
        rkv: Arc<RwLock<Rkv>>,
        store: SingleStore,
        key: nsCString,
        default_value: Option<OwnedValue>,
    ) -> GetTask {
        GetTask {
            callback: AtomicCell::new(Some(ThreadBoundRefPtr::new(callback))),
            rkv,
            store,
            key,
            default_value,
            result: AtomicCell::default(),
        }
    }

    fn convert(&self, result: Option<OwnedValue>) -> Result<RefPtr<nsIVariant>, KeyValueError> {
        Ok(match result {
            Some(val) => owned_to_variant(val)?,
            None => ().into_variant(),
        })
    }
}

impl Task for GetTask {
    fn run(&self) {
        // We do the work within a closure that returns a Result so we can
        // use the ? operator to simplify the implementation.
        self.result
            .store(Some(|| -> Result<Option<OwnedValue>, KeyValueError> {
                let key = str::from_utf8(&self.key)?;
                let env = self.rkv.read()?;
                let reader = env.read()?;
                let value = self.store.get(&reader, key)?;

                Ok(match value {
                    Some(value) => Some(OwnedValue::from(&value)),
                    None => match self.default_value {
                        Some(ref val) => Some(val.clone()),
                        None => None,
                    },
                })
            }()));
    }

    task_done!(value);
}

pub struct HasTask {
    callback: AtomicCell<Option<ThreadBoundRefPtr<nsIKeyValueVariantCallback>>>,
    rkv: Arc<RwLock<Rkv>>,
    store: SingleStore,
    key: nsCString,
    result: AtomicCell<Option<Result<bool, KeyValueError>>>,
}

impl HasTask {
    pub fn new(
        callback: RefPtr<nsIKeyValueVariantCallback>,
        rkv: Arc<RwLock<Rkv>>,
        store: SingleStore,
        key: nsCString,
    ) -> HasTask {
        HasTask {
            callback: AtomicCell::new(Some(ThreadBoundRefPtr::new(callback))),
            rkv,
            store,
            key,
            result: AtomicCell::default(),
        }
    }

    fn convert(&self, result: bool) -> Result<RefPtr<nsIVariant>, KeyValueError> {
        Ok(result.into_variant())
    }
}

impl Task for HasTask {
    fn run(&self) {
        // We do the work within a closure that returns a Result so we can
        // use the ? operator to simplify the implementation.
        self.result.store(Some(|| -> Result<bool, KeyValueError> {
            let key = str::from_utf8(&self.key)?;
            let env = self.rkv.read()?;
            let reader = env.read()?;
            let value = self.store.get(&reader, key)?;
            Ok(value.is_some())
        }()));
    }

    task_done!(value);
}

pub struct DeleteTask {
    callback: AtomicCell<Option<ThreadBoundRefPtr<nsIKeyValueVoidCallback>>>,
    rkv: Arc<RwLock<Rkv>>,
    store: SingleStore,
    key: nsCString,
    result: AtomicCell<Option<Result<(), KeyValueError>>>,
}

impl DeleteTask {
    pub fn new(
        callback: RefPtr<nsIKeyValueVoidCallback>,
        rkv: Arc<RwLock<Rkv>>,
        store: SingleStore,
        key: nsCString,
    ) -> DeleteTask {
        DeleteTask {
            callback: AtomicCell::new(Some(ThreadBoundRefPtr::new(callback))),
            rkv,
            store,
            key,
            result: AtomicCell::default(),
        }
    }
}

impl Task for DeleteTask {
    fn run(&self) {
        // We do the work within a closure that returns a Result so we can
        // use the ? operator to simplify the implementation.
        self.result.store(Some(|| -> Result<(), KeyValueError> {
            let key = str::from_utf8(&self.key)?;
            let env = self.rkv.read()?;
            let mut writer = env.write()?;

            match self.store.delete(&mut writer, key) {
                Ok(_) => (),

                // LMDB fails with an error if the key to delete wasn't found,
                // and Rkv returns that error, but we ignore it, as we expect most
                // of our consumers to want this behavior.
                Err(StoreError::LmdbError(lmdb::Error::NotFound)) => (),

                Err(err) => return Err(KeyValueError::StoreError(err)),
            };

            writer.commit()?;

            Ok(())
        }()));
    }

    task_done!(void);
}

pub struct ClearTask {
    callback: AtomicCell<Option<ThreadBoundRefPtr<nsIKeyValueVoidCallback>>>,
    rkv: Arc<RwLock<Rkv>>,
    store: SingleStore,
    result: AtomicCell<Option<Result<(), KeyValueError>>>,
}

impl ClearTask {
    pub fn new(
        callback: RefPtr<nsIKeyValueVoidCallback>,
        rkv: Arc<RwLock<Rkv>>,
        store: SingleStore,
    ) -> ClearTask {
        ClearTask {
            callback: AtomicCell::new(Some(ThreadBoundRefPtr::new(callback))),
            rkv,
            store,
            result: AtomicCell::default(),
        }
    }
}

impl Task for ClearTask {
    fn run(&self) {
        // We do the work within a closure that returns a Result so we can
        // use the ? operator to simplify the implementation.
        self.result.store(Some(|| -> Result<(), KeyValueError> {
            let env = self.rkv.read()?;
            let mut writer = env.write()?;
            self.store.clear(&mut writer)?;
            writer.commit()?;

            Ok(())
        }()));
    }

    task_done!(void);
}

pub struct EnumerateTask {
    callback: AtomicCell<Option<ThreadBoundRefPtr<nsIKeyValueEnumeratorCallback>>>,
    rkv: Arc<RwLock<Rkv>>,
    store: SingleStore,
    from_key: nsCString,
    to_key: nsCString,
    result: AtomicCell<Option<Result<Vec<KeyValuePairResult>, KeyValueError>>>,
}

impl EnumerateTask {
    pub fn new(
        callback: RefPtr<nsIKeyValueEnumeratorCallback>,
        rkv: Arc<RwLock<Rkv>>,
        store: SingleStore,
        from_key: nsCString,
        to_key: nsCString,
    ) -> EnumerateTask {
        EnumerateTask {
            callback: AtomicCell::new(Some(ThreadBoundRefPtr::new(callback))),
            rkv,
            store,
            from_key,
            to_key,
            result: AtomicCell::default(),
        }
    }

    fn convert(
        &self,
        result: Vec<KeyValuePairResult>,
    ) -> Result<RefPtr<KeyValueEnumerator>, KeyValueError> {
        Ok(KeyValueEnumerator::new(result))
    }
}

impl Task for EnumerateTask {
    fn run(&self) {
        // We do the work within a closure that returns a Result so we can
        // use the ? operator to simplify the implementation.
        self.result.store(Some(
            || -> Result<Vec<KeyValuePairResult>, KeyValueError> {
                let env = self.rkv.read()?;
                let reader = env.read()?;
                let from_key = str::from_utf8(&self.from_key)?;
                let to_key = str::from_utf8(&self.to_key)?;

                let iterator = if from_key.is_empty() {
                    self.store.iter_start(&reader)?
                } else {
                    self.store.iter_from(&reader, &from_key)?
                };

                // Ideally, we'd enumerate pairs lazily, as the consumer calls
                // nsIKeyValueEnumerator.getNext(), which calls our
                // KeyValueEnumerator.get_next() implementation.  But KeyValueEnumerator
                // can't reference the Iter because Rust "cannot #[derive(xpcom)]
                // on a generic type," and the Iter requires a lifetime parameter,
                // which would make KeyValueEnumerator generic.
                //
                // Our fallback approach is to eagerly collect the iterator
                // into a collection that KeyValueEnumerator owns.  Fixing this so we
                // enumerate pairs lazily is bug 1499252.
                let pairs: Vec<KeyValuePairResult> = iterator
                    // Convert the key to a string so we can compare it to the "to" key.
                    // For forward compatibility, we don't fail here if we can't convert
                    // a key to UTF-8.  Instead, we store the Err in the collection
                    // and fail lazily in KeyValueEnumerator.get_next().
                    .map(|result| match result {
                        Ok((key, val)) => Ok((str::from_utf8(&key), val)),
                        Err(err) => Err(err),
                    })
                    // Stop iterating once we reach the to_key, if any.
                    .take_while(|result| match result {
                        Ok((key, _val)) => {
                            if to_key.is_empty() {
                                true
                            } else {
                                match *key {
                                    Ok(key) => key < to_key,
                                    Err(_err) => true,
                                }
                            }
                        }
                        Err(_) => true,
                    })
                    // Convert the key/value pair to owned.
                    .map(|result| match result {
                        Ok((key, val)) => match (key, val) {
                            (Ok(key), Some(val)) => Ok((key.to_owned(), OwnedValue::from(&val))),
                            (Err(err), _) => Err(err.into()),
                            (_, None) => Err(KeyValueError::UnexpectedValue),
                        },
                        Err(err) => Err(KeyValueError::StoreError(err)),
                    })
                    .collect();

                Ok(pairs)
            }(),
        ));
    }

    task_done!(value);
}
