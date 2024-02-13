// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
#![allow(dead_code)] // TODO: Get rid of unused struct members
#![allow(clippy::upper_case_acronyms)] // TODO: Consider renaming things like `BRANCH`

//! A utility for migrating data from one LMDB environment to another. Notably, this tool
//! can migrate data from an enviroment created with a different bit-depth than the
//! current rkv consumer, which enables the consumer to retrieve data from an environment
//! that can't be read directly using the rkv APIs.
//!
//! The utility supports both 32-bit and 64-bit LMDB source environments, and it
//! automatically migrates data in both the default database and any named (sub)
//! databases.  It also migrates the source environment's "map size" and "max DBs"
//! configuration options to the destination environment.
//!
//! The destination environment must be at the rkv consumer's bit depth and should be
//! empty of data.  It can be an empty directory, in which case the utility will create a
//! new LMDB environment within the directory.
//!
//! The tool currently has these limitations:
//!
//! 1. It doesn't support migration from environments created with
//!     `EnvironmentFlags::NO_SUB_DIR`.  To migrate such an environment, create a
//!     temporary directory, copy the environment's data file to a file called data.mdb in
//!     the temporary directory, then migrate the temporary directory as the source
//!     environment.
//! 2. It doesn't support migration from databases created with DatabaseFlags::DUP_SORT`
//!     (with or without `DatabaseFlags::DUP_FIXED`).
//! 3. It doesn't account for existing data in the destination environment, which means
//!     that it can overwrite data (causing data loss) or fail to migrate data if the
//!     destination environment contains existing data.
//!
//! ## Basic Usage
//!
//! Call `Migrator::new()` with the path to the source environment to create a `Migrator`
//! instance; then call the instance's `migrate()` method with the path to the destination
//! environment to migrate data from the source to the destination environment.  For
//! example, this snippet migrates data from the tests/envs/ref_env_32 environment to a
//! new environment in a temporary directory:
//!
//! ```
//! use rkv::migrator::LmdbArchMigrator as Migrator;
//! use std::path::Path;
//! use tempfile::tempdir;
//! let mut migrator = Migrator::new(Path::new("tests/envs/ref_env_32")).unwrap();
//! migrator.migrate(&tempdir().unwrap().path()).unwrap();
//! ```
//!
//! Both `Migrator::new()` and `migrate()` return a `MigrateResult` that is either an
//! `Ok()` result or an `Err<MigrateError>`, where `MigrateError` is an enum whose
//! variants identify specific kinds of migration failures.

use std::{
    collections::{BTreeMap, HashMap},
    convert::TryFrom,
    fs::File,
    io::{Cursor, Read, Seek, SeekFrom, Write},
    path::{Path, PathBuf},
    rc::Rc,
    str,
};

use bitflags::bitflags;
use byteorder::{LittleEndian, ReadBytesExt};
use lmdb::{DatabaseFlags, Environment, Transaction, WriteFlags};

pub use super::arch_migrator_error::MigrateError;

const PAGESIZE: u16 = 4096;

// The magic number is 0xBEEFC0DE, which is 0xDEC0EFBE in little-endian. It appears at
// offset 12 on 32-bit systems and 16 on 64-bit systems. We don't support big-endian
// migration, but presumably we could do so by detecting the order of the bytes.
const MAGIC: [u8; 4] = [0xDE, 0xC0, 0xEF, 0xBE];

pub type MigrateResult<T> = Result<T, MigrateError>;

bitflags! {
    #[derive(Default, PartialEq, Eq, Debug, Clone, Copy)]
    struct PageFlags: u16 {
        const BRANCH = 0x01;
        const LEAF = 0x02;
        const OVERFLOW = 0x04;
        const META = 0x08;
        const DIRTY = 0x10;
        const LEAF2 = 0x20;
        const SUBP = 0x40;
        const LOOSE = 0x4000;
        const KEEP = 0x8000;
    }
}

bitflags! {
    #[derive(Default, PartialEq, Eq, Debug, Clone, Copy)]
    struct NodeFlags: u16 {
        const BIGDATA = 0x01;
        const SUBDATA = 0x02;
        const DUPDATA = 0x04;
    }
}

// The bit depth of the executable that created an LMDB environment. The Migrator
// determines this automatically based on the location of the magic number in data.mdb.
#[derive(Clone, Copy, PartialEq)]
enum Bits {
    U32,
    U64,
}

impl Bits {
    // The size of usize for the bit-depth represented by the enum variant.
    fn size(self) -> usize {
        match self {
            Bits::U32 => 4,
            Bits::U64 => 8,
        }
    }
}

