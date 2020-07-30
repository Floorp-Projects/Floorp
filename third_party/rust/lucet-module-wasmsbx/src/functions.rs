use crate::traps::{TrapManifest, TrapSite};
use cranelift_entity::entity_impl;
use serde::{Deserialize, Serialize};

use std::slice::from_raw_parts;

/// FunctionIndex is an identifier for a function, imported, exported, or external. The space of
/// FunctionIndex is shared for all of these, so `FunctionIndex(N)` may identify exported function
/// #2, `FunctionIndex(N + 1)` may identify an internal function, and `FunctionIndex(N + 2)` may
/// identify an imported function.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug, Serialize, Deserialize)]
pub struct FunctionIndex(u32);

impl FunctionIndex {
    pub fn from_u32(idx: u32) -> FunctionIndex {
        FunctionIndex(idx)
    }
    pub fn as_u32(&self) -> u32 {
        self.0
    }
}

/// ImportFunction describes an internal function - its internal function index and the name/module
/// pair that function should be found in.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug, Serialize, Deserialize)]
pub struct ImportFunction<'a> {
    pub fn_idx: FunctionIndex,
    pub module: &'a str,
    pub name: &'a str,
}

/// ExportFunction describes an exported function - its internal function index and a name that
/// function has been exported under.
#[derive(Clone, PartialEq, Eq, Hash, Debug, Serialize, Deserialize)]
pub struct ExportFunction<'a> {
    pub fn_idx: FunctionIndex,
    #[serde(borrow)]
    pub names: Vec<&'a str>,
}

pub struct OwnedExportFunction {
    pub fn_idx: FunctionIndex,
    pub names: Vec<String>,
}

impl OwnedExportFunction {
    pub fn to_ref<'a>(&'a self) -> ExportFunction<'a> {
        ExportFunction {
            fn_idx: self.fn_idx.clone(),
            names: self.names.iter().map(|x| x.as_str()).collect(),
        }
    }
}

pub struct OwnedImportFunction {
    pub fn_idx: FunctionIndex,
    pub module: String,
    pub name: String,
}

impl OwnedImportFunction {
    pub fn to_ref<'a>(&'a self) -> ImportFunction<'a> {
        ImportFunction {
            fn_idx: self.fn_idx.clone(),
            module: self.module.as_str(),
            name: self.name.as_str(),
        }
    }
}

/// UniqueSignatureIndex names a signature after collapsing duplicate signatures to a single
/// identifier, whereas SignatureIndex is directly what the original module specifies, and may
/// specify duplicates of types that are structurally equal.
#[derive(Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord, Debug, Serialize, Deserialize)]
pub struct UniqueSignatureIndex(u32);
entity_impl!(UniqueSignatureIndex);

/// FunctionPointer serves entirely as a safer way to work with function pointers than as raw u64
/// or usize values. It also avoids the need to write them as `fn` types, which cannot be freely
/// cast from one to another with `as`. If you need to call a `FunctionPointer`, use `as_usize()`
/// and transmute the resulting usize to a `fn` type with appropriate signature.
#[derive(Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord, Debug, Serialize, Deserialize)]
pub struct FunctionPointer(usize);

impl FunctionPointer {
    pub fn from_usize(ptr: usize) -> FunctionPointer {
        FunctionPointer(ptr)
    }
    pub fn as_usize(&self) -> usize {
        self.0
    }
}

/// Information about the corresponding function.
///
/// This is split from but closely related to a [`FunctionSpec`]. The distinction is largely for
/// serialization/deserialization simplicity, as [`FunctionSpec`] contains fields that need
/// cooperation from a loader, with manual layout and serialization as a result.
/// [`FunctionMetadata`] is the remainder of fields that can be automatically
/// serialized/deserialied and are small enough copying isn't a large concern.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct FunctionMetadata<'a> {
    pub signature: UniqueSignatureIndex,
    /// the "name" field is some human-friendly name, not necessarily the same as used to reach
    /// this function (through an export, for example), and may not even indicate that a function
    /// is exported at all.
    /// TODO: at some point when possible, this field ought to be set from the names section of a
    /// wasm module. At the moment that information is lost at parse time.
    #[serde(borrow)]
    pub name: Option<&'a str>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct OwnedFunctionMetadata {
    pub signature: UniqueSignatureIndex,
    pub name: Option<String>,
}

impl OwnedFunctionMetadata {
    pub fn to_ref(&self) -> FunctionMetadata<'_> {
        FunctionMetadata {
            signature: self.signature.clone(),
            name: self.name.as_ref().map(|n| n.as_str()),
        }
    }
}

pub struct FunctionHandle {
    pub ptr: FunctionPointer,
    pub id: FunctionIndex,
}

// The layout of this struct is very tightly coupled to lucetc's `write_function_manifest`!
//
// Specifically, `write_function_manifest` sets up relocations on `code_addr` and `traps_addr`.
// It does not explicitly serialize a correctly formed `FunctionSpec`, because addresses
// for these fields do not exist until the object is loaded in the future.
//
// So `write_function_manifest` has implicit knowledge of the layout of this structure
// (including padding bytes between `code_len` and `traps_addr`)
#[repr(C)]
#[derive(Clone, Debug)]
pub struct FunctionSpec {
    code_addr: u64,
    code_len: u32,
    traps_addr: u64,
    traps_len: u64,
}

impl FunctionSpec {
    pub fn new(code_addr: u64, code_len: u32, traps_addr: u64, traps_len: u64) -> Self {
        FunctionSpec {
            code_addr,
            code_len,
            traps_addr,
            traps_len,
        }
    }
    pub fn ptr(&self) -> FunctionPointer {
        FunctionPointer::from_usize(self.code_addr as usize)
    }
    pub fn code_len(&self) -> u32 {
        self.code_len
    }
    pub fn traps_len(&self) -> u64 {
        self.traps_len
    }
    pub fn contains(&self, addr: u64) -> bool {
        addr >= self.code_addr && (addr - self.code_addr) < (self.code_len as u64)
    }
    pub fn relative_addr(&self, addr: u64) -> Option<u32> {
        if let Some(offset) = addr.checked_sub(self.code_addr) {
            if offset < (self.code_len as u64) {
                // self.code_len is u32, so if the above check succeeded
                // offset must implicitly be <= u32::MAX - the following
                // conversion will not truncate bits
                return Some(offset as u32);
            }
        }

        None
    }
    pub fn traps(&self) -> Option<TrapManifest<'_>> {
        let traps_ptr = self.traps_addr as *const TrapSite;
        if !traps_ptr.is_null() {
            let traps_slice = unsafe { from_raw_parts(traps_ptr, self.traps_len as usize) };
            Some(TrapManifest::new(traps_slice))
        } else {
            None
        }
    }
}
