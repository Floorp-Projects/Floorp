use addr2line;
use findshlibs::{self, Segment, SharedLibrary};
use gimli;
use memmap::Mmap;
use object::{self, Object};
use std::cell::RefCell;
use std::env;
use std::fs::File;
use std::mem;
use std::os::raw::c_void;
use std::path::{Path, PathBuf};
use std::u32;

use SymbolName;

const MAPPINGS_CACHE_SIZE: usize = 4;

type Dwarf<'map> = addr2line::Context<gimli::EndianBuf<'map, gimli::RunTimeEndian>>;
type Symbols<'map> = object::SymbolMap<'map>;

struct Mapping {
    // 'static lifetime is a lie to hack around lack of support for self-referential structs.
    dwarf: Dwarf<'static>,
    symbols: Symbols<'static>,
    _map: Mmap,
}

impl Mapping {
    fn new(path: &PathBuf) -> Option<Mapping> {
        let file = File::open(path).ok()?;
        // TODO: not completely safe, see https://github.com/danburkert/memmap-rs/issues/25
        let map = unsafe { Mmap::map(&file).ok()? };
        let (dwarf, symbols) = {
            let object = object::File::parse(&*map).ok()?;
            let dwarf = addr2line::Context::new(&object).ok()?;
            let symbols = object.symbol_map();
            // Convert to 'static lifetimes.
            unsafe { (mem::transmute(dwarf), mem::transmute(symbols)) }
        };
        Some(Mapping {
            dwarf,
            symbols,
            _map: map,
        })
    }

    // Ensure the 'static lifetimes don't leak.
    fn rent<F>(&self, mut f: F)
    where
        F: FnMut(&Dwarf, &Symbols),
    {
        f(&self.dwarf, &self.symbols)
    }
}

thread_local! {
    // A very small, very simple LRU cache for debug info mappings.
    //
    // The hit rate should be very high, since the typical stack doesn't cross
    // between many shared libraries.
    //
    // The `addr2line::Context` structures are pretty expensive to create. Its
    // cost is expected to be amortized by subsequent `locate` queries, which
    // leverage the structures built when constructing `addr2line::Context`s to
    // get nice speedups. If we didn't have this cache, that amortization would
    // never happen, and symbolicating backtraces would be ssssllllooooowwww.
    static MAPPINGS_CACHE: RefCell<Vec<(PathBuf, Mapping)>>
        = RefCell::new(Vec::with_capacity(MAPPINGS_CACHE_SIZE));
}

fn with_mapping_for_path<F>(path: PathBuf, f: F)
where
    F: FnMut(&Dwarf, &Symbols),
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
            let mapping = match Mapping::new(&path) {
                None => return,
                Some(m) => m,
            };

            if cache.len() == MAPPINGS_CACHE_SIZE {
                cache.pop();
            }

            cache.insert(0, (path, mapping));
        }

        cache[0].1.rent(f);
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
    with_mapping_for_path(path, |dwarf, symbols| {
        let mut found_sym = false;
        if let Ok(mut frames) = dwarf.find_frames(addr.0 as u64) {
            while let Ok(Some(frame)) = frames.next() {
                let (file, line) = frame
                    .location
                    .map(|l| (l.file, l.line))
                    .unwrap_or((None, None));
                let name = frame
                    .function
                    .and_then(|f| f.raw_name().ok().map(|f| f.to_string()));
                let sym = super::Symbol {
                    inner: Symbol::new(addr.0 as usize, file, line, name),
                };
                cb(&sym);
                found_sym = true;
            }
        }

        // No DWARF info found, so fallback to the symbol table.
        if !found_sym {
            if let Some(name) = symbols.get(addr.0 as u64).and_then(|x| x.name()) {
                let sym = super::Symbol {
                    inner: Symbol::new(addr.0 as usize, None, None, Some(name.to_string())),
                };
                cb(&sym);
            }
        }
    });
}

pub struct Symbol {
    addr: usize,
    file: Option<PathBuf>,
    line: Option<u64>,
    name: Option<String>,
}

impl Symbol {
    fn new(addr: usize,
           file: Option<PathBuf>,
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
        self.file.as_ref().map(|f| f.as_ref())
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