// The equivalent of PAGEHDRSZ in LMDB, except that this one varies by bits.
fn page_header_size(bits: Bits) -> u64 {
    match bits {
        Bits::U32 => 12,
        Bits::U64 => 16,
    }
}

// The equivalent of P_INVALID in LMDB, except that this one varies by bits.
fn validate_page_num(page_num: u64, bits: Bits) -> MigrateResult<()> {
    let invalid_page_num = match bits {
        Bits::U32 => u64::from(!0u32),
        Bits::U64 => !0u64,
    };

    if page_num == invalid_page_num {
        return Err(MigrateError::InvalidPageNum);
    }

    Ok(())
}

#[derive(Clone, Debug, Default)]
struct Database {
    md_pad: u32,
    md_flags: DatabaseFlags,
    md_depth: u16,
    md_branch_pages: u64,
    md_leaf_pages: u64,
    md_overflow_pages: u64,
    md_entries: u64,
    md_root: u64,
}

impl Database {
    fn new(cursor: &mut Cursor<&[u8]>, bits: Bits) -> MigrateResult<Database> {
        Ok(Database {
            md_pad: cursor.read_u32::<LittleEndian>()?,
            md_flags: DatabaseFlags::from_bits(cursor.read_u16::<LittleEndian>()?.into())
                .ok_or(MigrateError::InvalidDatabaseBits)?,
            md_depth: cursor.read_u16::<LittleEndian>()?,
            md_branch_pages: cursor.read_uint::<LittleEndian>(bits.size())?,
            md_leaf_pages: cursor.read_uint::<LittleEndian>(bits.size())?,
            md_overflow_pages: cursor.read_uint::<LittleEndian>(bits.size())?,
            md_entries: cursor.read_uint::<LittleEndian>(bits.size())?,
            md_root: cursor.read_uint::<LittleEndian>(bits.size())?,
        })
    }
}

#[derive(Debug, Default)]
struct Databases {
    free: Database,
    main: Database,
}

#[derive(Debug, Default)]
struct MetaData {
    mm_magic: u32,
    mm_version: u32,
    mm_address: u64,
    mm_mapsize: u64,
    mm_dbs: Databases,
    mm_last_pg: u64,
    mm_txnid: u64,
}

#[derive(Debug)]
enum LeafNode {
    Regular {
        mn_lo: u16,
        mn_hi: u16,
        mn_flags: NodeFlags,
        mn_ksize: u16,
        mv_size: u32,
        key: Vec<u8>,
        value: Vec<u8>,
    },
    BigData {
        mn_lo: u16,
        mn_hi: u16,
        mn_flags: NodeFlags,
        mn_ksize: u16,
        mv_size: u32,
        key: Vec<u8>,
        overflow_pgno: u64,
    },
    SubData {
        mn_lo: u16,
        mn_hi: u16,
        mn_flags: NodeFlags,
        mn_ksize: u16,
        mv_size: u32,
        key: Vec<u8>,
        value: Vec<u8>,
        db: Database,
    },
}

#[derive(Debug, Default)]
struct BranchNode {
    mp_pgno: u64,
    mn_ksize: u16,
    mn_data: Vec<u8>,
}

#[derive(Debug)]
enum PageHeader {
    Regular {
        mp_pgno: u64,
        mp_flags: PageFlags,
        pb_lower: u16,
        pb_upper: u16,
    },
    Overflow {
        mp_pgno: u64,
        mp_flags: PageFlags,
        pb_pages: u32,
    },
}

#[derive(Debug)]
enum Page {
    META(MetaData),
    LEAF(Vec<LeafNode>),
    BRANCH(Vec<BranchNode>),
}

impl Page {
    fn new(buf: Vec<u8>, bits: Bits) -> MigrateResult<Page> {
        let mut cursor = std::io::Cursor::new(&buf[..]);

        match Self::parse_page_header(&mut cursor, bits)? {
            PageHeader::Regular {
                mp_flags, pb_lower, ..
            } => {
                if mp_flags.contains(PageFlags::LEAF2) || mp_flags.contains(PageFlags::SUBP) {
                    // We don't yet support DUPFIXED and DUPSORT databases.
                    return Err(MigrateError::UnsupportedPageHeaderVariant);
                }

                if mp_flags.contains(PageFlags::META) {
                    let meta_data = Self::parse_meta_data(&mut cursor, bits)?;
                    Ok(Page::META(meta_data))
                } else if mp_flags.contains(PageFlags::LEAF) {
                    let nodes = Self::parse_leaf_nodes(&mut cursor, pb_lower, bits)?;
                    Ok(Page::LEAF(nodes))
                } else if mp_flags.contains(PageFlags::BRANCH) {
                    let nodes = Self::parse_branch_nodes(&mut cursor, pb_lower, bits)?;
                    Ok(Page::BRANCH(nodes))
                } else {
                    Err(MigrateError::UnexpectedPageHeaderVariant)
                }
            }
            PageHeader::Overflow { .. } => {
                // There isn't anything to do, nor should we try to instantiate
                // a page of this type, as we only access them when reading
                // a value that is too large to fit into a leaf node.
                Err(MigrateError::UnexpectedPageHeaderVariant)
            }
        }
    }

