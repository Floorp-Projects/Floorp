/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate byteorder;
#[macro_use]
extern crate cstr;
extern crate firefox_on_glean;
#[macro_use]
extern crate log;
#[macro_use]
extern crate malloc_size_of_derive;
extern crate moz_task;
extern crate nserror;
extern crate thin_vec;
extern crate wr_malloc_size_of;
#[macro_use]
extern crate xpcom;

use wr_malloc_size_of as malloc_size_of;

use byteorder::{BigEndian, ReadBytesExt, WriteBytesExt};
use firefox_on_glean::metrics::data_storage;
use malloc_size_of::{MallocSizeOf, MallocSizeOfOps};
use moz_task::{create_background_task_queue, RunnableBuilder};
use nserror::{
    nsresult, NS_ERROR_FAILURE, NS_ERROR_ILLEGAL_INPUT, NS_ERROR_INVALID_ARG,
    NS_ERROR_NOT_AVAILABLE, NS_OK,
};
use nsstring::{nsACString, nsAString, nsCStr, nsCString, nsString};
use thin_vec::ThinVec;
use xpcom::interfaces::{
    nsIDataStorage, nsIDataStorageItem, nsIFile, nsIHandleReportCallback, nsIMemoryReporter,
    nsIMemoryReporterManager, nsIObserverService, nsIProperties, nsISerialEventTarget, nsISupports,
};
use xpcom::{xpcom_method, RefPtr, XpCom};

use std::collections::HashMap;
use std::ffi::CStr;
use std::fs::{File, OpenOptions};
use std::io::{BufRead, BufReader, ErrorKind, Read, Seek, SeekFrom, Write};
use std::os::raw::{c_char, c_void};
use std::path::PathBuf;
use std::sync::{Condvar, Mutex};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

/// Helper type for turning the nsIDataStorage::DataType "enum" into a rust
/// enum.
#[derive(Copy, Clone, Eq, PartialEq)]
enum DataType {
    Persistent,
    Private,
}

impl From<u8> for DataType {
    fn from(value: u8) -> Self {
        match value {
            nsIDataStorage::Persistent => DataType::Persistent,
            nsIDataStorage::Private => DataType::Private,
            _ => panic!("invalid nsIDataStorage::DataType"),
        }
    }
}

impl From<DataType> for u8 {
    fn from(value: DataType) -> Self {
        match value {
            DataType::Persistent => nsIDataStorage::Persistent,
            DataType::Private => nsIDataStorage::Private,
        }
    }
}

/// Returns the current day in days since the unix epoch, to a maximum of
/// u16::MAX days.
fn now_in_days() -> u16 {
    const SECONDS_PER_DAY: u64 = 60 * 60 * 24;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or(Duration::ZERO);
    (now.as_secs() / SECONDS_PER_DAY)
        .try_into()
        .unwrap_or(u16::MAX)
}

/// An entry in some DataStorageTable.
#[derive(Clone, MallocSizeOf)]
struct Entry {
    /// The number of unique days this Entry has been accessed on.
    score: u16,
    /// The number of days since the unix epoch this Entry was last accessed.
    last_accessed: u16,
    /// The key.
    key: Vec<u8>,
    /// The value.
    value: Vec<u8>,
    /// The slot index of this Entry.
    slot_index: usize,
}

impl Entry {
    /// Constructs an Entry given a line of text from the old DataStorage format.
    fn from_old_line(line: &str, slot_index: usize, value_length: usize) -> Result<Self, nsresult> {
        // the old format is <key>\t<score>\t<last accessed>\t<value>
        let parts: Vec<&str> = line.split('\t').collect();
        if parts.len() != 4 {
            return Err(NS_ERROR_ILLEGAL_INPUT);
        }
        let score = parts[1]
            .parse::<u16>()
            .map_err(|_| NS_ERROR_ILLEGAL_INPUT)?;
        let last_accessed = parts[2]
            .parse::<u16>()
            .map_err(|_| NS_ERROR_ILLEGAL_INPUT)?;
        let key = Vec::from(parts[0]);
        if key.len() > KEY_LENGTH {
            return Err(NS_ERROR_ILLEGAL_INPUT);
        }
        let value = Vec::from(parts[3]);
        if value.len() > value_length {
            return Err(NS_ERROR_ILLEGAL_INPUT);
        }
        Ok(Entry {
            score,
            last_accessed,
            key,
            value,
            slot_index,
        })
    }

    /// Constructs an Entry given the parsed parts from the current format.
    fn from_slot(
        score: u16,
        last_accessed: u16,
        key: Vec<u8>,
        value: Vec<u8>,
        slot_index: usize,
    ) -> Self {
        Entry {
            score,
            last_accessed,
            key,
            value,
            slot_index,
        }
    }

