/*!
Crate `walkdir` provides an efficient and cross platform implementation
of recursive directory traversal. Several options are exposed to control
iteration, such as whether to follow symbolic links (default off), limit the
maximum number of simultaneous open file descriptors and the ability to
efficiently skip descending into directories.

To use this crate, add `walkdir` as a dependency to your project's
`Cargo.toml`:

```text
[dependencies]
walkdir = "2"
```

# From the top

The [`WalkDir`] type builds iterators. The [`DirEntry`] type describes values
yielded by the iterator. Finally, the [`Error`] type is a small wrapper around
[`std::io::Error`] with additional information, such as if a loop was detected
while following symbolic links (not enabled by default).

[`WalkDir`]: struct.WalkDir.html
[`DirEntry`]: struct.DirEntry.html
[`Error`]: struct.Error.html
[`std::io::Error`]: https://doc.rust-lang.org/stable/std/io/struct.Error.html

# Example

The following code recursively iterates over the directory given and prints
the path for each entry:

```no_run
use walkdir::WalkDir;
# use walkdir::Error;

# fn try_main() -> Result<(), Error> {
for entry in WalkDir::new("foo") {
    println!("{}", entry?.path().display());
}
# Ok(())
# }
```

Or, if you'd like to iterate over all entries and ignore any errors that
may arise, use [`filter_map`]. (e.g., This code below will silently skip
directories that the owner of the running process does not have permission to
access.)

```no_run
use walkdir::WalkDir;

for entry in WalkDir::new("foo").into_iter().filter_map(|e| e.ok()) {
    println!("{}", entry.path().display());
}
```

[`filter_map`]: https://doc.rust-lang.org/stable/std/iter/trait.Iterator.html#method.filter_map

# Example: follow symbolic links

The same code as above, except [`follow_links`] is enabled:

```no_run
use walkdir::WalkDir;
# use walkdir::Error;

# fn try_main() -> Result<(), Error> {
for entry in WalkDir::new("foo").follow_links(true) {
    println!("{}", entry?.path().display());
}
# Ok(())
# }
```

[`follow_links`]: struct.WalkDir.html#method.follow_links

# Example: skip hidden files and directories on unix

This uses the [`filter_entry`] iterator adapter to avoid yielding hidden files
and directories efficiently (i.e. without recursing into hidden directories):

```no_run
use walkdir::{DirEntry, WalkDir};
# use walkdir::Error;

fn is_hidden(entry: &DirEntry) -> bool {
    entry.file_name()
         .to_str()
         .map(|s| s.starts_with("."))
         .unwrap_or(false)
}

# fn try_main() -> Result<(), Error> {
let walker = WalkDir::new("foo").into_iter();
for entry in walker.filter_entry(|e| !is_hidden(e)) {
    println!("{}", entry?.path().display());
}
# Ok(())
# }
```

[`filter_entry`]: struct.IntoIter.html#method.filter_entry
*/

#![doc(html_root_url = "https://docs.rs/walkdir/2.0.0")]
#![deny(missing_docs)]

#[cfg(test)]
extern crate quickcheck;
#[cfg(test)]
extern crate rand;
extern crate same_file;
#[cfg(windows)]
extern crate winapi;

use std::cmp::{Ordering, min};
use std::error;
use std::fmt;
use std::fs::{self, FileType, ReadDir};
use std::io;
use std::ffi::OsStr;
use std::path::{Path, PathBuf};
use std::result;
use std::vec;

use same_file::Handle;

#[cfg(unix)]
pub use unix::DirEntryExt;

#[cfg(test)]
mod tests;
#[cfg(unix)]
mod unix;

/// Like try, but for iterators that return [`Option<Result<_, _>>`].
///
/// [`Option<Result<_, _>>`]: https://doc.rust-lang.org/stable/std/option/enum.Option.html
macro_rules! itry {
    ($e:expr) => {
        match $e {
            Ok(v) => v,
            Err(err) => return Some(Err(From::from(err))),
        }
    }
}

/// A result type for walkdir operations.
///
/// Note that this result type embeds the error type in this crate. This
/// is only useful if you care about the additional information provided by
/// the error (such as the path associated with the error or whether a loop
/// was dectected). If you want things to Just Work, then you can use
/// [`io::Result`] instead since the error type in this package will
/// automatically convert to an [`io::Result`] when using the [`try!`] macro.
///
/// [`io::Result`]: https://doc.rust-lang.org/stable/std/io/type.Result.html
/// [`try!`]: https://doc.rust-lang.org/stable/std/macro.try.html
pub type Result<T> = ::std::result::Result<T, Error>;

/// A builder to create an iterator for recursively walking a directory.
///
/// Results are returned in depth first fashion, with directories yielded
/// before their contents. If [`contents_first`] is true, contents are yielded
/// before their directories. The order is unspecified but if [`sort_by`] is
/// given, directory entries are sorted according to this function. Directory
/// entries `.` and `..` are always omitted.
///
/// If an error occurs at any point during iteration, then it is returned in
/// place of its corresponding directory entry and iteration continues as
/// normal. If an error occurs while opening a directory for reading, then it
/// is not descended into (but the error is still yielded by the iterator).
/// Iteration may be stopped at any time. When the iterator is destroyed, all
/// resources associated with it are freed.
///
/// [`contents_first`]: struct.WalkDir.html#method.contents_first
/// [`sort_by`]: struct.WalkDir.html#method.sort_by
///
/// # Usage
///
/// This type implements [`IntoIterator`] so that it may be used as the subject
/// of a `for` loop. You may need to call [`into_iter`] explicitly if you want
/// to use iterator adapters such as [`filter_entry`].
///
/// Idiomatic use of this type should use method chaining to set desired
/// options. For example, this only shows entries with a depth of `1`, `2` or
/// `3` (relative to `foo`):
///
/// ```no_run
/// use walkdir::WalkDir;
/// # use walkdir::Error;
///
/// # fn try_main() -> Result<(), Error> {
/// for entry in WalkDir::new("foo").min_depth(1).max_depth(3) {
///     println!("{}", entry?.path().display());
/// }
/// # Ok(())
/// # }
/// ```
///
/// [`IntoIterator`]: https://doc.rust-lang.org/stable/std/iter/trait.IntoIterator.html
/// [`into_iter`]: https://doc.rust-lang.org/nightly/core/iter/trait.IntoIterator.html#tymethod.into_iter
/// [`filter_entry`]: struct.IntoIter.html#method.filter_entry
///
/// Note that the iterator by default includes the top-most directory. Since
/// this is the only directory yielded with depth `0`, it is easy to ignore it
/// with the [`min_depth`] setting:
///
/// ```no_run
/// use walkdir::WalkDir;
/// # use walkdir::Error;
///
/// # fn try_main() -> Result<(), Error> {
/// for entry in WalkDir::new("foo").min_depth(1) {
///     println!("{}", entry?.path().display());
/// }
/// # Ok(())
/// # }
/// ```
///
/// [`min_depth`]: struct.WalkDir.html#method.min_depth
///
/// This will only return descendents of the `foo` directory and not `foo`
/// itself.
///
/// # Loops
///
/// This iterator (like most/all recursive directory iterators) assumes that
/// no loops can be made with *hard* links on your file system. In particular,
/// this would require creating a hard link to a directory such that it creates
/// a loop. On most platforms, this operation is illegal.
///
/// Note that when following symbolic/soft links, loops are detected and an
/// error is reported.
#[derive(Debug)]
pub struct WalkDir {
    opts: WalkDirOptions,
    root: PathBuf,
}