    fn parse_page_header(cursor: &mut Cursor<&[u8]>, bits: Bits) -> MigrateResult<PageHeader> {
        let mp_pgno = cursor.read_uint::<LittleEndian>(bits.size())?;
        let _mp_pad = cursor.read_u16::<LittleEndian>()?;
        let mp_flags = PageFlags::from_bits(cursor.read_u16::<LittleEndian>()?)
            .ok_or(MigrateError::InvalidPageBits)?;

        if mp_flags.contains(PageFlags::OVERFLOW) {
            let pb_pages = cursor.read_u32::<LittleEndian>()?;
            Ok(PageHeader::Overflow {
                mp_pgno,
                mp_flags,
                pb_pages,
            })
        } else {
            let pb_lower = cursor.read_u16::<LittleEndian>()?;
            let pb_upper = cursor.read_u16::<LittleEndian>()?;
            Ok(PageHeader::Regular {
                mp_pgno,
                mp_flags,
                pb_lower,
                pb_upper,
            })
        }
    }

    fn parse_meta_data(cursor: &mut Cursor<&[u8]>, bits: Bits) -> MigrateResult<MetaData> {
        cursor.seek(SeekFrom::Start(page_header_size(bits)))?;

        Ok(MetaData {
            mm_magic: cursor.read_u32::<LittleEndian>()?,
            mm_version: cursor.read_u32::<LittleEndian>()?,
            mm_address: cursor.read_uint::<LittleEndian>(bits.size())?,
            mm_mapsize: cursor.read_uint::<LittleEndian>(bits.size())?,
            mm_dbs: Databases {
                free: Database::new(cursor, bits)?,
                main: Database::new(cursor, bits)?,
            },
            mm_last_pg: cursor.read_uint::<LittleEndian>(bits.size())?,
            mm_txnid: cursor.read_uint::<LittleEndian>(bits.size())?,
        })
    }

    fn parse_leaf_nodes(
        cursor: &mut Cursor<&[u8]>,
        pb_lower: u16,
        bits: Bits,
    ) -> MigrateResult<Vec<LeafNode>> {
        cursor.set_position(page_header_size(bits));
        let num_keys = Self::num_keys(pb_lower, bits);
        let mp_ptrs = Self::parse_mp_ptrs(cursor, num_keys)?;

        let mut leaf_nodes = Vec::with_capacity(num_keys as usize);

        for mp_ptr in mp_ptrs {
            cursor.set_position(u64::from(mp_ptr));
            leaf_nodes.push(Self::parse_leaf_node(cursor, bits)?);
        }

        Ok(leaf_nodes)
    }

    fn parse_leaf_node(cursor: &mut Cursor<&[u8]>, bits: Bits) -> MigrateResult<LeafNode> {
        // The order of the mn_lo and mn_hi fields is endian-dependent and would be
        // reversed in an LMDB environment created on a big-endian system.
        let mn_lo = cursor.read_u16::<LittleEndian>()?;
        let mn_hi = cursor.read_u16::<LittleEndian>()?;

        let mn_flags = NodeFlags::from_bits(cursor.read_u16::<LittleEndian>()?)
            .ok_or(MigrateError::InvalidNodeBits)?;
        let mn_ksize = cursor.read_u16::<LittleEndian>()?;

        let start = usize::try_from(cursor.position())?;
        let end = usize::try_from(cursor.position() + u64::from(mn_ksize))?;
        let key = cursor.get_ref()[start..end].to_vec();
        cursor.set_position(end as u64);

        let mv_size = Self::leaf_node_size(mn_lo, mn_hi);
        if mn_flags.contains(NodeFlags::BIGDATA) {
            let overflow_pgno = cursor.read_uint::<LittleEndian>(bits.size())?;
            Ok(LeafNode::BigData {
                mn_lo,
                mn_hi,
                mn_flags,
                mn_ksize,
                mv_size,
                key,
                overflow_pgno,
            })
        } else if mn_flags.contains(NodeFlags::SUBDATA) {
            let start = usize::try_from(cursor.position())?;
            let end = usize::try_from(cursor.position() + u64::from(mv_size))?;
            let value = cursor.get_ref()[start..end].to_vec();
            let mut cursor = std::io::Cursor::new(&value[..]);
            let db = Database::new(&mut cursor, bits)?;
            validate_page_num(db.md_root, bits)?;
            Ok(LeafNode::SubData {
                mn_lo,
                mn_hi,
                mn_flags,
                mn_ksize,
                mv_size,
                key,
                value,
                db,
            })
        } else {
            let start = usize::try_from(cursor.position())?;
            let end = usize::try_from(cursor.position() + u64::from(mv_size))?;
            let value = cursor.get_ref()[start..end].to_vec();
            Ok(LeafNode::Regular {
                mn_lo,
                mn_hi,
                mn_flags,
                mn_ksize,
                mv_size,
                key,
                value,
            })
        }
    }