    /// Constructs a new Entry given a key, value, and index.
    fn new(key: Vec<u8>, value: Vec<u8>, slot_index: usize) -> Self {
        Entry {
            score: 1,
            last_accessed: now_in_days(),
            key,
            value,
            slot_index,
        }
    }

    /// Constructs a new, empty `Entry` with the given index. Useful for clearing
    /// slots on disk.
    fn new_empty(slot_index: usize) -> Self {
        Entry {
            score: 0,
            last_accessed: 0,
            key: Vec::new(),
            value: Vec::new(),
            slot_index,
        }
    }

    /// Returns whether or not this is an empty `Entry` (an empty `Entry` has
    /// been created with `Entry::new_empty()` or cleared with
    /// `Entry::clear()`, has 0 `score` and `last_accessed`, and has an empty
    /// `key` and `value`.
    fn is_empty(&self) -> bool {
        self.score == 0 && self.last_accessed == 0 && self.key.is_empty() && self.value.is_empty()
    }

    /// If this Entry was last accessed on a day different from today,
    /// increments its score (as well as its last accessed day).
    /// Returns `true` if the score did in fact change, and `false` otherwise.
    fn update_score(&mut self) -> bool {
        let now_in_days = now_in_days();
        if self.last_accessed != now_in_days {
            self.last_accessed = now_in_days;
            self.score += 1;
            true
        } else {
            false
        }
    }

    /// Clear the data stored in this Entry. Useful for clearing a single slot
    /// on disk.
    fn clear(&mut self) {
        // Note: it's important that this preserves slot_index - the writer
        // needs it to know where to write out the zeroed Entry
        *self = Self::new_empty(self.slot_index);
    }
}

/// Strips all trailing 0 bytes from the end of the given vec.
/// Useful for converting 0-padded keys and values to their original, non-padded
/// state.
fn strip_zeroes(vec: &mut Vec<u8>) {
    let mut length = vec.len();
    while length > 0 && vec[length - 1] == 0 {
        length -= 1;
    }
    vec.truncate(length);
}

/// Given a slice of entries, returns a Vec<Entry> consisting of each Entry
/// with score equal to the minimum score among all entries.
fn get_entries_with_minimum_score(entries: &[Entry]) -> Vec<&Entry> {
    let mut min_score = u16::MAX;
    let mut min_score_entries = Vec::new();
    for entry in entries.iter() {
        if entry.score < min_score {
            min_score = entry.score;
            min_score_entries.clear();
        }
        if entry.score == min_score {
            min_score_entries.push(entry);
        }
    }
    min_score_entries
}

const MAX_SLOTS: usize = 2048;
const KEY_LENGTH: usize = 256;

/// Helper type to map between an entry key and the slot it is stored on.
type DataStorageTable = HashMap<Vec<u8>, usize>;

/// The main structure of this implementation. Keeps track of persistent
/// and private entries.
#[derive(MallocSizeOf)]
struct DataStorageInner {
    /// The key to slot index mapping table for persistent data.
    persistent_table: DataStorageTable,
    /// The persistent entries that are stored on disk.
    persistent_slots: Vec<Entry>,
    /// The key to slot index mapping table for private, temporary data.
    private_table: DataStorageTable,
    /// The private, temporary entries that are not stored on disk.
    /// This data is cleared upon observing "last-pb-context-exited", and is
    /// forgotten when the program shuts down.
    private_slots: Vec<Entry>,
    /// The name of the table (e.g. "SiteSecurityServiceState").
    name: String,
    /// The maximum permitted length of values.
    value_length: usize,
    /// A PathBuf holding the location of the profile directory, if available.
    maybe_profile_path: Option<PathBuf>,
    /// A serial event target to post tasks to, to write out changed persistent
    /// data in the background.
    #[ignore_malloc_size_of = "not implemented for nsISerialEventTarget"]
    write_queue: Option<RefPtr<nsISerialEventTarget>>,
}

impl DataStorageInner {
    fn new(
        name: String,
        value_length: usize,
        maybe_profile_path: Option<PathBuf>,
    ) -> Result<Self, nsresult> {
        Ok(DataStorageInner {
            persistent_table: DataStorageTable::new(),
            persistent_slots: Vec::new(),
            private_table: DataStorageTable::new(),
            private_slots: Vec::new(),
            name,
            value_length,
            maybe_profile_path,
            write_queue: Some(create_background_task_queue(cstr!("data_storage"))?),
        })
    }