struct WalkDirOptions {
    follow_links: bool,
    max_open: usize,
    min_depth: usize,
    max_depth: usize,
    sorter: Option<Box<
        FnMut(&DirEntry,&DirEntry) -> Ordering + Send + Sync + 'static
    >>,
    contents_first: bool,
}

impl fmt::Debug for WalkDirOptions {
    fn fmt(&self, f: &mut fmt::Formatter) -> result::Result<(), fmt::Error> {
        let sorter_str = if self.sorter.is_some() {
            // FnMut isn't `Debug`
            "Some(...)"
        } else {
            "None"
        };
        f.debug_struct("WalkDirOptions")
            .field("follow_links", &self.follow_links)
            .field("max_open", &self.max_open)
            .field("min_depth", &self.min_depth)
            .field("max_depth", &self.max_depth)
            .field("sorter", &sorter_str)
            .field("contents_first", &self.contents_first)
            .finish()
    }
}

impl WalkDir {
    /// Create a builder for a recursive directory iterator starting at the
    /// file path `root`. If `root` is a directory, then it is the first item
    /// yielded by the iterator. If `root` is a file, then it is the first
    /// and only item yielded by the iterator. If `root` is a symlink, then it
    /// is always followed.
    pub fn new<P: AsRef<Path>>(root: P) -> Self {
        WalkDir {
            opts: WalkDirOptions {
                follow_links: false,
                max_open: 10,
                min_depth: 0,
                max_depth: ::std::usize::MAX,
                sorter: None,
                contents_first: false,
            },
            root: root.as_ref().to_path_buf(),
        }
    }

    /// Set the minimum depth of entries yielded by the iterator.
    ///
    /// The smallest depth is `0` and always corresponds to the path given
    /// to the `new` function on this type. Its direct descendents have depth
    /// `1`, and their descendents have depth `2`, and so on.
    pub fn min_depth(mut self, depth: usize) -> Self {
        self.opts.min_depth = depth;
        if self.opts.min_depth > self.opts.max_depth {
            self.opts.min_depth = self.opts.max_depth;
        }
        self
    }

    /// Set the maximum depth of entries yield by the iterator.
    ///
    /// The smallest depth is `0` and always corresponds to the path given
    /// to the `new` function on this type. Its direct descendents have depth
    /// `1`, and their descendents have depth `2`, and so on.
    ///
    /// Note that this will not simply filter the entries of the iterator, but
    /// it will actually avoid descending into directories when the depth is
    /// exceeded.
    pub fn max_depth(mut self, depth: usize) -> Self {
        self.opts.max_depth = depth;
        if self.opts.max_depth < self.opts.min_depth {
            self.opts.max_depth = self.opts.min_depth;
        }
        self
    }

    /// Follow symbolic links. By default, this is disabled.
    ///
    /// When `yes` is `true`, symbolic links are followed as if they were
    /// normal directories and files. If a symbolic link is broken or is
    /// involved in a loop, an error is yielded.
    ///
    /// When enabled, the yielded [`DirEntry`] values represent the target of
    /// the link while the path corresponds to the link. See the [`DirEntry`]
    /// type for more details.
    ///
    /// [`DirEntry`]: struct.DirEntry.html
    pub fn follow_links(mut self, yes: bool) -> Self {
        self.opts.follow_links = yes;
        self
    }

    /// Set the maximum number of simultaneously open file descriptors used
    /// by the iterator.
    ///
    /// `n` must be greater than or equal to `1`. If `n` is `0`, then it is set
    /// to `1` automatically. If this is not set, then it defaults to some
    /// reasonably low number.
    ///
    /// This setting has no impact on the results yielded by the iterator
    /// (even when `n` is `1`). Instead, this setting represents a trade off
    /// between scarce resources (file descriptors) and memory. Namely, when
    /// the maximum number of file descriptors is reached and a new directory
    /// needs to be opened to continue iteration, then a previous directory
    /// handle is closed and has its unyielded entries stored in memory. In
    /// practice, this is a satisfying trade off because it scales with respect
    /// to the *depth* of your file tree. Therefore, low values (even `1`) are
    /// acceptable.
    ///
    /// Note that this value does not impact the number of system calls made by
    /// an exhausted iterator.
    ///
    /// # Platform behavior
    ///
    /// On Windows, if `follow_links` is enabled, then this limit is not
    /// respected. In particular, the maximum number of file descriptors opened
    /// is proportional to the depth of the directory tree traversed.
    pub fn max_open(mut self, mut n: usize) -> Self {
        if n == 0 {
            n = 1;
        }
        self.opts.max_open = n;
        self
    }

    /// Set a function for sorting directory entries.
    ///
    /// If a compare function is set, the resulting iterator will return all
    /// paths in sorted order. The compare function will be called to compare
    /// entries from the same directory.
    ///
    /// ```rust,no-run
    /// use std::cmp;
    /// use std::ffi::OsString;
    /// use walkdir::WalkDir;
    ///
    /// WalkDir::new("foo").sort_by(|a,b| a.file_name().cmp(b.file_name()));
    /// ```
    pub fn sort_by<F>(mut self, cmp: F) -> Self
    where F: FnMut(&DirEntry, &DirEntry) -> Ordering + Send + Sync + 'static
    {
        self.opts.sorter = Some(Box::new(cmp));
        self
    }