    fn leaf_node_size(mn_lo: u16, mn_hi: u16) -> u32 {
        u32::from(mn_lo) + ((u32::from(mn_hi)) << 16)
    }

    fn parse_branch_nodes(
        cursor: &mut Cursor<&[u8]>,
        pb_lower: u16,
        bits: Bits,
    ) -> MigrateResult<Vec<BranchNode>> {
        let num_keys = Self::num_keys(pb_lower, bits);
        let mp_ptrs = Self::parse_mp_ptrs(cursor, num_keys)?;

        let mut branch_nodes = Vec::with_capacity(num_keys as usize);

        for mp_ptr in mp_ptrs {
            cursor.set_position(u64::from(mp_ptr));
            branch_nodes.push(Self::parse_branch_node(cursor, bits)?)
        }

        Ok(branch_nodes)
    }

    fn parse_branch_node(cursor: &mut Cursor<&[u8]>, bits: Bits) -> MigrateResult<BranchNode> {
        // The order of the mn_lo and mn_hi fields is endian-dependent and would be
        // reversed in an LMDB environment created on a big-endian system.
        let mn_lo = cursor.read_u16::<LittleEndian>()?;
        let mn_hi = cursor.read_u16::<LittleEndian>()?;

        let mn_flags = cursor.read_u16::<LittleEndian>()?;

        // Branch nodes overload the mn_lo, mn_hi, and mn_flags fields to store the page
        // number, so we derive the number from those fields.
        let mp_pgno = Self::branch_node_page_num(mn_lo, mn_hi, mn_flags, bits);

        let mn_ksize = cursor.read_u16::<LittleEndian>()?;

        let position = cursor.position();
        let start = usize::try_from(position)?;
        let end = usize::try_from(position + u64::from(mn_ksize))?;
        let mn_data = cursor.get_ref()[start..end].to_vec();
        cursor.set_position(end as u64);

        Ok(BranchNode {
            mp_pgno,
            mn_ksize,
            mn_data,
        })
    }

    fn branch_node_page_num(mn_lo: u16, mn_hi: u16, mn_flags: u16, bits: Bits) -> u64 {
        let mut page_num = u64::from(u32::from(mn_lo) + (u32::from(mn_hi) << 16));
        if bits == Bits::U64 {
            page_num += u64::from(mn_flags) << 32;
        }
        page_num
    }

    fn parse_mp_ptrs(cursor: &mut Cursor<&[u8]>, num_keys: u64) -> MigrateResult<Vec<u16>> {
        let mut mp_ptrs = Vec::with_capacity(num_keys as usize);
        for _ in 0..num_keys {
            mp_ptrs.push(cursor.read_u16::<LittleEndian>()?);
        }
        Ok(mp_ptrs)
    }

    fn num_keys(pb_lower: u16, bits: Bits) -> u64 {
        (u64::from(pb_lower) - page_header_size(bits)) >> 1
    }
}

pub struct Migrator {
    file: File,
    bits: Bits,
}

impl Migrator {
    /// Create a new Migrator for the LMDB environment at the given path. This tries to
    /// open the data.mdb file in the environment and determine the bit depth of the
    /// executable that created it, so it can fail and return an Err if the file can't be
    /// opened or the depth determined.
    pub fn new(path: &Path) -> MigrateResult<Migrator> {
        let mut path = PathBuf::from(path);
        path.push("data.mdb");
        let mut file = File::open(&path)?;

        file.seek(SeekFrom::Start(page_header_size(Bits::U32)))?;
        let mut buf = [0; 4];
        file.read_exact(&mut buf)?;

        let bits = if buf == MAGIC {
            Bits::U32
        } else {
            file.seek(SeekFrom::Start(page_header_size(Bits::U64)))?;
            file.read_exact(&mut buf)?;
            if buf == MAGIC {
                Bits::U64
            } else {
                return Err(MigrateError::IndeterminateBitDepth);
            }
        };

        Ok(Migrator { file, bits })
    }