    /// Initializes the DataStorageInner. If the profile directory is not
    /// present, does nothing. If the backing file is available, processes it.
    /// Otherwise, if the old backing file is available, migrates it to the
    /// current format.
    fn initialize(&mut self) -> Result<(), nsresult> {
        let Some(profile_path) = self.maybe_profile_path.as_ref() else {
            return Ok(());
        };
        let mut backing_path = profile_path.clone();
        backing_path.push(format!("{}.bin", &self.name));
        let mut old_backing_path = profile_path.clone();
        old_backing_path.push(format!("{}.txt", &self.name));
        if backing_path.exists() {
            self.read(backing_path)
        } else if old_backing_path.exists() {
            self.read_old_format(old_backing_path)
        } else {
            Ok(())
        }
    }

    /// Reads the backing file, processing each slot.
    fn read(&mut self, path: PathBuf) -> Result<(), nsresult> {
        let f = OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .open(path)
            .map_err(|_| NS_ERROR_FAILURE)?;
        let mut backing_file = BufReader::new(f);
        let mut slots = Vec::new();
        // First read each entry into the persistent slots list.
        while slots.len() < MAX_SLOTS {
            if let Some(entry) = self.process_slot(&mut backing_file, slots.len())? {
                slots.push(entry);
            } else {
                break;
            }
        }
        self.persistent_slots = slots;
        // Then build the key -> slot index lookup table.
        self.persistent_table = self
            .persistent_slots
            .iter()
            .filter(|slot| !slot.is_empty())
            .map(|slot| (slot.key.clone(), slot.slot_index))
            .collect();
        let num_entries = self.persistent_table.len() as i64;
        match self.name.as_str() {
            "AlternateServices" => data_storage::alternate_services.set(num_entries),
            "ClientAuthRememberList" => data_storage::client_auth_remember_list.set(num_entries),
            "SiteSecurityServiceState" => {
                data_storage::site_security_service_state.set(num_entries)
            }
            _ => panic!("unknown nsIDataStorageManager::DataStorage"),
        }
        Ok(())
    }

    /// Processes a slot (via a reader) by reading its metadata, key, and
    /// value. If the checksum fails or if the score or last accessed fields
    /// are 0, this is an empty slot. Otherwise, un-0-pads the key and value,
    /// creates a new Entry, and puts it in the persistent table.
    fn process_slot<R: Read>(
        &mut self,
        reader: &mut R,
        slot_index: usize,
    ) -> Result<Option<Entry>, nsresult> {
        // Format is [checksum][score][last accessed][key][value], where
        // checksum is 2 bytes big-endian, score and last accessed are 2 bytes
        // big-endian, key is KEY_LENGTH bytes (currently 256), and value is
        // self.value_length bytes (1024 for most instances, but 24 for
        // SiteSecurityServiceState - see DataStorageManager::Get).
        let mut checksum = match reader.read_u16::<BigEndian>() {
            Ok(checksum) => checksum,
            // The file may be shorter than expected due to unoccupied slots.
            // Every slot after the last read slot is unoccupied.
            Err(e) if e.kind() == ErrorKind::UnexpectedEof => return Ok(None),
            Err(_) => return Err(NS_ERROR_FAILURE),
        };
        let score = reader
            .read_u16::<BigEndian>()
            .map_err(|_| NS_ERROR_FAILURE)?;
        checksum ^= score;
        let last_accessed = reader
            .read_u16::<BigEndian>()
            .map_err(|_| NS_ERROR_FAILURE)?;
        checksum ^= last_accessed;

        let mut key = vec![0u8; KEY_LENGTH];
        reader.read_exact(&mut key).map_err(|_| NS_ERROR_FAILURE)?;
        for mut chunk in key.chunks(2) {
            checksum ^= chunk
                .read_u16::<BigEndian>()
                .map_err(|_| NS_ERROR_FAILURE)?;
        }
        strip_zeroes(&mut key);
        let mut value = vec![0u8; self.value_length];
        reader
            .read_exact(&mut value)
            .map_err(|_| NS_ERROR_FAILURE)?;
        for mut chunk in value.chunks(2) {
            checksum ^= chunk
                .read_u16::<BigEndian>()
                .map_err(|_| NS_ERROR_FAILURE)?;
        }
        strip_zeroes(&mut value);

        // If this slot is incomplete, corrupted, or empty, treat it as empty.
        if checksum != 0 || score == 0 || last_accessed == 0 {
            // This slot is empty.
            return Ok(Some(Entry::new_empty(slot_index)));
        }

        Ok(Some(Entry::from_slot(
            score,
            last_accessed,
            key,
            value,
            slot_index,
        )))
    }