    /// Yield a directory's contents before the directory itself. By default,
    /// this is disabled.
    ///
    /// When `yes` is `false` (as is the default), the directory is yielded
    /// before its contents are read. This is useful when, e.g. you want to
    /// skip processing of some directories.
    ///
    /// When `yes` is `true`, the iterator yields the contents of a directory
    /// before yielding the directory itself. This is useful when, e.g. you
    /// want to recursively delete a directory.
    ///
    /// # Example
    ///
    /// Assume the following directory tree:
    ///
    /// ```text
    /// foo/
    ///   abc/
    ///     qrs
    ///     tuv
    ///   def/
    /// ```
    ///
    /// With contents_first disabled (the default), the following code visits
    /// the directory tree in depth-first order:
    ///
    /// ```no_run
    /// use walkdir::WalkDir;
    ///
    /// for entry in WalkDir::new("foo") {
    ///     let entry = entry.unwrap();
    ///     println!("{}", entry.path().display());
    /// }
    ///
    /// // foo
    /// // foo/abc
    /// // foo/abc/qrs
    /// // foo/abc/tuv
    /// // foo/def
    /// ```
    ///
    /// With contents_first enabled:
    ///
    /// ```no_run
    /// use walkdir::WalkDir;
    ///
    /// for entry in WalkDir::new("foo").contents_first(true) {
    ///     let entry = entry.unwrap();
    ///     println!("{}", entry.path().display());
    /// }
    ///
    /// // foo/abc/qrs
    /// // foo/abc/tuv
    /// // foo/abc
    /// // foo/def
    /// // foo
    /// ```
    pub fn contents_first(mut self, yes: bool) -> Self {
        self.opts.contents_first = yes;
        self
    }
}

impl IntoIterator for WalkDir {
    type Item = Result<DirEntry>;
    type IntoIter = IntoIter;

    fn into_iter(self) -> IntoIter {
        IntoIter {
            opts: self.opts,
            start: Some(self.root),
            stack_list: vec![],
            stack_path: vec![],
            oldest_opened: 0,
            depth: 0,
            deferred_dirs: vec![],
        }
    }
}

/// An iterator for recursively descending into a directory.
///
/// A value with this type must be constructed with the [`WalkDir`] type, which
/// uses a builder pattern to set options such as min/max depth, max open file
/// descriptors and whether the iterator should follow symbolic links. After
/// constructing a `WalkDir`, call [`.into_iter()`] at the end of the chain.
///
/// The order of elements yielded by this iterator is unspecified.
///
/// [`WalkDir`]: struct.WalkDir.html
/// [`.into_iter()`]: struct.WalkDir.html#into_iter.v
#[derive(Debug)]
pub struct IntoIter {
    /// Options specified in the builder. Depths, max fds, etc.
    opts: WalkDirOptions,
    /// The start path.
    ///
    /// This is only `Some(...)` at the beginning. After the first iteration,
    /// this is always `None`.
    start: Option<PathBuf>,
    /// A stack of open (up to max fd) or closed handles to directories.
    /// An open handle is a plain [`fs::ReadDir`] while a closed handle is
    /// a `Vec<fs::DirEntry>` corresponding to the as-of-yet consumed entries.
    ///
    /// [`fs::ReadDir`]: https://doc.rust-lang.org/stable/std/fs/struct.ReadDir.html
    stack_list: Vec<DirList>,
    /// A stack of file paths.
    ///
    /// This is *only* used when [`follow_links`] is enabled. In all other
    /// cases this stack is empty.
    ///
    /// [`follow_links`]: struct.WalkDir.html#method.follow_links
    stack_path: Vec<Ancestor>,
    /// An index into `stack_list` that points to the oldest open directory
    /// handle. If the maximum fd limit is reached and a new directory needs to
    /// be read, the handle at this index is closed before the new directory is
    /// opened.
    oldest_opened: usize,
    /// The current depth of iteration (the length of the stack at the
    /// beginning of each iteration).
    depth: usize,
    /// A list of DirEntries corresponding to directories, that are
    /// yielded after their contents has been fully yielded. This is only
    /// used when `contents_first` is enabled.
    deferred_dirs: Vec<DirEntry>,
}

/// An ancestor is an item in the directory tree traversed by walkdir, and is
/// used to check for loops in the tree when traversing symlinks.
#[derive(Debug)]
struct Ancestor {
    /// The path of this ancestor.
    path: PathBuf,
    /// An open file to this ancesor. This is only used on Windows where
    /// opening a file handle appears to be quite expensive, so we choose to
    /// cache it. This comes at the cost of not respecting the file descriptor
    /// limit set by the user.
    #[cfg(windows)]
    handle: Handle,
}

impl Ancestor {
    /// Create a new ancestor from the given directory path.
    #[cfg(windows)]
    fn new(dent: &DirEntry) -> io::Result<Ancestor> {
        let handle = Handle::from_path(dent.path())?;
        Ok(Ancestor {
            path: dent.path().to_path_buf(),
            handle: handle,
        })
    }

    /// Create a new ancestor from the given directory path.
    #[cfg(not(windows))]
    fn new(dent: &DirEntry) -> io::Result<Ancestor> {
        Ok(Ancestor { path: dent.path().to_path_buf() })
    }

    /// Returns true if and only if the given open file handle corresponds to
    /// the same directory as this ancestor.
    #[cfg(windows)]
    fn is_same(&self, child: &Handle) -> io::Result<bool> {
        Ok(child == &self.handle)
    }

    /// Returns true if and only if the given open file handle corresponds to
    /// the same directory as this ancestor.
    #[cfg(not(windows))]
    fn is_same(&self, child: &Handle) -> io::Result<bool> {
        Ok(child == &Handle::from_path(&self.path)?)
    }
}

/// A sequence of unconsumed directory entries.
///
/// This represents the opened or closed state of a directory handle. When
/// open, future entries are read by iterating over the raw `fs::ReadDir`.
/// When closed, all future entries are read into memory. Iteration then
/// proceeds over a [`Vec<fs::DirEntry>`].
///
/// [`fs::ReadDir`]: https://doc.rust-lang.org/stable/std/fs/struct.ReadDir.html
/// [`Vec<fs::DirEntry>`]: https://doc.rust-lang.org/stable/std/vec/struct.Vec.html
#[derive(Debug)]
enum DirList {
    /// An opened handle.
    ///
    /// This includes the depth of the handle itself.
    ///
    /// If there was an error with the initial [`fs::read_dir`] call, then it
    /// is stored here. (We use an [`Option<...>`] to make yielding the error
    /// exactly once simpler.)
    ///
    /// [`fs::read_dir`]: https://doc.rust-lang.org/stable/std/fs/fn.read_dir.html
    /// [`Option<...>`]: https://doc.rust-lang.org/stable/std/option/enum.Option.html
    Opened { depth: usize, it: result::Result<ReadDir, Option<Error>> },
    /// A closed handle.
    ///
    /// All remaining directory entries are read into memory.
    Closed(vec::IntoIter<Result<DirEntry>>),
}

