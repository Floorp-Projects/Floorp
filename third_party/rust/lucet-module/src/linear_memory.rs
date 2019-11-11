use crate::Error;
use serde::{Deserialize, Serialize};

/// Specification of the linear memory of a module
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LinearMemorySpec<'a> {
    /// Specification of the heap used to implement the linear memory
    pub heap: HeapSpec,
    /// Initialization values for linear memory
    #[serde(borrow)]
    pub initializer: SparseData<'a>,
}

/// Specification of the linear memory of a module
///
/// This is a version of [`LinearMemorySpec`](../struct.LinearMemorySpec.html) with an
/// `OwnedSparseData` for the initializer.
/// This type is useful when directly building up a value to be serialized.
pub struct OwnedLinearMemorySpec {
    /// Specification of the heap used to implement the linear memory
    pub heap: HeapSpec,
    /// Initialization values for linear memory
    pub initializer: OwnedSparseData,
}

impl OwnedLinearMemorySpec {
    pub fn to_ref<'a>(&'a self) -> LinearMemorySpec<'a> {
        LinearMemorySpec {
            heap: self.heap.clone(),
            initializer: self.initializer.to_ref(),
        }
    }
}

/// Specifications about the heap of a Lucet module.
#[derive(Debug, Default, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct HeapSpec {
    /// Total bytes of memory for the heap to possibly expand into, as configured for Cranelift
    /// codegen.
    ///
    /// All of this memory is addressable. Only some part of it is accessible - from 0 to the
    /// initial size, guaranteed, and up to the `max_size`.  This size allows Cranelift to elide
    /// checks of the *base pointer*. At the moment that just means checking if it is greater than
    /// 4gb, in which case it can elide the base pointer check completely. In the future, Cranelift
    /// could use a solver to elide more base pointer checks if it can prove the calculation will
    /// always be less than this bound.
    ///
    /// Specified in bytes, and must be evenly divisible by the host page size (4K).
    pub reserved_size: u64,

    /// Total bytes of memory *after* the reserved area, as configured for Cranelift codegen.
    ///
    /// All of this memory is addressable, but it is never accessible - it is guaranteed to trap if
    /// an access happens in this region. This size allows Cranelift to use *common subexpression
    /// elimination* to reduce checks of the *sum of base pointer and offset* (where the offset is
    /// always rounded up to a multiple of the guard size, to be friendly to CSE).
    ///
    /// Specified in bytes, and must be evenly divisible by the host page size (4K).
    pub guard_size: u64,

    /// Total bytes of memory for the WebAssembly program's linear memory upon initialization.
    ///
    /// Specified in bytes, must be evenly divisible by the WebAssembly page size (64K), and must be
    /// less than or equal to `reserved_size`.
    pub initial_size: u64,

    /// Maximum bytes of memory for the WebAssembly program's linear memory at any time.
    ///
    /// This is not necessarily the same as `reserved_size` - we want to be able to tune the check
    /// bound there separately than the declaration of a max size in the client program.
    ///
    /// The program may optionally define this value. If it does, it must be less than the
    /// `reserved_size`. If it does not, the max size is left up to the runtime, and is allowed to
    /// be less than `reserved_size`.
    pub max_size: Option<u64>,
}

impl HeapSpec {
    pub fn new(
        reserved_size: u64,
        guard_size: u64,
        initial_size: u64,
        max_size: Option<u64>,
    ) -> Self {
        Self {
            reserved_size,
            guard_size,
            initial_size,
            max_size,
        }
    }

    /// Some very small test programs dont specify a memory import or definition.
    pub fn empty() -> Self {
        Self {
            reserved_size: 0,
            guard_size: 0,
            initial_size: 0,
            max_size: None,
        }
    }
}

/// A sparse representation of a Lucet module's initial heap.
///
/// The lifetime parameter exists to support zero-copy deserialization for the `&[u8]` slices
/// representing non-zero pages. For a variant with owned `Vec<u8>` pages, see
/// [`OwnedSparseData`](owned/struct.OwnedSparseData.html).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SparseData<'a> {
    /// Indices into the vector correspond to the offset, in host page (4k) increments, from the
    /// base of the instance heap.
    ///
    /// If the option at a given index is None, the page is initialized as zeros. Otherwise,
    /// the contents of the page are given as a slice of exactly 4k bytes.
    ///
    /// The deserializer of this datastructure does not make sure the 4k invariant holds,
    /// but the constructor on the serializier side does.
    #[serde(borrow)]
    pages: Vec<Option<&'a [u8]>>,
}

impl<'a> SparseData<'a> {
    /// Create a new `SparseData` from its constituent pages.
    ///
    /// Entries in the `pages` argument which are `Some` must contain a slice of exactly the host
    /// page size (4096), otherwise this function returns `Error::IncorrectPageSize`. Entries which
    /// are `None` are interpreted as empty pages, which will be zeroed by the runtime.
    pub fn new(pages: Vec<Option<&'a [u8]>>) -> Result<Self, Error> {
        if !pages.iter().all(|page| match page {
            Some(contents) => contents.len() == 4096,
            None => true,
        }) {
            return Err(Error::IncorrectPageSize);
        }

        Ok(Self { pages })
    }

    pub fn pages(&self) -> &[Option<&'a [u8]>] {
        &self.pages
    }

    pub fn get_page(&self, offset: usize) -> &Option<&'a [u8]> {
        self.pages.get(offset).unwrap_or(&None)
    }

    pub fn len(&self) -> usize {
        self.pages.len()
    }
}

/// A sparse representation of a Lucet module's initial heap.
///
/// This is a version of [`SparseData`](../struct.SparseData.html) with owned `Vec<u8>`s
/// representing pages. This type is useful when directly building up a value to be serialized.
pub struct OwnedSparseData {
    pages: Vec<Option<Vec<u8>>>,
}

impl OwnedSparseData {
    /// Create a new `OwnedSparseData` from its consitutent pages.
    ///
    /// Entries in the `pages` argument which are `Some` must contain a vector of exactly the host
    /// page size (4096), otherwise this function returns `Error::IncorrectPageSize`. Entries which
    /// are `None` are interpreted as empty pages, which will be zeroed by the runtime.
    pub fn new(pages: Vec<Option<Vec<u8>>>) -> Result<Self, Error> {
        if !pages.iter().all(|page| match page {
            Some(contents) => contents.len() == 4096,
            None => true,
        }) {
            return Err(Error::IncorrectPageSize);
        }
        Ok(Self { pages })
    }

    /// Create a [`SparseData`](../struct.SparseData.html) backed by the values in this
    /// `OwnedSparseData`.
    pub fn to_ref<'a>(&'a self) -> SparseData<'a> {
        SparseData::new(
            self.pages
                .iter()
                .map(|c| match c {
                    Some(data) => Some(data.as_slice()),
                    None => None,
                })
                .collect(),
        )
        .expect("SparseData invariant enforced by OwnedSparseData constructor")
    }
}