    /// Migrates from the old format to the current format.
    fn read_old_format(&mut self, path: PathBuf) -> Result<(), nsresult> {
        let file = File::open(path).map_err(|_| NS_ERROR_FAILURE)?;
        let reader = BufReader::new(file);
        // First read each line in the old file into the persistent slots list.
        // The old format was limited to 1024 lines, so only expect that many.
        for line in reader.lines().flatten().take(1024) {
            match Entry::from_old_line(&line, self.persistent_slots.len(), self.value_length) {
                Ok(entry) => {
                    if self.persistent_slots.len() >= MAX_SLOTS {
                        warn!("too many lines in old DataStorage format");
                        break;
                    }
                    if !entry.is_empty() {
                        self.persistent_slots.push(entry);
                    } else {
                        warn!("empty entry in old DataStorage format?");
                    }
                }
                Err(_) => {
                    warn!("failed to migrate a line from old DataStorage format");
                }
            }
        }
        // Then build the key -> slot index lookup table.
        self.persistent_table = self
            .persistent_slots
            .iter()
            .filter(|slot| !slot.is_empty())
            .map(|slot| (slot.key.clone(), slot.slot_index))
            .collect();
        // Finally, write out the migrated data to the new backing file.
        self.async_write_entries(self.persistent_slots.clone())?;
        let num_entries = self.persistent_table.len() as i64;
        match self.name.as_str() {
            "AlternateServices" => data_storage::alternate_services.set(num_entries),
            "ClientAuthRememberList" => data_storage::client_auth_remember_list.set(num_entries),
            "SiteSecurityServiceState" => {
                data_storage::site_security_service_state.set(num_entries)
            }
            _ => panic!("unknown nsIDataStorageManager::DataStorage"),
        }
        Ok(())
    }

    /// Given an `Entry` and `DataType`, this function updates the internal
    /// list of slots and the mapping from keys to slot indices. If the slot
    /// assigned to the `Entry` is already occupied, the existing `Entry` is
    /// evicted.
    /// After updating internal state, if the type of this entry is persistent,
    /// this function dispatches an event to asynchronously write the data out.
    fn put_internal(&mut self, entry: Entry, type_: DataType) -> Result<(), nsresult> {
        let (table, slots) = self.get_table_and_slots_for_type_mut(type_);
        if entry.slot_index < slots.len() {
            let entry_to_evict = &slots[entry.slot_index];
            if !entry_to_evict.is_empty() {
                table.remove(&entry_to_evict.key);
            }
        }
        let _ = table.insert(entry.key.clone(), entry.slot_index);
        if entry.slot_index < slots.len() {
            slots[entry.slot_index] = entry.clone();
        } else if entry.slot_index == slots.len() {
            slots.push(entry.clone());
        } else {
            panic!(
                "put_internal should not have been given an Entry with slot_index > slots.len()"
            );
        }
        if type_ == DataType::Persistent {
            self.async_write_entry(entry)?;
        }
        Ok(())
    }

    /// Returns the total length of each slot on disk.
    fn slot_length(&self) -> usize {
        // Checksum is 2 bytes, and score and last accessed are 2 bytes each.
        2 + 2 + 2 + KEY_LENGTH + self.value_length
    }

    /// Gets the next free slot index, or determines a slot to evict (but
    /// doesn't actually perform the eviction - the caller must do that).
    fn get_free_slot_or_slot_to_evict(&self, type_: DataType) -> usize {
        let (_, slots) = self.get_table_and_slots_for_type(type_);
        let maybe_unoccupied_slot = slots
            .iter()
            .enumerate()
            .find(|(_, maybe_empty_entry)| maybe_empty_entry.is_empty());
        if let Some((unoccupied_slot, _)) = maybe_unoccupied_slot {
            return unoccupied_slot;
        }
        // If `slots` isn't full, the next free slot index is one more than the
        // current last index.
        if slots.len() < MAX_SLOTS {
            return slots.len();
        }
        // If there isn't an unoccupied slot, evict the entry with the lowest score.
        let min_score_entries = get_entries_with_minimum_score(&slots);
        // `min_score_entry` is the oldest Entry with the minimum score.
        // There must be at least one such Entry, so unwrap it or abort.
        let min_score_entry = min_score_entries
            .iter()
            .min_by_key(|e| e.last_accessed)
            .unwrap();
        min_score_entry.slot_index
    }

    /// Helper function to get a handle on the slot list and key to slot index
    /// mapping for the given `DataType`.
    fn get_table_and_slots_for_type(&self, type_: DataType) -> (&DataStorageTable, &[Entry]) {
        match type_ {
            DataType::Persistent => (&self.persistent_table, &self.persistent_slots),
            DataType::Private => (&self.private_table, &self.private_slots),
        }
    }