/// A directory entry.
///
/// This is the type of value that is yielded from the iterators defined in
/// this crate.
///
/// On Unix systems, this type implements the [`DirEntryExt`] trait, which
/// provides efficient access to the inode number of the directory entry.
///
/// # Differences with `std::fs::DirEntry`
///
/// This type mostly mirrors the type by the same name in [`std::fs`]. There
/// are some differences however:
///
/// * All recursive directory iterators must inspect the entry's type.
/// Therefore, the value is stored and its access is guaranteed to be cheap and
/// successful.
/// * [`path`] and [`file_name`] return borrowed variants.
/// * If [`follow_links`] was enabled on the originating iterator, then all
/// operations except for [`path`] operate on the link target. Otherwise, all
/// operations operate on the symbolic link.
///
/// [`std::fs`]: https://doc.rust-lang.org/stable/std/fs/index.html
/// [`path`]: #method.path
/// [`file_name`]: #method.file_name
/// [`follow_links`]: struct.WalkDir.html#method.follow_links
/// [`DirEntryExt`]: trait.DirEntryExt.html
pub struct DirEntry {
    /// The path as reported by the [`fs::ReadDir`] iterator (even if it's a
    /// symbolic link).
    ///
    /// [`fs::ReadDir`]: https://doc.rust-lang.org/stable/std/fs/struct.ReadDir.html
    path: PathBuf,
    /// The file type. Necessary for recursive iteration, so store it.
    ty: FileType,
    /// Is set when this entry was created from a symbolic link and the user
    /// excepts the iterator to follow symbolic links.
    follow_link: bool,
    /// The depth at which this entry was generated relative to the root.
    depth: usize,
    /// The underlying inode number (Unix only).
    #[cfg(unix)]
    ino: u64,
    /// The underlying metadata (Windows only). We store this on Windows
    /// because this comes for free while reading a directory.
    ///
    /// We use this to determine whether an entry is a directory or not, which
    /// works around a bug in Rust's standard library:
    /// https://github.com/rust-lang/rust/issues/46484
    #[cfg(windows)]
    metadata: fs::Metadata,
}

impl Iterator for IntoIter {
    type Item = Result<DirEntry>;
    /// Advances the iterator and returns the next value.
    ///
    /// # Errors
    ///
    /// If the iterator fails to retrieve the next value, this method returns
    /// an error value. The error will be wrapped in an Option::Some.
    fn next(&mut self) -> Option<Result<DirEntry>> {
        if let Some(start) = self.start.take() {
            let dent = itry!(DirEntry::from_link(0, start));
            if let Some(result) = self.handle_entry(dent) {
                return Some(result);
            }
        }
        while !self.stack_list.is_empty() {
            self.depth = self.stack_list.len();
            if let Some(dentry) = self.get_deferred_dir() {
                return Some(Ok(dentry));
            }
            if self.depth > self.opts.max_depth {
                // If we've exceeded the max depth, pop the current dir
                // so that we don't descend.
                self.pop();
                continue;
            }
            // Unwrap is safe here because we've verified above that
            // `self.stack_list` is not empty
            match self.stack_list.last_mut().expect("bug in walkdir").next() {
                None => self.pop(),
                Some(Err(err)) => return Some(Err(err)),
                Some(Ok(dent)) => {
                    if let Some(result) = self.handle_entry(dent) {
                        return Some(result);
                    }
                }
            }
        }
        if self.opts.contents_first {
            self.depth = self.stack_list.len();
            if let Some(dentry) = self.get_deferred_dir() {
                return Some(Ok(dentry));
            }
        }
        None
    }
}

impl IntoIter {
    /// Skips the current directory.
    ///
    /// This causes the iterator to stop traversing the contents of the least
    /// recently yielded directory. This means any remaining entries in that
    /// directory will be skipped (including sub-directories).
    ///
    /// Note that the ergonomics of this method are questionable since it
    /// borrows the iterator mutably. Namely, you must write out the looping
    /// condition manually. For example, to skip hidden entries efficiently on
    /// unix systems:
    ///
    /// ```no_run
    /// use walkdir::{DirEntry, WalkDir};
    ///
    /// fn is_hidden(entry: &DirEntry) -> bool {
    ///     entry.file_name()
    ///          .to_str()
    ///          .map(|s| s.starts_with("."))
    ///          .unwrap_or(false)
    /// }
    ///
    /// let mut it = WalkDir::new("foo").into_iter();
    /// loop {
    ///     let entry = match it.next() {
    ///         None => break,
    ///         Some(Err(err)) => panic!("ERROR: {}", err),
    ///         Some(Ok(entry)) => entry,
    ///     };
    ///     if is_hidden(&entry) {
    ///         if entry.file_type().is_dir() {
    ///             it.skip_current_dir();
    ///         }
    ///         continue;
    ///     }
    ///     println!("{}", entry.path().display());
    /// }
    /// ```
    ///
    /// You may find it more convenient to use the [`filter_entry`] iterator
    /// adapter. (See its documentation for the same example functionality as
    /// above.)
    ///
    /// [`filter_entry`]: #method.filter_entry
    pub fn skip_current_dir(&mut self) {
        if !self.stack_list.is_empty() {
            self.stack_list.pop();
        }
        if !self.stack_path.is_empty() {
            self.stack_path.pop();
        }
    }

    /// Yields only entries which satisfy the given predicate and skips
    /// descending into directories that do not satisfy the given predicate.
    ///
    /// The predicate is applied to all entries. If the predicate is
    /// true, iteration carries on as normal. If the predicate is false, the
    /// entry is ignored and if it is a directory, it is not descended into.
    ///
    /// This is often more convenient to use than [`skip_current_dir`]. For
    /// example, to skip hidden files and directories efficiently on unix
    /// systems:
    ///
    /// ```no_run
    /// use walkdir::{DirEntry, WalkDir};
    /// # use walkdir::Error;
    ///
    /// fn is_hidden(entry: &DirEntry) -> bool {
    ///     entry.file_name()
    ///          .to_str()
    ///          .map(|s| s.starts_with("."))
    ///          .unwrap_or(false)
    /// }
    ///
    /// # fn try_main() -> Result<(), Error> {
    /// for entry in WalkDir::new("foo")
    ///                      .into_iter()
    ///                      .filter_entry(|e| !is_hidden(e)) {
    ///     println!("{}", entry?.path().display());
    /// }
    /// # Ok(())
    /// # }
    /// ```
    ///
    /// Note that the iterator will still yield errors for reading entries that
    /// may not satisfy the predicate.
    ///
    /// Note that entries skipped with [`min_depth`] and [`max_depth`] are not
    /// passed to this predicate.
    ///
    /// Note that if the iterator has `contents_first` enabled, then this
    /// method is no different than calling the standard `Iterator::filter`
    /// method (because directory entries are yielded after they've been
    /// descended into).
    ///
    /// [`skip_current_dir`]: #method.skip_current_dir
    /// [`min_depth`]: struct.WalkDir.html#method.min_depth
    /// [`max_depth`]: struct.WalkDir.html#method.max_depth
    pub fn filter_entry<P>(self, predicate: P) -> FilterEntry<Self, P>
    where P: FnMut(&DirEntry) -> bool
    {
        FilterEntry { it: self, predicate: predicate }
    }