    /// Dump the data in one of the databases in the LMDB environment. If the `database`
    /// paremeter is None, then we dump the data in the main database.  If it's the name
    /// of a subdatabase, then we dump the data in that subdatabase.
    ///
    /// Note that the output isn't identical to that of the `mdb_dump` utility, since
    /// `mdb_dump` includes subdatabase key/value pairs when dumping the main database,
    /// and those values are architecture-dependent, since they contain pointer-sized
    /// data.
    ///
    /// If we wanted to support identical output, we could parameterize inclusion of
    /// subdatabase pairs in get_pairs() and include them when dumping data, while
    /// continuing to exclude them when migrating data.
    pub fn dump<T: Write>(&mut self, database: Option<&str>, mut out: T) -> MigrateResult<()> {
        let meta_data = self.get_meta_data()?;
        let root_page_num = meta_data.mm_dbs.main.md_root;
        let root_page = Rc::new(self.get_page(root_page_num)?);

        let pairs;
        if let Some(database) = database {
            let subdbs = self.get_subdbs(root_page)?;
            let database = subdbs
                .get(database.as_bytes())
                .ok_or_else(|| MigrateError::DatabaseNotFound(database.to_string()))?;
            let root_page_num = database.md_root;
            let root_page = Rc::new(self.get_page(root_page_num)?);
            pairs = self.get_pairs(root_page)?;
        } else {
            pairs = self.get_pairs(root_page)?;
        }

        out.write_all(b"VERSION=3\n")?;
        out.write_all(b"format=bytevalue\n")?;
        if let Some(database) = database {
            writeln!(out, "database={database}")?;
        }
        out.write_all(b"type=btree\n")?;
        writeln!(out, "mapsize={}", meta_data.mm_mapsize)?;
        out.write_all(b"maxreaders=126\n")?;
        out.write_all(b"db_pagesize=4096\n")?;
        out.write_all(b"HEADER=END\n")?;

        for (key, value) in pairs {
            out.write_all(b" ")?;
            for byte in key {
                write!(out, "{byte:02x}")?;
            }
            out.write_all(b"\n")?;
            out.write_all(b" ")?;
            for byte in value {
                write!(out, "{byte:02x}")?;
            }
            out.write_all(b"\n")?;
        }

        out.write_all(b"DATA=END\n")?;

        Ok(())
    }

    /// Migrate all data in all of databases in the existing LMDB environment to a new
    /// environment.  This includes all key/value pairs in the main database that aren't
    /// metadata about subdatabases and all key/value pairs in all subdatabases.
    ///
    /// We also set the map size and maximum databases of the new environment to their
    /// values for the existing environment.  But we don't set other metadata, and we
    /// don't check that the new environment is empty before migrating data.
    ///
    /// Thus it's possible for this to overwrite existing data or fail to migrate data if
    /// the new environment isn't empty.  It's the consumer's responsibility to ensure
    /// that data can be safely migrated to the new environment.  In general, this means
    /// that environment should be empty.
    pub fn migrate(&mut self, dest: &Path) -> MigrateResult<()> {
        let meta_data = self.get_meta_data()?;
        let root_page_num = meta_data.mm_dbs.main.md_root;
        validate_page_num(root_page_num, self.bits)?;
        let root_page = Rc::new(self.get_page(root_page_num)?);
        let subdbs = self.get_subdbs(Rc::clone(&root_page))?;

        let env = Environment::new()
            .set_map_size(meta_data.mm_mapsize as usize)
            .set_max_dbs(subdbs.len() as u32)
            .open(dest)?;

        // Create the databases before we open a read-write transaction, since database
        // creation requires its own read-write transaction, which would hang while
        // awaiting completion of an existing one.
        env.create_db(None, meta_data.mm_dbs.main.md_flags)?;
        for (subdb_name, subdb_info) in &subdbs {
            env.create_db(Some(str::from_utf8(subdb_name)?), subdb_info.md_flags)?;
        }

        // Now open the read-write transaction that we'll use to migrate all the data.
        let mut txn = env.begin_rw_txn()?;

        // Migrate the main database.
        let pairs = self.get_pairs(root_page)?;
        let db = env.open_db(None)?;
        for (key, value) in pairs {
            // If we knew that the target database was empty, we could specify
            // WriteFlags::APPEND to speed up the migration.
            txn.put(db, &key, &value, WriteFlags::empty())?;
        }

        // Migrate subdatabases.
        for (subdb_name, subdb_info) in &subdbs {
            let root_page = Rc::new(self.get_page(subdb_info.md_root)?);
            let pairs = self.get_pairs(root_page)?;
            let db = env.open_db(Some(str::from_utf8(subdb_name)?))?;
            for (key, value) in pairs {
                // If we knew that the target database was empty, we could specify
                // WriteFlags::APPEND to speed up the migration.
                txn.put(db, &key, &value, WriteFlags::empty())?;
            }
        }

        txn.commit()?;

        Ok(())
    }

