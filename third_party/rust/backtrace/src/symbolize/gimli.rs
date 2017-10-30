use addr2line;
use findshlibs::{self, Segment, SharedLibrary};
use std::cell::RefCell;
use std::env;
use std::os::raw::c_void;
use std::path::{Path, PathBuf};
use std::u32;

use SymbolName;

const MAPPINGS_CACHE_SIZE: usize = 4;

thread_local! {
    // A very small, very simple LRU cache for debug info mappings.
    //
    // The hit rate should be very high, since the typical stack doesn't cross
    // between many shared libraries.
    //
    // The `addr2line::Mapping` structures are pretty expensive to create. Its
    // cost is expected to be amortized by subsequent `locate` queries, which
    // leverage the structures built when constructing `addr2line::Mapping`s to
    // get nice speedups. If we didn't have this cache, that amortization would
    // never happen, and symbolicating backtraces would be ssssllllooooowwww.
    static MAPPINGS_CACHE: RefCell<Vec<(PathBuf, addr2line::Mapping)>>
        = RefCell::new(Vec::with_capacity(MAPPINGS_CACHE_SIZE));
}

fn with_mapping_for_path<F>(path: PathBuf, mut f: F)
where
    F: FnMut(&mut addr2line::Mapping)
{
    MAPPINGS_CACHE.with(|cache| {
        let mut cache = cache.borrow_mut();

        let idx = cache.iter().position(|&(ref p, _)| p == &path);

        // Invariant: after this conditional completes without early returning
        // from an error, the cache entry for this path is at index 0.

        if let Some(idx) = idx {
            // When the mapping is already in the cache, move it to the front.
            if idx != 0 {
                let entry = cache.remove(idx);
                cache.insert(0, entry);
            }
        } else {
            // When the mapping is not in the cache, create a new mapping,
            // insert it into the front of the cache, and evict the oldest cache
            // entry if necessary.
            let opts = addr2line::Options::default()
                .with_functions();

            let mapping = match opts.build(&path) {
                Err(_) => return,
                Ok(m) => m,
            };

            if cache.len() == MAPPINGS_CACHE_SIZE {
                cache.pop();
            }

            cache.insert(0, (path, mapping));
        }

        f(&mut cache[0].1);
    });
}

pub fn resolve(addr: *mut c_void, cb: &mut FnMut(&super::Symbol)) {
    // First, find the file containing the segment that the given AVMA (after
    // relocation) address falls within. Use the containing segment to compute
    // the SVMA (before relocation) address.
    //
    // Note that the OS APIs that `SharedLibrary::each` is implemented with hold
    // a lock for the duration of the `each` call, so we want to keep this
    // section as short as possible to avoid contention with other threads
    // capturing backtraces.
    let addr = findshlibs::Avma(addr as *mut u8 as *const u8);
    let mut so_info = None;
    findshlibs::TargetSharedLibrary::each(|so| {
        use findshlibs::IterationControl::*;

        for segment in so.segments() {
            if segment.contains_avma(so, addr) {
                let addr = so.avma_to_svma(addr);
                let path = so.name().to_string_lossy();
                so_info = Some((addr, path.to_string()));
                return Break;
            }
        }

        Continue
    });
    let (addr, path) = match so_info {
        None => return,
        Some((a, p)) => (a, p),
    };

    // Second, fixup the path. Empty path means that this address falls within
    // the main executable, not a shared library.
    let path = if path.is_empty() {
        match env::current_exe() {
            Err(_) => return,
            Ok(p) => p,
        }
    } else {
        PathBuf::from(path)
    };

    // Finally, get a cached mapping or create a new mapping for this file, and
    // evaluate the DWARF info to find the file/line/name for this address.
    with_mapping_for_path(path, |mapping| {
        let (file, line, func) = match mapping.locate(addr.0 as u64) {
            Ok(None) | Err(_) => return,
            Ok(Some((file, line, func))) => (file, line, func),
        };

        let sym = super::Symbol {
            inner: Symbol::new(addr.0 as usize,
                               file,
                               line,
                               func.map(|f| f.to_string()))
        };

        cb(&sym);
    });
}

pub struct Symbol {
    addr: usize,
    file: PathBuf,
    line: Option<u64>,
    name: Option<String>,
}

impl Symbol {
    fn new(addr: usize,
           file: PathBuf,
           line: Option<u64>,
           name: Option<String>)
           -> Symbol {
        Symbol {
            addr,
            file,
            line,
            name,
        }
    }

    pub fn name(&self) -> Option<SymbolName> {
        self.name.as_ref().map(|s| SymbolName::new(s.as_bytes()))
    }

    pub fn addr(&self) -> Option<*mut c_void> {
        Some(self.addr as *mut c_void)
    }

    pub fn filename(&self) -> Option<&Path> {
        Some(self.file.as_ref())
    }

    pub fn lineno(&self) -> Option<u32> {
        self.line
            .and_then(|l| if l > (u32::MAX as u64) {
                None
            } else {
                Some(l as u32)
            })
    }
}