    fn handle_entry(
        &mut self,
        mut dent: DirEntry,
    ) -> Option<Result<DirEntry>> {
        if self.opts.follow_links && dent.file_type().is_symlink() {
            dent = itry!(self.follow(dent));
        }
        let is_normal_dir = !dent.file_type().is_symlink() && dent.is_dir();
        if is_normal_dir {
            itry!(self.push(&dent));
        }
        if is_normal_dir && self.opts.contents_first {
            self.deferred_dirs.push(dent);
            None
        } else if self.skippable() {
            None
        } else {
            Some(Ok(dent))
        }
    }

    fn get_deferred_dir(&mut self) -> Option<DirEntry> {
        if self.opts.contents_first {
            if self.depth < self.deferred_dirs.len() {
                // Unwrap is safe here because we've guaranteed that
                // `self.deferred_dirs.len()` can never be less than 1
                let deferred: DirEntry = self.deferred_dirs.pop()
                    .expect("bug in walkdir");
                if !self.skippable() {
                    return Some(deferred);
                }
            }
        }
        None
    }

    fn push(&mut self, dent: &DirEntry) -> Result<()> {
        // Make room for another open file descriptor if we've hit the max.
        let free = self.stack_list
            .len()
            .checked_sub(self.oldest_opened).unwrap();
        if free == self.opts.max_open {
            self.stack_list[self.oldest_opened].close();
            // Unwrap is safe here because self.oldest_opened is guaranteed to
            // never be greater than `self.stack_list.len()`, which implies
            // that the subtraction won't underflow and that adding 1 will
            // never overflow.
            self.oldest_opened = self.oldest_opened.checked_add(1).unwrap();
        }
        // Open a handle to reading the directory's entries.
        let rd = fs::read_dir(dent.path()).map_err(|err| {
            Some(Error::from_path(self.depth, dent.path().to_path_buf(), err))
        });
        let mut list = DirList::Opened { depth: self.depth, it: rd };
        if let Some(ref mut cmp) = self.opts.sorter {
            let mut entries: Vec<_> = list.collect();
            entries.sort_by(|a, b| {
                match (a, b) {
                    (&Ok(ref a), &Ok(ref b)) => {
                        cmp(a, b)
                    }
                    (&Err(_), &Err(_)) => Ordering::Equal,
                    (&Ok(_), &Err(_)) => Ordering::Greater,
                    (&Err(_), &Ok(_)) => Ordering::Less,
                }
            });
            list = DirList::Closed(entries.into_iter());
        }
        if self.opts.follow_links {
            let ancestor = Ancestor::new(&dent).map_err(|err| {
                Error::from_io(self.depth, err)
            })?;
            self.stack_path.push(ancestor);
        }
        // We push this after stack_path since creating the Ancestor can fail.
        // If it fails, then we return the error and won't descend.
        self.stack_list.push(list);
        Ok(())
    }

    fn pop(&mut self) {
        self.stack_list.pop().expect("cannot pop from empty stack");
        if self.opts.follow_links {
            self.stack_path.pop().expect("BUG: list/path stacks out of sync");
        }
        // If everything in the stack is already closed, then there is
        // room for at least one more open descriptor and it will
        // always be at the top of the stack.
        self.oldest_opened = min(self.oldest_opened, self.stack_list.len());
    }

    fn follow(&self, mut dent: DirEntry) -> Result<DirEntry> {
        dent = DirEntry::from_link(self.depth, dent.path().to_path_buf())?;
        // The only way a symlink can cause a loop is if it points
        // to a directory. Otherwise, it always points to a leaf
        // and we can omit any loop checks.
        if dent.is_dir() {
            self.check_loop(dent.path())?;
        }
        Ok(dent)
    }

    fn check_loop<P: AsRef<Path>>(&self, child: P) -> Result<()> {
        let hchild = Handle::from_path(&child).map_err(|err| {
            Error::from_io(self.depth, err)
        })?;
        for ancestor in self.stack_path.iter().rev() {
            let is_same = ancestor.is_same(&hchild).map_err(|err| {
                Error::from_io(self.depth, err)
            })?;
            if is_same {
                return Err(Error {
                    depth: self.depth,
                    inner: ErrorInner::Loop {
                        ancestor: ancestor.path.to_path_buf(),
                        child: child.as_ref().to_path_buf(),
                    },
                });
            }
        }
        Ok(())
    }

    fn skippable(&self) -> bool {
        self.depth < self.opts.min_depth || self.depth > self.opts.max_depth
    }
}

impl DirList {
    fn close(&mut self) {
        if let DirList::Opened { .. } = *self {
            *self = DirList::Closed(self.collect::<Vec<_>>().into_iter());
        }
    }
}

impl Iterator for DirList {
    type Item = Result<DirEntry>;

    #[inline(always)]
    fn next(&mut self) -> Option<Result<DirEntry>> {
        match *self {
            DirList::Closed(ref mut it) => it.next(),
            DirList::Opened { depth, ref mut it } => match *it {
                Err(ref mut err) => err.take().map(Err),
                Ok(ref mut rd) => rd.next().map(|r| match r {
                    Ok(r) => DirEntry::from_entry(depth + 1, &r),
                    Err(err) => Err(Error::from_io(depth + 1, err))
                }),
            }
        }
    }
}

impl DirEntry {
    /// The full path that this entry represents.
    ///
    /// The full path is created by joining the parents of this entry up to the
    /// root initially given to [`WalkDir::new`] with the file name of this
    /// entry.
    ///
    /// Note that this *always* returns the path reported by the underlying
    /// directory entry, even when symbolic links are followed. To get the
    /// target path, use [`path_is_symlink`] to (cheaply) check if this entry
    /// corresponds to a symbolic link, and [`std::fs::read_link`] to resolve
    /// the target.
    ///
    /// [`WalkDir::new`]: struct.WalkDir.html#method.new
    /// [`path_is_symlink`]: struct.DirEntry.html#method.path_is_symlink
    /// [`std::fs::read_link`]: https://doc.rust-lang.org/stable/std/fs/fn.read_link.html
    pub fn path(&self) -> &Path {
        &self.path
    }

    /// Returns `true` if and only if this entry was created from a symbolic
    /// link. This is unaffected by the [`follow_links`] setting.
    ///
    /// When `true`, the value returned by the [`path`] method is a
    /// symbolic link name. To get the full target path, you must call
    /// [`std::fs::read_link(entry.path())`].
    ///
    /// [`path`]: struct.DirEntry.html#method.path
    /// [`follow_links`]: struct.WalkDir.html#method.follow_links
    /// [`std::fs::read_link(entry.path())`]: https://doc.rust-lang.org/stable/std/fs/fn.read_link.html
    pub fn path_is_symlink(&self) -> bool {
        self.ty.is_symlink() || self.follow_link
    }