    fn get_subdbs(&mut self, root_page: Rc<Page>) -> MigrateResult<HashMap<Vec<u8>, Database>> {
        let mut subdbs = HashMap::new();
        let mut pages = vec![root_page];

        while let Some(page) = pages.pop() {
            match &*page {
                Page::BRANCH(nodes) => {
                    for branch in nodes {
                        pages.push(Rc::new(self.get_page(branch.mp_pgno)?));
                    }
                }
                Page::LEAF(nodes) => {
                    for leaf in nodes {
                        if let LeafNode::SubData { key, db, .. } = leaf {
                            subdbs.insert(key.to_vec(), db.clone());
                        };
                    }
                }
                _ => {
                    return Err(MigrateError::UnexpectedPageVariant);
                }
            }
        }

        Ok(subdbs)
    }

    fn get_pairs(&mut self, root_page: Rc<Page>) -> MigrateResult<BTreeMap<Vec<u8>, Vec<u8>>> {
        let mut pairs = BTreeMap::new();
        let mut pages = vec![root_page];

        while let Some(page) = pages.pop() {
            match &*page {
                Page::BRANCH(nodes) => {
                    for branch in nodes {
                        pages.push(Rc::new(self.get_page(branch.mp_pgno)?));
                    }
                }
                Page::LEAF(nodes) => {
                    for leaf in nodes {
                        match leaf {
                            LeafNode::Regular { key, value, .. } => {
                                pairs.insert(key.to_vec(), value.to_vec());
                            }
                            LeafNode::BigData {
                                mv_size,
                                key,
                                overflow_pgno,
                                ..
                            } => {
                                // Perhaps we could reduce memory consumption during a
                                // migration by waiting to read big data until it's time
                                // to write it to the new database.
                                let value = self.read_data(
                                    *overflow_pgno * u64::from(PAGESIZE)
                                        + page_header_size(self.bits),
                                    *mv_size as usize,
                                )?;
                                pairs.insert(key.to_vec(), value);
                            }
                            LeafNode::SubData { .. } => {
                                // We don't include subdatabase leaves in pairs, since
                                // there's no architecture-neutral representation of them,
                                // and in any case they're meta-data that should get
                                // recreated when we migrate the subdatabases themselves.
                                //
                                // If we wanted to create identical dumps to those
                                // produced by `mdb_dump`, however, we could allow
                                // consumers to specify that they'd like to include these
                                // records.
                            }
                        };
                    }
                }
                _ => {
                    return Err(MigrateError::UnexpectedPageVariant);
                }
            }
        }

        Ok(pairs)
    }

    fn read_data(&mut self, offset: u64, size: usize) -> MigrateResult<Vec<u8>> {
        self.file.seek(SeekFrom::Start(offset))?;
        let mut buf: Vec<u8> = vec![0; size];
        self.file.read_exact(&mut buf[0..size])?;
        Ok(buf.to_vec())
    }

    fn get_page(&mut self, page_no: u64) -> MigrateResult<Page> {
        Page::new(
            self.read_data(page_no * u64::from(PAGESIZE), usize::from(PAGESIZE))?,
            self.bits,
        )
    }