    /// Helper function to get a mutable handle on the slot list and key to
    /// slot index mapping for the given `DataType`.
    fn get_table_and_slots_for_type_mut(
        &mut self,
        type_: DataType,
    ) -> (&mut DataStorageTable, &mut Vec<Entry>) {
        match type_ {
            DataType::Persistent => (&mut self.persistent_table, &mut self.persistent_slots),
            DataType::Private => (&mut self.private_table, &mut self.private_slots),
        }
    }

    /// Helper function to look up an `Entry` by its key and type.
    fn get_entry(&mut self, key: &[u8], type_: DataType) -> Option<&mut Entry> {
        let (table, slots) = self.get_table_and_slots_for_type_mut(type_);
        let slot_index = table.get(key)?;
        Some(&mut slots[*slot_index])
    }

    /// Gets a value by key, if available. Updates the Entry's score when appropriate.
    fn get(&mut self, key: &[u8], type_: DataType) -> Result<Vec<u8>, nsresult> {
        let Some(entry) = self.get_entry(key, type_) else {
            return Err(NS_ERROR_NOT_AVAILABLE);
        };
        let value = entry.value.clone();
        if entry.update_score() && type_ == DataType::Persistent {
            let entry = entry.clone();
            self.async_write_entry(entry)?;
        }
        Ok(value)
    }

    /// Inserts or updates a value by key. Updates the Entry's score if applicable.
    fn put(&mut self, key: Vec<u8>, value: Vec<u8>, type_: DataType) -> Result<(), nsresult> {
        if key.len() > KEY_LENGTH || value.len() > self.value_length {
            return Err(NS_ERROR_INVALID_ARG);
        }
        if let Some(existing_entry) = self.get_entry(&key, type_) {
            let data_changed = existing_entry.value != value;
            if data_changed {
                existing_entry.value = value;
            }
            if (existing_entry.update_score() || data_changed) && type_ == DataType::Persistent {
                let entry = existing_entry.clone();
                self.async_write_entry(entry)?;
            }
            Ok(())
        } else {
            let slot_index = self.get_free_slot_or_slot_to_evict(type_);
            let entry = Entry::new(key.clone(), value, slot_index);
            self.put_internal(entry, type_)
        }
    }

    /// Removes an Entry by key, if it is present.
    fn remove(&mut self, key: &Vec<u8>, type_: DataType) -> Result<(), nsresult> {
        let (table, slots) = self.get_table_and_slots_for_type_mut(type_);
        let Some(slot_index) = table.remove(key) else {
            return Ok(());
        };
        let entry = &mut slots[slot_index];
        entry.clear();
        if type_ == DataType::Persistent {
            let entry = entry.clone();
            self.async_write_entry(entry)?;
        }
        Ok(())
    }

    /// Clears all tables and the backing persistent file.
    fn clear(&mut self) -> Result<(), nsresult> {
        self.persistent_table.clear();
        self.private_table.clear();
        self.persistent_slots.clear();
        self.private_slots.clear();
        let Some(profile_path) = self.maybe_profile_path.clone() else {
            return Ok(());
        };
        let Some(write_queue) = self.write_queue.clone() else {
            return Ok(());
        };
        let name = self.name.clone();
        RunnableBuilder::new("data_storage::remove_backing_files", move || {
            let old_backing_path = profile_path.join(format!("{name}.txt"));
            let _ = std::fs::remove_file(old_backing_path);
            let backing_path = profile_path.join(format!("{name}.bin"));
            let _ = std::fs::remove_file(backing_path);
        })
        .may_block(true)
        .dispatch(write_queue.coerce())
    }

    /// Clears only data in the private table.
    fn clear_private_data(&mut self) {
        self.private_table.clear();
        self.private_slots.clear();
    }

    /// Asynchronously writes the given entry on the background serial event
    /// target.
    fn async_write_entry(&self, entry: Entry) -> Result<(), nsresult> {
        self.async_write_entries(vec![entry])
    }

    /// Asynchronously writes the given entries on the background serial event
    /// target.
    fn async_write_entries(&self, entries: Vec<Entry>) -> Result<(), nsresult> {
        let Some(mut backing_path) = self.maybe_profile_path.clone() else {
            return Ok(());
        };
        let Some(write_queue) = self.write_queue.clone() else {
            return Ok(());
        };
        backing_path.push(format!("{}.bin", &self.name));
        let value_length = self.value_length;
        let slot_length = self.slot_length();
        RunnableBuilder::new("data_storage::write_entries", move || {
            let _ = write_entries(entries, backing_path, value_length, slot_length);
        })
        .may_block(true)
        .dispatch(write_queue.coerce())
    }