    /// Return the metadata for the file that this entry points to.
    ///
    /// This will follow symbolic links if and only if the [`WalkDir`] value
    /// has [`follow_links`] enabled.
    ///
    /// # Platform behavior
    ///
    /// This always calls [`std::fs::symlink_metadata`].
    ///
    /// If this entry is a symbolic link and [`follow_links`] is enabled, then
    /// [`std::fs::metadata`] is called instead.
    ///
    /// # Errors
    ///
    /// Similar to [`std::fs::metadata`], returns errors for path values that
    /// the program does not have permissions to access or if the path does not
    /// exist.
    ///
    /// [`WalkDir`]: struct.WalkDir.html
    /// [`follow_links`]: struct.WalkDir.html#method.follow_links
    /// [`std::fs::metadata`]: https://doc.rust-lang.org/std/fs/fn.metadata.html
    /// [`std::fs::symlink_metadata`]: https://doc.rust-lang.org/stable/std/fs/fn.symlink_metadata.html
    pub fn metadata(&self) -> Result<fs::Metadata> {
        self.metadata_internal()
    }

    #[cfg(windows)]
    fn metadata_internal(&self) -> Result<fs::Metadata> {
        if self.follow_link {
            fs::metadata(&self.path)
        } else {
            Ok(self.metadata.clone())
        }.map_err(|err| Error::from_entry(self, err))
    }

    #[cfg(not(windows))]
    fn metadata_internal(&self) -> Result<fs::Metadata> {
        if self.follow_link {
            fs::metadata(&self.path)
        } else {
            fs::symlink_metadata(&self.path)
        }.map_err(|err| Error::from_entry(self, err))
    }

    /// Return the file type for the file that this entry points to.
    ///
    /// If this is a symbolic link and [`follow_links`] is `true`, then this
    /// returns the type of the target.
    ///
    /// This never makes any system calls.
    ///
    /// [`follow_links`]: struct.WalkDir.html#method.follow_links
    pub fn file_type(&self) -> fs::FileType {
        self.ty
    }

    /// Return the file name of this entry.
    ///
    /// If this entry has no file name (e.g., `/`), then the full path is
    /// returned.
    pub fn file_name(&self) -> &OsStr {
        self.path.file_name().unwrap_or_else(|| self.path.as_os_str())
    }

    /// Returns the depth at which this entry was created relative to the root.
    ///
    /// The smallest depth is `0` and always corresponds to the path given
    /// to the `new` function on `WalkDir`. Its direct descendents have depth
    /// `1`, and their descendents have depth `2`, and so on.
    pub fn depth(&self) -> usize {
        self.depth
    }

    /// Returns true if and only if this entry points to a directory.
    ///
    /// This works around a bug in Rust's standard library:
    /// https://github.com/rust-lang/rust/issues/46484
    #[cfg(windows)]
    fn is_dir(&self) -> bool {
        use std::os::windows::fs::MetadataExt;
        use winapi::um::winnt::FILE_ATTRIBUTE_DIRECTORY;
        self.metadata.file_attributes() & FILE_ATTRIBUTE_DIRECTORY != 0
    }

    /// Returns true if and only if this entry points to a directory.
    #[cfg(not(windows))]
    fn is_dir(&self) -> bool {
        self.ty.is_dir()
    }

    #[cfg(windows)]
    fn from_entry(depth: usize, ent: &fs::DirEntry) -> Result<DirEntry> {
        let path = ent.path();
        let ty = ent.file_type().map_err(|err| {
            Error::from_path(depth, path.clone(), err)
        })?;
        let md = ent.metadata().map_err(|err| {
            Error::from_path(depth, path.clone(), err)
        })?;
        Ok(DirEntry {
            path: path,
            ty: ty,
            follow_link: false,
            depth: depth,
            metadata: md,
        })
    }

    #[cfg(unix)]
    fn from_entry(depth: usize, ent: &fs::DirEntry) -> Result<DirEntry> {
        use std::os::unix::fs::DirEntryExt;

        let ty = ent.file_type().map_err(|err| {
            Error::from_path(depth, ent.path(), err)
        })?;
        Ok(DirEntry {
            path: ent.path(),
            ty: ty,
            follow_link: false,
            depth: depth,
            ino: ent.ino(),
        })
    }

    #[cfg(not(any(unix, windows)))]
    fn from_entry(depth: usize, ent: &fs::DirEntry) -> Result<DirEntry> {
        use std::os::unix::fs::DirEntryExt;

        let ty = ent.file_type().map_err(|err| {
            Error::from_path(depth, ent.path(), err)
        })?;
        Ok(DirEntry {
            path: ent.path(),
            ty: ty,
            follow_link: false,
            depth: depth,
        })
    }

    #[cfg(windows)]
    fn from_link(depth: usize, pb: PathBuf) -> Result<DirEntry> {
        let md = fs::metadata(&pb).map_err(|err| {
            Error::from_path(depth, pb.clone(), err)
        })?;
        Ok(DirEntry {
            path: pb,
            ty: md.file_type(),
            follow_link: true,
            depth: depth,
            metadata: md,
        })
    }

    #[cfg(unix)]
    fn from_link(depth: usize, pb: PathBuf) -> Result<DirEntry> {
        use std::os::unix::fs::MetadataExt;

        let md = fs::metadata(&pb).map_err(|err| {
            Error::from_path(depth, pb.clone(), err)
        })?;
        Ok(DirEntry {
            path: pb,
            ty: md.file_type(),
            follow_link: true,
            depth: depth,
            ino: md.ino(),
        })
    }

    #[cfg(not(any(unix, windows)))]
    fn from_link(depth: usize, pb: PathBuf) -> Result<DirEntry> {
        use std::os::unix::fs::MetadataExt;

        let md = fs::metadata(&pb).map_err(|err| {
            Error::from_path(depth, pb.clone(), err)
        })?;
        Ok(DirEntry {
            path: pb,
            ty: md.file_type(),
            follow_link: true,
            depth: depth,
        })
    }
}

impl Clone for DirEntry {
    #[cfg(windows)]
    fn clone(&self) -> DirEntry {
        DirEntry {
            path: self.path.clone(),
            ty: self.ty,
            follow_link: self.follow_link,
            depth: self.depth,
            metadata: self.metadata.clone(),
        }
    }

    #[cfg(unix)]
    fn clone(&self) -> DirEntry {
        DirEntry {
            path: self.path.clone(),
            ty: self.ty,
            follow_link: self.follow_link,
            depth: self.depth,
            ino: self.ino,
        }
    }

    #[cfg(not(any(unix, windows)))]
    fn clone(&self) -> DirEntry {
        DirEntry {
            path: self.path.clone(),
            ty: self.ty,
            follow_link: self.follow_link,
            depth: self.depth,
            ino: self.ino,
        }
    }
}

impl fmt::Debug for DirEntry {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "DirEntry({:?})", self.path)
    }
}