    fn get_meta_data(&mut self) -> MigrateResult<MetaData> {
        let (page0, page1) = (self.get_page(0)?, self.get_page(1)?);

        match (page0, page1) {
            (Page::META(meta0), Page::META(meta1)) => {
                let meta = if meta1.mm_txnid > meta0.mm_txnid {
                    meta1
                } else {
                    meta0
                };
                if meta.mm_magic != 0xBE_EF_C0_DE {
                    return Err(MigrateError::InvalidMagicNum);
                }
                if meta.mm_version != 1 && meta.mm_version != 999 {
                    return Err(MigrateError::InvalidDataVersion);
                }
                Ok(meta)
            }
            _ => Err(MigrateError::UnexpectedPageVariant),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use std::{env, fs, mem::size_of};

    use lmdb::{Environment, Error as LmdbError};
    use tempfile::{tempdir, tempfile};

    fn compare_files(ref_file: &mut File, new_file: &mut File) -> MigrateResult<()> {
        ref_file.seek(SeekFrom::Start(0))?;
        new_file.seek(SeekFrom::Start(0))?;

        let ref_buf = &mut [0; 1024];
        let new_buf = &mut [0; 1024];

        loop {
            match ref_file.read(ref_buf) {
                Err(err) => panic!("{}", err),
                Ok(ref_len) => match new_file.read(new_buf) {
                    Err(err) => panic!("{}", err),
                    Ok(new_len) => {
                        assert_eq!(ref_len, new_len);
                        if ref_len == 0 {
                            break;
                        };
                        assert_eq!(ref_buf[0..ref_len], new_buf[0..new_len]);
                    }
                },
            }
        }

        Ok(())
    }

    #[test]
    fn test_dump_32() -> MigrateResult<()> {
        let cwd = env::current_dir()?;
        let cwd = cwd.to_str().ok_or(MigrateError::StringConversionError)?;
        let test_env_path: PathBuf = [cwd, "tests", "envs", "ref_env_32"].iter().collect();

        // Dump data from the test env to a new dump file.
        let mut migrator = Migrator::new(&test_env_path)?;
        let mut new_dump_file = tempfile()?;
        migrator.dump(None, &new_dump_file)?;

        // Open the reference dump file.
        let ref_dump_file_path: PathBuf = [cwd, "tests", "envs", "ref_dump.txt"].iter().collect();
        let mut ref_dump_file = File::open(ref_dump_file_path)?;

        // Compare the new dump file to the reference dump file.
        compare_files(&mut ref_dump_file, &mut new_dump_file)?;

        Ok(())
    }

    #[test]
    fn test_dump_32_subdb() -> MigrateResult<()> {
        let cwd = env::current_dir()?;
        let cwd = cwd.to_str().ok_or(MigrateError::StringConversionError)?;
        let test_env_path: PathBuf = [cwd, "tests", "envs", "ref_env_32"].iter().collect();

        // Dump data from the test env to a new dump file.
        let mut migrator = Migrator::new(&test_env_path)?;
        let mut new_dump_file = tempfile()?;
        migrator.dump(Some("subdb"), &new_dump_file)?;

        // Open the reference dump file.
        let ref_dump_file_path: PathBuf = [cwd, "tests", "envs", "ref_dump_subdb.txt"]
            .iter()
            .collect();
        let mut ref_dump_file = File::open(ref_dump_file_path)?;

        // Compare the new dump file to the reference dump file.
        compare_files(&mut ref_dump_file, &mut new_dump_file)?;

        Ok(())
    }

    #[test]
    fn test_dump_64() -> MigrateResult<()> {
        let cwd = env::current_dir()?;
        let cwd = cwd.to_str().ok_or(MigrateError::StringConversionError)?;
        let test_env_path: PathBuf = [cwd, "tests", "envs", "ref_env_64"].iter().collect();

        // Dump data from the test env to a new dump file.
        let mut migrator = Migrator::new(&test_env_path)?;
        let mut new_dump_file = tempfile()?;
        migrator.dump(None, &new_dump_file)?;

        // Open the reference dump file.
        let ref_dump_file_path: PathBuf = [cwd, "tests", "envs", "ref_dump.txt"].iter().collect();
        let mut ref_dump_file = File::open(ref_dump_file_path)?;

        // Compare the new dump file to the reference dump file.
        compare_files(&mut ref_dump_file, &mut new_dump_file)?;

        Ok(())
    }

    #[test]
    fn test_dump_64_subdb() -> MigrateResult<()> {
        let cwd = env::current_dir()?;
        let cwd = cwd.to_str().ok_or(MigrateError::StringConversionError)?;
        let test_env_path: PathBuf = [cwd, "tests", "envs", "ref_env_64"].iter().collect();

        // Dump data from the test env to a new dump file.
        let mut migrator = Migrator::new(&test_env_path)?;
        let mut new_dump_file = tempfile()?;
        migrator.dump(Some("subdb"), &new_dump_file)?;

        // Open the reference dump file.
        let ref_dump_file_path: PathBuf = [cwd, "tests", "envs", "ref_dump_subdb.txt"]
            .iter()
            .collect();
        let mut ref_dump_file = File::open(ref_dump_file_path)?;

        // Compare the new dump file to the reference dump file.
        compare_files(&mut ref_dump_file, &mut new_dump_file)?;

        Ok(())
    }

    #[test]
    fn test_migrate_64() -> MigrateResult<()> {
        let cwd = env::current_dir()?;
        let cwd = cwd.to_str().ok_or(MigrateError::StringConversionError)?;
        let test_env_path: PathBuf = [cwd, "tests", "envs", "ref_env_64"].iter().collect();

        // Migrate data from the old env to a new one.
        let new_env = tempdir()?;
        let mut migrator = Migrator::new(&test_env_path)?;
        migrator.migrate(new_env.path())?;

        // Dump data from the new env to a new dump file.
        let mut migrator = Migrator::new(new_env.path())?;
        let mut new_dump_file = tempfile()?;
        migrator.dump(Some("subdb"), &new_dump_file)?;

        // Open the reference dump file.
        let ref_dump_file_path: PathBuf = [cwd, "tests", "envs", "ref_dump_subdb.txt"]
            .iter()
            .collect();
        let mut ref_dump_file = File::open(ref_dump_file_path)?;

        // Compare the new dump file to the reference dump file.
        compare_files(&mut ref_dump_file, &mut new_dump_file)?;

        Ok(())
    }

    #[test]
    fn test_migrate_32() -> MigrateResult<()> {
        let cwd = env::current_dir()?;
        let cwd = cwd.to_str().ok_or(MigrateError::StringConversionError)?;
        let test_env_path: PathBuf = [cwd, "tests", "envs", "ref_env_32"].iter().collect();

        // Migrate data from the old env to a new one.
        let new_env = tempdir()?;
        let mut migrator = Migrator::new(&test_env_path)?;
        migrator.migrate(new_env.path())?;

        // Dump data from the new env to a new dump file.
        let mut migrator = Migrator::new(new_env.path())?;
        let mut new_dump_file = tempfile()?;
        migrator.dump(Some("subdb"), &new_dump_file)?;

        // Open the reference dump file.
        let ref_dump_file_path: PathBuf = [cwd, "tests", "envs", "ref_dump_subdb.txt"]
            .iter()
            .collect();
        let mut ref_dump_file = File::open(ref_dump_file_path)?;

        // Compare the new dump file to the reference dump file.
        compare_files(&mut ref_dump_file, &mut new_dump_file)?;

        Ok(())
    }

    #[test]
    fn test_migrate_and_replace() -> MigrateResult<()> {
        let test_env_name = match size_of::<usize>() {
            4 => "ref_env_64",
            8 => "ref_env_32",
            _ => panic!("only 32- and 64-bit depths are supported"),
        };

        let cwd = env::current_dir()?;
        let cwd = cwd.to_str().ok_or(MigrateError::StringConversionError)?;
        let test_env_path: PathBuf = [cwd, "tests", "envs", test_env_name].iter().collect();

        let old_env = tempdir()?;
        fs::copy(
            test_env_path.join("data.mdb"),
            old_env.path().join("data.mdb"),
        )?;
        fs::copy(
            test_env_path.join("lock.mdb"),
            old_env.path().join("lock.mdb"),
        )?;

        // Confirm that it isn't possible to open the old environment with LMDB.
        assert_eq!(
            match Environment::new().open(old_env.path()) {
                Err(err) => err,
                _ => panic!("opening the environment should have failed"),
            },
            LmdbError::Invalid
        );

        // Migrate data from the old env to a new one.
        let new_env = tempdir()?;
        let mut migrator = Migrator::new(old_env.path())?;
        migrator.migrate(new_env.path())?;

        // Dump data from the new env to a new dump file.
        let mut migrator = Migrator::new(new_env.path())?;
        let mut new_dump_file = tempfile()?;
        migrator.dump(Some("subdb"), &new_dump_file)?;

        // Open the reference dump file.
        let ref_dump_file_path: PathBuf = [cwd, "tests", "envs", "ref_dump_subdb.txt"]
            .iter()
            .collect();
        let mut ref_dump_file = File::open(ref_dump_file_path)?;

        // Compare the new dump file to the reference dump file.
        compare_files(&mut ref_dump_file, &mut new_dump_file)?;

        // Overwrite the old env's files with the new env's files and confirm that it's now
        // possible to open the old env with LMDB.
        fs::copy(
            new_env.path().join("data.mdb"),
            old_env.path().join("data.mdb"),
        )?;
        fs::copy(
            new_env.path().join("lock.mdb"),
            old_env.path().join("lock.mdb"),
        )?;
        assert!(Environment::new().open(old_env.path()).is_ok());

        Ok(())
    }
}