    /// Drop the write queue to prevent further writes.
    fn drop_write_queue(&mut self) {
        let _ = self.write_queue.take();
    }

    /// Takes a callback that is run for each entry in each table.
    fn for_each<F>(&self, mut f: F)
    where
        F: FnMut(&Entry, DataType),
    {
        for entry in &self.persistent_slots {
            f(entry, DataType::Persistent);
        }
        for entry in &self.private_slots {
            f(entry, DataType::Private);
        }
    }

    /// Collects the memory used by this DataStorageInner.
    fn collect_reports(
        &self,
        ops: &mut MallocSizeOfOps,
        callback: &nsIHandleReportCallback,
        data: Option<&nsISupports>,
    ) -> Result<(), nsresult> {
        let size = self.size_of(ops);
        let data = match data {
            Some(data) => data as *const nsISupports,
            None => std::ptr::null() as *const nsISupports,
        };
        unsafe {
            callback
                .Callback(
                    &nsCStr::new() as &nsACString,
                    &nsCString::from(format!("explicit/data-storage/{}", self.name)) as &nsACString,
                    nsIMemoryReporter::KIND_HEAP,
                    nsIMemoryReporter::UNITS_BYTES,
                    size as i64,
                    &nsCStr::from("Memory used by PSM data storage cache") as &nsACString,
                    data,
                )
                .to_result()
        }
    }
}

#[xpcom(implement(nsIDataStorageItem), atomic)]
struct DataStorageItem {
    key: nsCString,
    value: nsCString,
    type_: u8,
}

impl DataStorageItem {
    xpcom_method!(get_key => GetKey() -> nsACString);
    fn get_key(&self) -> Result<nsCString, nsresult> {
        Ok(self.key.clone())
    }

    xpcom_method!(get_value => GetValue() -> nsACString);
    fn get_value(&self) -> Result<nsCString, nsresult> {
        Ok(self.value.clone())
    }

    xpcom_method!(get_type => GetType() -> u8);
    fn get_type(&self) -> Result<u8, nsresult> {
        Ok(self.type_)
    }
}

type VoidPtrToSizeFn = unsafe extern "C" fn(ptr: *const c_void) -> usize;

/// Helper struct that coordinates xpcom access to the DataStorageInner that
/// actually holds the data.
#[xpcom(implement(nsIDataStorage, nsIMemoryReporter, nsIObserver), atomic)]
struct DataStorage {
    ready: (Mutex<bool>, Condvar),
    data: Mutex<DataStorageInner>,
    size_of_op: VoidPtrToSizeFn,
    enclosing_size_of_op: VoidPtrToSizeFn,
}

impl DataStorage {
    xpcom_method!(get => Get(key: *const nsACString, type_: u8) -> nsACString);
    fn get(&self, key: &nsACString, type_: u8) -> Result<nsCString, nsresult> {
        self.wait_for_ready()?;
        let mut storage = self.data.lock().unwrap();
        storage
            .get(&Vec::from(key.as_ref()), type_.into())
            .map(|data| nsCString::from(data))
    }

    xpcom_method!(put => Put(key: *const nsACString, value: *const nsACString, type_: u8));
    fn put(&self, key: &nsACString, value: &nsACString, type_: u8) -> Result<(), nsresult> {
        self.wait_for_ready()?;
        let mut storage = self.data.lock().unwrap();
        storage.put(
            Vec::from(key.as_ref()),
            Vec::from(value.as_ref()),
            type_.into(),
        )
    }

    xpcom_method!(remove => Remove(key: *const nsACString, type_: u8));
    fn remove(&self, key: &nsACString, type_: u8) -> Result<(), nsresult> {
        self.wait_for_ready()?;
        let mut storage = self.data.lock().unwrap();
        storage.remove(&Vec::from(key.as_ref()), type_.into())?;
        Ok(())
    }

    xpcom_method!(clear => Clear());
    fn clear(&self) -> Result<(), nsresult> {
        self.wait_for_ready()?;
        let mut storage = self.data.lock().unwrap();
        storage.clear()?;
        Ok(())
    }

    xpcom_method!(is_ready => IsReady() -> bool);
    fn is_ready(&self) -> Result<bool, nsresult> {
        let ready = self.ready.0.lock().unwrap();
        Ok(*ready)
    }