/// A recursive directory iterator that skips entries.
///
/// Values of this type are created by calling [`.filter_entry()`] on an
/// `IntoIter`, which is formed by calling [`.into_iter()`] on a `WalkDir`.
///
/// Directories that fail the predicate `P` are skipped. Namely, they are
/// never yielded and never descended into.
///
/// Entries that are skipped with the [`min_depth`] and [`max_depth`] options
/// are not passed through this filter.
///
/// If opening a handle to a directory resulted in an error, then it is yielded
/// and no corresponding call to the predicate is made.
///
/// Type parameter `I` refers to the underlying iterator and `P` refers to the
/// predicate, which is usually `FnMut(&DirEntry) -> bool`.
///
/// [`.filter_entry()`]: struct.IntoIter.html#method.filter_entry
/// [`.into_iter()`]: struct.WalkDir.html#into_iter.v
/// [`min_depth`]: struct.WalkDir.html#method.min_depth
/// [`max_depth`]: struct.WalkDir.html#method.max_depth
#[derive(Debug)]
pub struct FilterEntry<I, P> {
    it: I,
    predicate: P,
}

impl<P> Iterator for FilterEntry<IntoIter, P>
where P: FnMut(&DirEntry) -> bool
{
    type Item = Result<DirEntry>;

    /// Advances the iterator and returns the next value.
    ///
    /// # Errors
    ///
    /// If the iterator fails to retrieve the next value, this method returns
    /// an error value. The error will be wrapped in an `Option::Some`.
    fn next(&mut self) -> Option<Result<DirEntry>> {
        loop {
            let dent = match self.it.next() {
                None => return None,
                Some(result) => itry!(result),
            };
            if !(self.predicate)(&dent) {
                if dent.is_dir() {
                    self.it.skip_current_dir();
                }
                continue;
            }
            return Some(Ok(dent));
        }
    }
}

impl<P> FilterEntry<IntoIter, P> where P: FnMut(&DirEntry) -> bool {
    /// Yields only entries which satisfy the given predicate and skips
    /// descending into directories that do not satisfy the given predicate.
    ///
    /// The predicate is applied to all entries. If the predicate is
    /// true, iteration carries on as normal. If the predicate is false, the
    /// entry is ignored and if it is a directory, it is not descended into.
    ///
    /// This is often more convenient to use than [`skip_current_dir`]. For
    /// example, to skip hidden files and directories efficiently on unix
    /// systems:
    ///
    /// ```no_run
    /// use walkdir::{DirEntry, WalkDir};
    /// # use walkdir::Error;
    ///
    /// fn is_hidden(entry: &DirEntry) -> bool {
    ///     entry.file_name()
    ///          .to_str()
    ///          .map(|s| s.starts_with("."))
    ///          .unwrap_or(false)
    /// }
    ///
    /// # fn try_main() -> Result<(), Error> {
    /// for entry in WalkDir::new("foo")
    ///                      .into_iter()
    ///                      .filter_entry(|e| !is_hidden(e)) {
    ///     println!("{}", entry?.path().display());
    /// }
    /// # Ok(())
    /// # }
    /// ```
    ///
    /// Note that the iterator will still yield errors for reading entries that
    /// may not satisfy the predicate.
    ///
    /// Note that entries skipped with [`min_depth`] and [`max_depth`] are not
    /// passed to this predicate.
    ///
    /// Note that if the iterator has `contents_first` enabled, then this
    /// method is no different than calling the standard `Iterator::filter`
    /// method (because directory entries are yielded after they've been
    /// descended into).
    ///
    /// [`skip_current_dir`]: #method.skip_current_dir
    /// [`min_depth`]: struct.WalkDir.html#method.min_depth
    /// [`max_depth`]: struct.WalkDir.html#method.max_depth
    pub fn filter_entry(self, predicate: P) -> FilterEntry<Self, P> {
        FilterEntry { it: self, predicate: predicate }
    }

    /// Skips the current directory.
    ///
    /// This causes the iterator to stop traversing the contents of the least
    /// recently yielded directory. This means any remaining entries in that
    /// directory will be skipped (including sub-directories).
    ///
    /// Note that the ergonomics of this method are questionable since it
    /// borrows the iterator mutably. Namely, you must write out the looping
    /// condition manually. For example, to skip hidden entries efficiently on
    /// unix systems:
    ///
    /// ```no_run
    /// use walkdir::{DirEntry, WalkDir};
    ///
    /// fn is_hidden(entry: &DirEntry) -> bool {
    ///     entry.file_name()
    ///          .to_str()
    ///          .map(|s| s.starts_with("."))
    ///          .unwrap_or(false)
    /// }
    ///
    /// let mut it = WalkDir::new("foo").into_iter();
    /// loop {
    ///     let entry = match it.next() {
    ///         None => break,
    ///         Some(Err(err)) => panic!("ERROR: {}", err),
    ///         Some(Ok(entry)) => entry,
    ///     };
    ///     if is_hidden(&entry) {
    ///         if entry.file_type().is_dir() {
    ///             it.skip_current_dir();
    ///         }
    ///         continue;
    ///     }
    ///     println!("{}", entry.path().display());
    /// }
    /// ```
    ///
    /// You may find it more convenient to use the [`filter_entry`] iterator
    /// adapter. (See its documentation for the same example functionality as
    /// above.)
    ///
    /// [`filter_entry`]: #method.filter_entry
    pub fn skip_current_dir(&mut self) {
        self.it.skip_current_dir();
    }
}

/// An error produced by recursively walking a directory.
///
/// This error type is a light wrapper around [`std::io::Error`]. In
/// particular, it adds the following information:
///
/// * The depth at which the error occurred in the file tree, relative to the
/// root.
/// * The path, if any, associated with the IO error.
/// * An indication that a loop occurred when following symbolic links. In this
/// case, there is no underlying IO error.
///
/// To maintain good ergonomics, this type has a
/// [`impl From<Error> for std::io::Error`][impl] defined which preserves the original context.
/// This allows you to use an [`io::Result`] with methods in this crate if you don't care about
/// accessing the underlying error data in a structured form.
///
/// [`std::io::Error`]: https://doc.rust-lang.org/stable/std/io/struct.Error.html
/// [`io::Result`]: https://doc.rust-lang.org/stable/std/io/type.Result.html
/// [impl]: struct.Error.html#impl-From%3CError%3E
#[derive(Debug)]
pub struct Error {
    depth: usize,
    inner: ErrorInner,
}

#[derive(Debug)]
enum ErrorInner {
    Io { path: Option<PathBuf>, err: io::Error },
    Loop { ancestor: PathBuf, child: PathBuf },
}