    xpcom_method!(get_all => GetAll() -> ThinVec<Option<RefPtr<nsIDataStorageItem>>>);
    fn get_all(&self) -> Result<ThinVec<Option<RefPtr<nsIDataStorageItem>>>, nsresult> {
        self.wait_for_ready()?;
        let storage = self.data.lock().unwrap();
        let mut items = ThinVec::new();
        let add_item = |entry: &Entry, data_type: DataType| {
            let item = DataStorageItem::allocate(InitDataStorageItem {
                key: entry.key.clone().into(),
                value: entry.value.clone().into(),
                type_: data_type.into(),
            });
            items.push(Some(RefPtr::new(item.coerce())));
        };
        storage.for_each(add_item);
        Ok(items)
    }

    fn indicate_ready(&self) -> Result<(), nsresult> {
        let (ready_mutex, condvar) = &self.ready;
        let mut ready = ready_mutex.lock().unwrap();
        *ready = true;
        condvar.notify_all();
        Ok(())
    }

    fn wait_for_ready(&self) -> Result<(), nsresult> {
        let (ready_mutex, condvar) = &self.ready;
        let mut ready = ready_mutex.lock().unwrap();
        while !*ready {
            ready = condvar.wait(ready).unwrap();
        }
        Ok(())
    }

    fn initialize(&self) -> Result<(), nsresult> {
        let mut storage = self.data.lock().unwrap();
        // If this fails, the implementation is "ready", but it probably won't
        // store any data persistently. This is expected in cases where there
        // is no profile directory.
        let _ = storage.initialize();
        self.indicate_ready()
    }

    xpcom_method!(collect_reports => CollectReports(callback: *const nsIHandleReportCallback, data: *const nsISupports, anonymize: bool));
    fn collect_reports(
        &self,
        callback: &nsIHandleReportCallback,
        data: Option<&nsISupports>,
        _anonymize: bool,
    ) -> Result<(), nsresult> {
        let storage = self.data.lock().unwrap();
        let mut ops = MallocSizeOfOps::new(self.size_of_op, Some(self.enclosing_size_of_op));
        storage.collect_reports(&mut ops, callback, data)
    }

    xpcom_method!(observe => Observe(_subject: *const nsISupports, topic: *const c_char, _data: *const u16));
    unsafe fn observe(
        &self,
        _subject: Option<&nsISupports>,
        topic: *const c_char,
        _data: *const u16,
    ) -> Result<(), nsresult> {
        let mut storage = self.data.lock().unwrap();
        let topic = CStr::from_ptr(topic);
        // Observe shutdown - prevent any further writes.
        // The backing file is in the profile directory, so stop writing when
        // that goes away.
        // "xpcom-shutdown-threads" is a backstop for situations where the
        // "profile-before-change" notification is not emitted.
        if topic == cstr!("profile-before-change") || topic == cstr!("xpcom-shutdown-threads") {
            storage.drop_write_queue();
        } else if topic == cstr!("last-pb-context-exited") {
            storage.clear_private_data();
        }
        Ok(())
    }
}

/// Given some entries, the path of the backing file, and metadata about Entry
/// length, writes an Entry to the backing file in the appropriate slot.
/// Creates the backing file if it does not exist.
fn write_entries(
    entries: Vec<Entry>,
    backing_path: PathBuf,
    value_length: usize,
    slot_length: usize,
) -> Result<(), std::io::Error> {
    let mut backing_file = OpenOptions::new()
        .write(true)
        .create(true)
        .open(backing_path)?;
    let Some(max_slot_index) = entries.iter().map(|entry| entry.slot_index).max() else {
        return Ok(()); // can only happen if entries is empty
    };
    let necessary_len = ((max_slot_index + 1) * slot_length) as u64;
    if backing_file.metadata()?.len() < necessary_len {
        backing_file.set_len(necessary_len)?;
    }
    let mut buf = vec![0u8; slot_length];
    for entry in entries {
        let mut buf_writer = buf.as_mut_slice();
        buf_writer.write_u16::<BigEndian>(0)?; // set checksum to 0 for now
        let mut checksum = entry.score;
        buf_writer.write_u16::<BigEndian>(entry.score)?;
        checksum ^= entry.last_accessed;
        buf_writer.write_u16::<BigEndian>(entry.last_accessed)?;
        for mut chunk in entry.key.chunks(2) {
            if chunk.len() == 1 {
                checksum ^= (chunk[0] as u16) << 8;
            } else {
                checksum ^= chunk.read_u16::<BigEndian>()?;
            }
        }
        if entry.key.len() > KEY_LENGTH {
            continue;
        }
        buf_writer.write_all(&entry.key)?;
        let (key_remainder, mut buf_writer) = buf_writer.split_at_mut(KEY_LENGTH - entry.key.len());
        key_remainder.fill(0);
        for mut chunk in entry.value.chunks(2) {
            if chunk.len() == 1 {
                checksum ^= (chunk[0] as u16) << 8;
            } else {
                checksum ^= chunk.read_u16::<BigEndian>()?;
            }
        }
        if entry.value.len() > value_length {
            continue;
        }
        buf_writer.write_all(&entry.value)?;
        buf_writer.fill(0);

        backing_file.seek(SeekFrom::Start((entry.slot_index * slot_length) as u64))?;
        backing_file.write_all(&buf)?;
        backing_file.flush()?;
        backing_file.seek(SeekFrom::Start((entry.slot_index * slot_length) as u64))?;
        backing_file.write_u16::<BigEndian>(checksum)?;
    }
    Ok(())
}