impl Error {
    /// Returns the path associated with this error if one exists.
    ///
    /// For example, if an error occurred while opening a directory handle,
    /// the error will include the path passed to [`std::fs::read_dir`].
    ///
    /// [`std::fs::read_dir`]: https://doc.rust-lang.org/stable/std/fs/fn.read_dir.html
    pub fn path(&self) -> Option<&Path> {
        match self.inner {
            ErrorInner::Io { path: None, .. } => None,
            ErrorInner::Io { path: Some(ref path), .. } => Some(path),
            ErrorInner::Loop { ref child, .. } => Some(child),
        }
    }

    /// Returns the path at which a cycle was detected.
    ///
    /// If no cycle was detected, [`None`] is returned.
    ///
    /// A cycle is detected when a directory entry is equivalent to one of
    /// its ancestors.
    ///
    /// To get the path to the child directory entry in the cycle, use the
    /// [`path`] method.
    ///
    /// [`None`]: https://doc.rust-lang.org/stable/std/option/enum.Option.html#variant.None
    /// [`path`]: struct.Error.html#path
    pub fn loop_ancestor(&self) -> Option<&Path> {
        match self.inner {
            ErrorInner::Loop { ref ancestor, .. } => Some(ancestor),
            _ => None,
        }
    }

    /// Returns the depth at which this error occurred relative to the root.
    ///
    /// The smallest depth is `0` and always corresponds to the path given to
    /// the [`new`] function on [`WalkDir`]. Its direct descendents have depth
    /// `1`, and their descendents have depth `2`, and so on.
    ///
    /// [`new`]: struct.WalkDir.html#method.new
    /// [`WalkDir`]: struct.WalkDir.html
    pub fn depth(&self) -> usize {
        self.depth
    }

    /// Inspect the original [`io::Error`] if there is one.
    ///
    /// [`None`] is returned if the [`Error`] doesn't correspond to an
    /// [`io::Error`]. This might happen, for example, when the error was
    /// produced because a cycle was found in the directory tree while
    /// following symbolic links.
    ///
    /// This method returns a borrowed value that is bound to the lifetime of the [`Error`]. To
    /// obtain an owned value, the [`into_io_error`] can be used instead.
    ///
    /// > This is the original [`io::Error`] and is _not_ the same as
    /// > [`impl From<Error> for std::io::Error`][impl] which contains additional context about the
    /// error.
    ///
    /// # Example
    ///
    /// ```rust,no-run
    /// use std::io;
    /// use std::path::Path;
    ///
    /// use walkdir::WalkDir;
    ///
    /// for entry in WalkDir::new("foo") {
    ///     match entry {
    ///         Ok(entry) => println!("{}", entry.path().display()),
    ///         Err(err) => {
    ///             let path = err.path().unwrap_or(Path::new("")).display();
    ///             println!("failed to access entry {}", path);
    ///             if let Some(inner) = err.io_error() {
    ///                 match inner.kind() {
    ///                     io::ErrorKind::InvalidData => {
    ///                         println!(
    ///                             "entry contains invalid data: {}",
    ///                             inner)
    ///                     }
    ///                     io::ErrorKind::PermissionDenied => {
    ///                         println!(
    ///                             "Missing permission to read entry: {}",
    ///                             inner)
    ///                     }
    ///                     _ => {
    ///                         println!(
    ///                             "Unexpected error occurred: {}",
    ///                             inner)
    ///                     }
    ///                 }
    ///             }
    ///         }
    ///     }
    /// }
    /// ```
    ///
    /// [`None`]: https://doc.rust-lang.org/stable/std/option/enum.Option.html#variant.None
    /// [`io::Error`]: https://doc.rust-lang.org/stable/std/io/struct.Error.html
    /// [`From`]: https://doc.rust-lang.org/stable/std/convert/trait.From.html
    /// [`Error`]: struct.Error.html
    /// [`into_io_error`]: struct.Error.html#method.into_io_error
    /// [impl]: struct.Error.html#impl-From%3CError%3E
    pub fn io_error(&self) -> Option<&io::Error> {
       match self.inner {
            ErrorInner::Io { ref err, .. } => Some(err),
            ErrorInner::Loop { .. } => None,
       }
    }

    /// Similar to [`io_error`] except consumes self to convert to the original
    /// [`io::Error`] if one exists.
    ///
    /// [`io_error`]: struct.Error.html#method.io_error
    /// [`io::Error`]: https://doc.rust-lang.org/stable/std/io/struct.Error.html
    pub fn into_io_error(self) -> Option<io::Error> {
       match self.inner {
            ErrorInner::Io { err, .. } => Some(err),
            ErrorInner::Loop { .. } => None,
       }
    }

    fn from_path(depth: usize, pb: PathBuf, err: io::Error) -> Self {
        Error {
            depth: depth,
            inner: ErrorInner::Io { path: Some(pb), err: err },
        }
    }

    fn from_entry(dent: &DirEntry, err: io::Error) -> Self {
        Error {
            depth: dent.depth,
            inner: ErrorInner::Io {
                path: Some(dent.path().to_path_buf()),
                err: err,
            },
        }
    }

    fn from_io(depth: usize, err: io::Error) -> Self {
        Error {
            depth: depth,
            inner: ErrorInner::Io { path: None, err: err },
        }
    }
}

impl error::Error for Error {
    fn description(&self) -> &str {
        match self.inner {
            ErrorInner::Io { ref err, .. } => err.description(),
            ErrorInner::Loop { .. } => "file system loop found",
        }
    }

    fn cause(&self) -> Option<&error::Error> {
        match self.inner {
            ErrorInner::Io { ref err, .. } => Some(err),
            ErrorInner::Loop { .. } => None,
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.inner {
            ErrorInner::Io { path: None, ref err } => {
                err.fmt(f)
            }
            ErrorInner::Io { path: Some(ref path), ref err } => {
                write!(f, "IO error for operation on {}: {}",
                       path.display(), err)
            }
            ErrorInner::Loop { ref ancestor, ref child } => {
                write!(f, "File system loop found: \
                           {} points to an ancestor {}",
                       child.display(), ancestor.display())
            }
        }
    }
}

impl From<Error> for io::Error {
    /// Convert the [`Error`] to an [`io::Error`], preserving the original [`Error`] as the ["inner
    /// error"]. Note that this also makes the display of the error include the context.
    ///
    /// This is different from [`into_io_error`] which returns the original [`io::Error`].
    ///
    /// [`Error`]: struct.Error.html
    /// [`io::Error`]: https://doc.rust-lang.org/stable/std/io/struct.Error.html
    /// ["inner error"]: https://doc.rust-lang.org/std/io/struct.Error.html#method.into_inner
    /// [`into_io_error`]: struct.WalkDir.html#method.into_io_error
    fn from(walk_err: Error) -> io::Error {
        let kind = match walk_err {
            Error { inner: ErrorInner::Io { ref err, .. }, .. } => {
                err.kind()
            }
            Error { inner: ErrorInner::Loop { .. }, .. } => {
                io::ErrorKind::Other
            }
        };
        io::Error::new(kind, walk_err)
    }
}