/// Uses the xpcom directory service to try to obtain the profile directory.
fn get_profile_path() -> Result<PathBuf, nsresult> {
    let directory_service: RefPtr<nsIProperties> =
        xpcom::components::Directory::service().map_err(|_| NS_ERROR_FAILURE)?;
    let mut profile_dir = xpcom::GetterAddrefs::<nsIFile>::new();
    unsafe {
        directory_service
            .Get(
                cstr!("ProfD").as_ptr(),
                &nsIFile::IID,
                profile_dir.void_ptr(),
            )
            .to_result()?;
    }
    let profile_dir = profile_dir.refptr().ok_or(NS_ERROR_FAILURE)?;
    let mut profile_path = nsString::new();
    unsafe {
        (*profile_dir).GetPath(&mut *profile_path).to_result()?;
    }
    let profile_path = String::from_utf16(profile_path.as_ref()).map_err(|_| NS_ERROR_FAILURE)?;
    Ok(PathBuf::from(profile_path))
}

fn make_data_storage_internal(
    basename: &str,
    value_length: usize,
    size_of_op: VoidPtrToSizeFn,
    enclosing_size_of_op: VoidPtrToSizeFn,
) -> Result<RefPtr<nsIDataStorage>, nsresult> {
    let maybe_profile_path = get_profile_path().ok();
    let data_storage = DataStorage::allocate(InitDataStorage {
        ready: (Mutex::new(false), Condvar::new()),
        data: Mutex::new(DataStorageInner::new(
            basename.to_string(),
            value_length,
            maybe_profile_path,
        )?),
        size_of_op,
        enclosing_size_of_op,
    });
    // Initialize the DataStorage on a background thread.
    let data_storage_for_background_initialization = data_storage.clone();
    RunnableBuilder::new("data_storage::initialize", move || {
        let _ = data_storage_for_background_initialization.initialize();
    })
    .may_block(true)
    .dispatch_background_task()?;

    // Observe shutdown and when the last private browsing context exits.
    if let Ok(observer_service) = xpcom::components::Observer::service::<nsIObserverService>() {
        unsafe {
            observer_service
                .AddObserver(
                    data_storage.coerce(),
                    cstr!("profile-before-change").as_ptr(),
                    false,
                )
                .to_result()?;
            observer_service
                .AddObserver(
                    data_storage.coerce(),
                    cstr!("xpcom-shutdown-threads").as_ptr(),
                    false,
                )
                .to_result()?;
            observer_service
                .AddObserver(
                    data_storage.coerce(),
                    cstr!("last-pb-context-exited").as_ptr(),
                    false,
                )
                .to_result()?;
        }
    }

    // Register the DataStorage as a memory reporter.
    if let Some(memory_reporter_manager) = xpcom::get_service::<nsIMemoryReporterManager>(cstr!(
        "@mozilla.org/memory-reporter-manager;1"
    )) {
        unsafe {
            memory_reporter_manager
                .RegisterStrongReporter(data_storage.coerce())
                .to_result()?;
        }
    }

    Ok(RefPtr::new(data_storage.coerce()))
}

#[no_mangle]
pub unsafe extern "C" fn make_data_storage(
    basename: *const nsAString,
    value_length: usize,
    size_of_op: VoidPtrToSizeFn,
    enclosing_size_of_op: VoidPtrToSizeFn,
    result: *mut *const xpcom::interfaces::nsIDataStorage,
) -> nsresult {
    if basename.is_null() || result.is_null() {
        return NS_ERROR_INVALID_ARG;
    }
    let basename = &*basename;
    let basename = basename.to_string();
    match make_data_storage_internal(&basename, value_length, size_of_op, enclosing_size_of_op) {
        Ok(val) => val.forget(&mut *result),
        Err(e) => return e,
    }
    NS_OK
}
