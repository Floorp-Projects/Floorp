//! Unix path manipulation.
//!
//! This crate provides two types, [`PathBuf`] and [`Path`] (akin to `String`
//! and `str`), for working with paths abstractly. These types are thin wrappers
//! around `UnixString` and `UnixStr` respectively, meaning that they work
//! directly on strings independently from the local platform's path syntax.
//!
//! Paths can be parsed into [`Component`]s by iterating over the structure
//! returned by the [`components`] method on [`Path`]. [`Component`]s roughly
//! correspond to the substrings between path separators (`/`). You can
//! reconstruct an equivalent path from components with the [`push`] method on
//! [`PathBuf`]; note that the paths may differ syntactically by the
//! normalization described in the documentation for the [`components`] method.
//!
//! ## Simple usage
//!
//! Path manipulation includes both parsing components from slices and building
//! new owned paths.
//!
//! To parse a path, you can create a [`Path`] slice from a `str`
//! slice and start asking questions:
//!
//! ```
//! use unix_path::Path;
//! use unix_str::UnixStr;
//!
//! let path = Path::new("/tmp/foo/bar.txt");
//!
//! let parent = path.parent();
//! assert_eq!(parent, Some(Path::new("/tmp/foo")));
//!
//! let file_stem = path.file_stem();
//! assert_eq!(file_stem, Some(UnixStr::new("bar")));
//!
//! let extension = path.extension();
//! assert_eq!(extension, Some(UnixStr::new("txt")));
//! ```
//!
//! To build or modify paths, use [`PathBuf`]:
//!
//! ```
//! use unix_path::PathBuf;
//!
//! // This way works...
//! let mut path = PathBuf::from("/");
//!
//! path.push("feel");
//! path.push("the");
//!
//! path.set_extension("force");
//!
//! // ... but push is best used if you don't know everything up
//! // front. If you do, this way is better:
//! let path: PathBuf = ["/", "feel", "the.force"].iter().collect();
//! ```
//!
//! [`Component`]: enum.Component.html
//! [`components`]:struct.Path.html#method.components
//! [`PathBuf`]: struct.PathBuf.html
//! [`Path`]: struct.Path.html
//! [`push`]: struct.PathBuf.html#method.push

#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(feature = "shrink_to", feature(shrink_to))]

#[cfg(feature = "alloc")]
extern crate alloc;

use unix_str::UnixStr;
#[cfg(feature = "alloc")]
use unix_str::UnixString;

#[cfg(feature = "alloc")]
use core::borrow::Borrow;
use core::cmp;
use core::fmt;
use core::hash::{Hash, Hasher};
#[cfg(feature = "alloc")]
use core::iter;
use core::iter::FusedIterator;
#[cfg(feature = "alloc")]
use core::ops::{self, Deref};

#[cfg(feature = "alloc")]
use alloc::{
    borrow::{Cow, ToOwned},
    boxed::Box,
    rc::Rc,
    str::FromStr,
    string::String,
    sync::Arc,
    vec::Vec,
};

#[cfg(feature = "std")]
use std::error::Error;

mod lossy;

////////////////////////////////////////////////////////////////////////////////
// Exposed parsing helpers
////////////////////////////////////////////////////////////////////////////////

/// Determines whether the character is the permitted path separator for Unix,
/// `/`.
///
/// # Examples
///
/// ```
/// assert!(unix_path::is_separator('/'));
/// assert!(!unix_path::is_separator('❤'));
/// ```
pub fn is_separator(c: char) -> bool {
    c == '/'
}

/// The separator of path components for Unix, `/`.
pub const MAIN_SEPARATOR: char = '/';

////////////////////////////////////////////////////////////////////////////////
// Misc helpers
////////////////////////////////////////////////////////////////////////////////

// Iterate through `iter` while it matches `prefix`; return `None` if `prefix`
// is not a prefix of `iter`, otherwise return `Some(iter_after_prefix)` giving
// `iter` after having exhausted `prefix`.
fn iter_after<'a, 'b, I, J>(mut iter: I, mut prefix: J) -> Option<I>
where
    I: Iterator<Item = Component<'a>> + Clone,
    J: Iterator<Item = Component<'b>>,
{
    loop {
        let mut iter_next = iter.clone();
        match (iter_next.next(), prefix.next()) {
            (Some(ref x), Some(ref y)) if x == y => (),
            (Some(_), Some(_)) => return None,
            (Some(_), None) => return Some(iter),
            (None, None) => return Some(iter),
            (None, Some(_)) => return None,
        }
        iter = iter_next;
    }
}

fn unix_str_as_u8_slice(s: &UnixStr) -> &[u8] {
    unsafe { &*(s as *const UnixStr as *const [u8]) }
}
unsafe fn u8_slice_as_unix_str(s: &[u8]) -> &UnixStr {
    &*(s as *const [u8] as *const UnixStr)
}

////////////////////////////////////////////////////////////////////////////////
// Cross-platform, iterator-independent parsing
////////////////////////////////////////////////////////////////////////////////

/// Says whether the first byte after the prefix is a separator.
fn has_physical_root(path: &[u8]) -> bool {
    !path.is_empty() && path[0] == b'/'
}

// basic workhorse for splitting stem and extension
fn split_file_at_dot(file: &UnixStr) -> (Option<&UnixStr>, Option<&UnixStr>) {
    unsafe {
        if unix_str_as_u8_slice(file) == b".." {
            return (Some(file), None);
        }

        // The unsafety here stems from converting between &OsStr and &[u8]
        // and back. This is safe to do because (1) we only look at ASCII
        // contents of the encoding and (2) new &OsStr values are produced
        // only from ASCII-bounded slices of existing &OsStr values.

        let mut iter = unix_str_as_u8_slice(file).rsplitn(2, |b| *b == b'.');
        let after = iter.next();
        let before = iter.next();
        if before == Some(b"") {
            (Some(file), None)
        } else {
            (
                before.map(|s| u8_slice_as_unix_str(s)),
                after.map(|s| u8_slice_as_unix_str(s)),
            )
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// The core iterators
////////////////////////////////////////////////////////////////////////////////

/// Component parsing works by a double-ended state machine; the cursors at the
/// front and back of the path each keep track of what parts of the path have
/// been consumed so far.
///
/// Going front to back, a path is made up of a prefix, a starting
/// directory component, and a body (of normal components)
#[derive(Copy, Clone, PartialEq, PartialOrd, Debug)]
enum State {
    Prefix = 0,
    StartDir = 1, // / or . or nothing
    Body = 2,     // foo/bar/baz
    Done = 3,
}

/// A single component of a path.
///
/// A `Component` roughly corresponds to a substring between path separators
/// (`/`).
///
/// This `enum` is created by iterating over [`Components`], which in turn is
/// created by the [`components`][`Path::components`] method on [`Path`].
///
/// # Examples
///
/// ```rust
/// use unix_path::{Component, Path};
///
/// let path = Path::new("/tmp/foo/bar.txt");
/// let components = path.components().collect::<Vec<_>>();
/// assert_eq!(&components, &[
///     Component::RootDir,
///     Component::Normal("tmp".as_ref()),
///     Component::Normal("foo".as_ref()),
///     Component::Normal("bar.txt".as_ref()),
/// ]);
/// ```
///
/// [`Components`]: struct.Components.html
/// [`Path`]: struct.Path.html
/// [`Path::components`]: struct.Path.html#method.components
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Component<'a> {
    /// The root directory component, appears after any prefix and before anything else.
    ///
    /// It represents a separator that designates that a path starts from root.
    RootDir,

    /// A reference to the current directory, i.e., `.`.
    CurDir,

    /// A reference to the parent directory, i.e., `..`.
    ParentDir,

    /// A normal component, e.g., `a` and `b` in `a/b`.
    ///
    /// This variant is the most common one, it represents references to files
    /// or directories.
    Normal(&'a UnixStr),
}

impl<'a> Component<'a> {
    /// Extracts the underlying `UnixStr` slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// let path = Path::new("./tmp/foo/bar.txt");
    /// let components: Vec<_> = path.components().map(|comp| comp.as_unix_str()).collect();
    /// assert_eq!(&components, &[".", "tmp", "foo", "bar.txt"]);
    /// ```
    pub fn as_unix_str(self) -> &'a UnixStr {
        match self {
            Component::RootDir => UnixStr::new("/"),
            Component::CurDir => UnixStr::new("."),
            Component::ParentDir => UnixStr::new(".."),
            Component::Normal(path) => path,
        }
    }
}

impl AsRef<UnixStr> for Component<'_> {
    fn as_ref(&self) -> &UnixStr {
        self.as_unix_str()
    }
}

impl AsRef<Path> for Component<'_> {
    fn as_ref(&self) -> &Path {
        self.as_unix_str().as_ref()
    }
}

/// An iterator over the [`Component`]s of a [`Path`].
///
/// This `struct` is created by the [`components`] method on [`Path`].
/// See its documentation for more.
///
/// # Examples
///
/// ```
/// use unix_path::Path;
///
/// let path = Path::new("/tmp/foo/bar.txt");
///
/// for component in path.components() {
///     println!("{:?}", component);
/// }
/// ```
///
/// [`Component`]: enum.Component.html
/// [`components`]: struct.Path.html#method.components
/// [`Path`]: struct.Path.html
#[derive(Clone)]
pub struct Components<'a> {
    // The path left to parse components from
    path: &'a [u8],

    // true if path *physically* has a root separator;.
    has_physical_root: bool,

    // The iterator is double-ended, and these two states keep track of what has
    // been produced from either end
    front: State,
    back: State,
}

/// An iterator over the [`Component`]s of a [`Path`], as `UnixStr` slices.
///
/// This `struct` is created by the [`iter`] method on [`Path`].
/// See its documentation for more.
///
/// [`Component`]: enum.Component.html
/// [`iter`]: struct.Path.html#method.iter
/// [`Path`]: struct.Path.html
#[derive(Clone)]
pub struct Iter<'a> {
    inner: Components<'a>,
}

impl fmt::Debug for Components<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        struct DebugHelper<'a>(&'a Path);

        impl fmt::Debug for DebugHelper<'_> {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.debug_list().entries(self.0.components()).finish()
            }
        }

        f.debug_tuple("Components")
            .field(&DebugHelper(self.as_path()))
            .finish()
    }
}

impl<'a> Components<'a> {
    // Given the iteration so far, how much of the pre-State::Body path is left?
    #[inline]
    fn len_before_body(&self) -> usize {
        let root = if self.front <= State::StartDir && self.has_physical_root {
            1
        } else {
            0
        };
        let cur_dir = if self.front <= State::StartDir && self.include_cur_dir() {
            1
        } else {
            0
        };
        root + cur_dir
    }

    // is the iteration complete?
    #[inline]
    fn finished(&self) -> bool {
        self.front == State::Done || self.back == State::Done || self.front > self.back
    }

    #[inline]
    fn is_sep_byte(&self, b: u8) -> bool {
        b == b'/'
    }

    /// Extracts a slice corresponding to the portion of the path remaining for iteration.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// let mut components = Path::new("/tmp/foo/bar.txt").components();
    /// components.next();
    /// components.next();
    ///
    /// assert_eq!(Path::new("foo/bar.txt"), components.as_path());
    /// ```
    pub fn as_path(&self) -> &'a Path {
        let mut comps = self.clone();
        if comps.front == State::Body {
            comps.trim_left();
        }
        if comps.back == State::Body {
            comps.trim_right();
        }
        unsafe { Path::from_u8_slice(comps.path) }
    }

    /// Is the *original* path rooted?
    fn has_root(&self) -> bool {
        self.has_physical_root
    }

    /// Should the normalized path include a leading . ?
    fn include_cur_dir(&self) -> bool {
        if self.has_root() {
            return false;
        }
        let mut iter = self.path[..].iter();
        match (iter.next(), iter.next()) {
            (Some(&b'.'), None) => true,
            (Some(&b'.'), Some(&b)) => self.is_sep_byte(b),
            _ => false,
        }
    }

    // parse a given byte sequence into the corresponding path component
    fn parse_single_component<'b>(&self, comp: &'b [u8]) -> Option<Component<'b>> {
        match comp {
            b"." => None, // . components are normalized away, except at
            // the beginning of a path, which is treated
            // separately via `include_cur_dir`
            b".." => Some(Component::ParentDir),
            b"" => None,
            _ => Some(Component::Normal(unsafe { u8_slice_as_unix_str(comp) })),
        }
    }

    // parse a component from the left, saying how many bytes to consume to
    // remove the component
    fn parse_next_component(&self) -> (usize, Option<Component<'a>>) {
        debug_assert!(self.front == State::Body);
        let (extra, comp) = match self.path.iter().position(|b| self.is_sep_byte(*b)) {
            None => (0, self.path),
            Some(i) => (1, &self.path[..i]),
        };
        (comp.len() + extra, self.parse_single_component(comp))
    }

    // parse a component from the right, saying how many bytes to consume to
    // remove the component
    fn parse_next_component_back(&self) -> (usize, Option<Component<'a>>) {
        debug_assert!(self.back == State::Body);
        let start = self.len_before_body();
        let (extra, comp) = match self.path[start..]
            .iter()
            .rposition(|b| self.is_sep_byte(*b))
        {
            None => (0, &self.path[start..]),
            Some(i) => (1, &self.path[start + i + 1..]),
        };
        (comp.len() + extra, self.parse_single_component(comp))
    }

    // trim away repeated separators (i.e., empty components) on the left
    fn trim_left(&mut self) {
        while !self.path.is_empty() {
            let (size, comp) = self.parse_next_component();
            if comp.is_some() {
                return;
            } else {
                self.path = &self.path[size..];
            }
        }
    }

    // trim away repeated separators (i.e., empty components) on the right
    fn trim_right(&mut self) {
        while self.path.len() > self.len_before_body() {
            let (size, comp) = self.parse_next_component_back();
            if comp.is_some() {
                return;
            } else {
                self.path = &self.path[..self.path.len() - size];
            }
        }
    }
}

impl AsRef<Path> for Components<'_> {
    fn as_ref(&self) -> &Path {
        self.as_path()
    }
}

impl AsRef<UnixStr> for Components<'_> {
    fn as_ref(&self) -> &UnixStr {
        self.as_path().as_unix_str()
    }
}

impl fmt::Debug for Iter<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        struct DebugHelper<'a>(&'a Path);

        impl fmt::Debug for DebugHelper<'_> {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.debug_list().entries(self.0.iter()).finish()
            }
        }

        f.debug_tuple("Iter")
            .field(&DebugHelper(self.as_path()))
            .finish()
    }
}

impl<'a> Iter<'a> {
    /// Extracts a slice corresponding to the portion of the path remaining for iteration.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// let mut iter = Path::new("/tmp/foo/bar.txt").iter();
    /// iter.next();
    /// iter.next();
    ///
    /// assert_eq!(Path::new("foo/bar.txt"), iter.as_path());
    /// ```
    pub fn as_path(&self) -> &'a Path {
        self.inner.as_path()
    }
}

impl AsRef<Path> for Iter<'_> {
    fn as_ref(&self) -> &Path {
        self.as_path()
    }
}

impl AsRef<UnixStr> for Iter<'_> {
    fn as_ref(&self) -> &UnixStr {
        self.as_path().as_unix_str()
    }
}

impl<'a> Iterator for Iter<'a> {
    type Item = &'a UnixStr;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.next().map(Component::as_unix_str)
    }
}

impl<'a> DoubleEndedIterator for Iter<'a> {
    fn next_back(&mut self) -> Option<Self::Item> {
        self.inner.next_back().map(Component::as_unix_str)
    }
}

impl FusedIterator for Iter<'_> {}

impl<'a> Iterator for Components<'a> {
    type Item = Component<'a>;

    fn next(&mut self) -> Option<Component<'a>> {
        while !self.finished() {
            match self.front {
                State::Prefix => {
                    self.front = State::StartDir;
                }
                State::StartDir => {
                    self.front = State::Body;
                    if self.has_physical_root {
                        debug_assert!(!self.path.is_empty());
                        self.path = &self.path[1..];
                        return Some(Component::RootDir);
                    } else if self.include_cur_dir() {
                        debug_assert!(!self.path.is_empty());
                        self.path = &self.path[1..];
                        return Some(Component::CurDir);
                    }
                }
                State::Body if !self.path.is_empty() => {
                    let (size, comp) = self.parse_next_component();
                    self.path = &self.path[size..];
                    if comp.is_some() {
                        return comp;
                    }
                }
                State::Body => {
                    self.front = State::Done;
                }
                State::Done => unreachable!(),
            }
        }
        None
    }
}

impl<'a> DoubleEndedIterator for Components<'a> {
    fn next_back(&mut self) -> Option<Component<'a>> {
        while !self.finished() {
            match self.back {
                State::Body if self.path.len() > self.len_before_body() => {
                    let (size, comp) = self.parse_next_component_back();
                    self.path = &self.path[..self.path.len() - size];
                    if comp.is_some() {
                        return comp;
                    }
                }
                State::Body => {
                    self.back = State::StartDir;
                }
                State::StartDir => {
                    self.back = State::Prefix;
                    if self.has_physical_root {
                        self.path = &self.path[..self.path.len() - 1];
                        return Some(Component::RootDir);
                    } else if self.include_cur_dir() {
                        self.path = &self.path[..self.path.len() - 1];
                        return Some(Component::CurDir);
                    }
                }
                State::Prefix => {
                    self.back = State::Done;
                    return None;
                }
                State::Done => unreachable!(),
            }
        }
        None
    }
}

impl FusedIterator for Components<'_> {}

impl<'a> cmp::PartialEq for Components<'a> {
    fn eq(&self, other: &Components<'a>) -> bool {
        Iterator::eq(self.clone(), other.clone())
    }
}

impl cmp::Eq for Components<'_> {}

impl<'a> cmp::PartialOrd for Components<'a> {
    fn partial_cmp(&self, other: &Components<'a>) -> Option<cmp::Ordering> {
        Iterator::partial_cmp(self.clone(), other.clone())
    }
}

impl cmp::Ord for Components<'_> {
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        Iterator::cmp(self.clone(), other.clone())
    }
}

/// An iterator over [`Path`] and its ancestors.
///
/// This `struct` is created by the [`ancestors`] method on [`Path`].
/// See its documentation for more.
///
/// # Examples
///
/// ```
/// use unix_path::Path;
///
/// let path = Path::new("/foo/bar");
///
/// for ancestor in path.ancestors() {
///     println!("{:?}", ancestor);
/// }
/// ```
///
/// [`ancestors`]: struct.Path.html#method.ancestors
/// [`Path`]: struct.Path.html
#[derive(Copy, Clone, Debug)]
pub struct Ancestors<'a> {
    next: Option<&'a Path>,
}

impl<'a> Iterator for Ancestors<'a> {
    type Item = &'a Path;

    fn next(&mut self) -> Option<Self::Item> {
        let next = self.next;
        self.next = next.and_then(Path::parent);
        next
    }
}

impl FusedIterator for Ancestors<'_> {}

////////////////////////////////////////////////////////////////////////////////
// Basic types and traits
////////////////////////////////////////////////////////////////////////////////

/// An owned, mutable path (akin to `String`).
///
/// This type provides methods like [`push`] and [`set_extension`] that mutate
/// the path in place. It also implements `Deref` to [`Path`], meaning that
/// all methods on [`Path`] slices are available on `PathBuf` values as well.
///
/// [`Path`]: struct.Path.html
/// [`push`]: struct.PathBuf.html#method.push
/// [`set_extension`]: struct.PathBuf.html#method.set_extension
///
/// More details about the overall approach can be found in
/// the [crate documentation](index.html).
///
/// # Examples
///
/// You can use [`push`] to build up a `PathBuf` from
/// components:
///
/// ```
/// use unix_path::PathBuf;
///
/// let mut path = PathBuf::new();
///
/// path.push("/");
/// path.push("feel");
/// path.push("the");
///
/// path.set_extension("force");
/// ```
///
/// However, [`push`] is best used for dynamic situations. This is a better way
/// to do this when you know all of the components ahead of time:
///
/// ```
/// use unix_path::PathBuf;
///
/// let path: PathBuf = ["/", "feel", "the.force"].iter().collect();
/// ```
///
/// We can still do better than this! Since these are all strings, we can use
/// `From::from`:
///
/// ```
/// use unix_path::PathBuf;
///
/// let path = PathBuf::from(r"/feel/the.force");
/// ```
///
/// Which method works best depends on what kind of situation you're in.
#[derive(Clone)]
#[cfg(feature = "alloc")]
pub struct PathBuf {
    inner: UnixString,
}

#[cfg(feature = "alloc")]
impl PathBuf {
    fn as_mut_vec(&mut self) -> &mut Vec<u8> {
        unsafe { &mut *(self as *mut PathBuf as *mut Vec<u8>) }
    }

    /// Allocates an empty `PathBuf`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::PathBuf;
    ///
    /// let path = PathBuf::new();
    /// ```
    pub fn new() -> PathBuf {
        PathBuf {
            inner: UnixString::new(),
        }
    }

    /// Creates a new `PathBuf` with a given capacity used to create the
    /// internal `UnixString`. See `with_capacity` defined on `UnixString`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::PathBuf;
    ///
    /// let mut path = PathBuf::with_capacity(10);
    /// let capacity = path.capacity();
    ///
    /// // This push is done without reallocating
    /// path.push("/");
    ///
    /// assert_eq!(capacity, path.capacity());
    /// ```
    pub fn with_capacity(capacity: usize) -> PathBuf {
        PathBuf {
            inner: UnixString::with_capacity(capacity),
        }
    }

    /// Coerces to a [`Path`] slice.
    ///
    /// [`Path`]: struct.Path.html
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::{Path, PathBuf};
    ///
    /// let p = PathBuf::from("/test");
    /// assert_eq!(Path::new("/test"), p.as_path());
    /// ```
    pub fn as_path(&self) -> &Path {
        self
    }

    /// Extends `self` with `path`.
    ///
    /// If `path` is absolute, it replaces the current path.
    ///
    /// # Examples
    ///
    /// Pushing a relative path extends the existing path:
    ///
    /// ```
    /// use unix_path::PathBuf;
    ///
    /// let mut path = PathBuf::from("/tmp");
    /// path.push("file.bk");
    /// assert_eq!(path, PathBuf::from("/tmp/file.bk"));
    /// ```
    ///
    /// Pushing an absolute path replaces the existing path:
    ///
    /// ```
    /// use unix_path::PathBuf;
    ///
    /// let mut path = PathBuf::from("/tmp");
    /// path.push("/etc");
    /// assert_eq!(path, PathBuf::from("/etc"));
    /// ```
    pub fn push<P: AsRef<Path>>(&mut self, path: P) {
        self._push(path.as_ref())
    }

    fn _push(&mut self, path: &Path) {
        // in general, a separator is needed if the rightmost byte is not a separator
        let need_sep = self
            .as_mut_vec()
            .last()
            .map(|c| *c != b'/')
            .unwrap_or(false);

        // absolute `path` replaces `self`
        if path.is_absolute() || path.has_root() {
            self.as_mut_vec().truncate(0);
        } else if need_sep {
            self.inner.push("/");
        }

        self.inner.push(path.as_unix_str());
    }

    /// Truncates `self` to [`self.parent`].
    ///
    /// Returns `false` and does nothing if [`self.parent`] is `None`.
    /// Otherwise, returns `true`.
    ///
    /// [`self.parent`]: struct.PathBuf.html#method.parent
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::{Path, PathBuf};
    ///
    /// let mut p = PathBuf::from("/test/test.rs");
    ///
    /// p.pop();
    /// assert_eq!(Path::new("/test"), p);
    /// p.pop();
    /// assert_eq!(Path::new("/"), p);
    /// ```
    pub fn pop(&mut self) -> bool {
        match self.parent().map(|p| p.as_unix_str().len()) {
            Some(len) => {
                self.as_mut_vec().truncate(len);
                true
            }
            None => false,
        }
    }

    /// Updates [`self.file_name`] to `file_name`.
    ///
    /// If [`self.file_name`] was `None`, this is equivalent to pushing
    /// `file_name`.
    ///
    /// Otherwise it is equivalent to calling [`pop`] and then pushing
    /// `file_name`. The new path will be a sibling of the original path.
    /// (That is, it will have the same parent.)
    ///
    /// [`self.file_name`]: struct.PathBuf.html#method.file_name
    /// [`pop`]: struct.PathBuf.html#method.pop
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::PathBuf;
    ///
    /// let mut buf = PathBuf::from("/");
    /// assert!(buf.file_name() == None);
    /// buf.set_file_name("bar");
    /// assert!(buf == PathBuf::from("/bar"));
    /// assert!(buf.file_name().is_some());
    /// buf.set_file_name("baz.txt");
    /// assert!(buf == PathBuf::from("/baz.txt"));
    /// ```
    pub fn set_file_name<S: AsRef<UnixStr>>(&mut self, file_name: S) {
        self._set_file_name(file_name.as_ref())
    }

    fn _set_file_name(&mut self, file_name: &UnixStr) {
        if self.file_name().is_some() {
            let popped = self.pop();
            debug_assert!(popped);
        }
        self.push(file_name);
    }

    /// Updates [`self.extension`] to `extension`.
    ///
    /// Returns `false` and does nothing if [`self.file_name`] is `None`,
    /// returns `true` and updates the extension otherwise.
    ///
    /// If [`self.extension`] is `None`, the extension is added; otherwise
    /// it is replaced.
    ///
    /// [`self.file_name`]: struct.PathBuf.html#method.file_name
    /// [`self.extension`]: struct.PathBuf.html#method.extension
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::{Path, PathBuf};
    ///
    /// let mut p = PathBuf::from("/feel/the");
    ///
    /// p.set_extension("force");
    /// assert_eq!(Path::new("/feel/the.force"), p.as_path());
    ///
    /// p.set_extension("dark_side");
    /// assert_eq!(Path::new("/feel/the.dark_side"), p.as_path());
    /// ```

    pub fn set_extension<S: AsRef<UnixStr>>(&mut self, extension: S) -> bool {
        self._set_extension(extension.as_ref())
    }

    fn _set_extension(&mut self, extension: &UnixStr) -> bool {
        let file_stem = match self.file_stem() {
            None => return false,
            Some(f) => unix_str_as_u8_slice(f),
        };

        // truncate until right after the file stem
        let end_file_stem = file_stem[file_stem.len()..].as_ptr() as usize;
        let start = unix_str_as_u8_slice(&self.inner).as_ptr() as usize;
        let v = self.as_mut_vec();
        v.truncate(end_file_stem.wrapping_sub(start));

        // add the new extension, if any
        let new = unix_str_as_u8_slice(extension);
        if !new.is_empty() {
            v.reserve_exact(new.len() + 1);
            v.push(b'.');
            v.extend_from_slice(new);
        }

        true
    }

    /// Consumes the `PathBuf`, yielding its internal `UnixString` storage.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::PathBuf;
    ///
    /// let p = PathBuf::from("/the/head");
    /// let bytes = p.into_unix_string();
    /// ```
    pub fn into_unix_string(self) -> UnixString {
        self.inner
    }

    /// Converts this `PathBuf` into a boxed [`Path`].
    ///
    /// [`Path`]: struct.Path.html
    pub fn into_boxed_path(self) -> Box<Path> {
        let rw = Box::into_raw(self.inner.into_boxed_unix_str()) as *mut Path;
        unsafe { Box::from_raw(rw) }
    }

    /// Invokes `capacity` on the underlying instance of `UnixString`.
    pub fn capacity(&self) -> usize {
        self.inner.capacity()
    }

    /// Invokes `clear` on the underlying instance of `UnixString`.
    pub fn clear(&mut self) {
        self.inner.clear()
    }

    /// Invokes `reserve` on the underlying instance of `UnixString`.
    pub fn reserve(&mut self, additional: usize) {
        self.inner.reserve(additional)
    }

    /// Invokes `reserve_exact` on the underlying instance of `UnixString`.
    pub fn reserve_exact(&mut self, additional: usize) {
        self.inner.reserve_exact(additional)
    }

    /// Invokes `shrink_to_fit` on the underlying instance of `UnixString`.
    pub fn shrink_to_fit(&mut self) {
        self.inner.shrink_to_fit()
    }

    /// Invokes `shrink_to` on the underlying instance of `UnixString`.
    #[cfg(feature = "shrink_to")]
    pub fn shrink_to(&mut self, min_capacity: usize) {
        self.inner.shrink_to(min_capacity)
    }
}

#[cfg(feature = "alloc")]
impl From<&Path> for Box<Path> {
    fn from(path: &Path) -> Box<Path> {
        let boxed: Box<UnixStr> = path.inner.into();
        let rw = Box::into_raw(boxed) as *mut Path;
        unsafe { Box::from_raw(rw) }
    }
}

#[cfg(feature = "alloc")]
impl From<Cow<'_, Path>> for Box<Path> {
    #[inline]
    fn from(cow: Cow<'_, Path>) -> Box<Path> {
        match cow {
            Cow::Borrowed(path) => Box::from(path),
            Cow::Owned(path) => Box::from(path),
        }
    }
}

#[cfg(feature = "alloc")]
impl From<Box<Path>> for PathBuf {
    /// Converts a `Box<Path>` into a `PathBuf`
    ///
    /// This conversion does not allocate or copy memory.
    fn from(boxed: Box<Path>) -> PathBuf {
        boxed.into_path_buf()
    }
}

#[cfg(feature = "alloc")]
impl From<PathBuf> for Box<Path> {
    /// Converts a `PathBuf` into a `Box<Path>`
    ///
    /// This conversion currently should not allocate memory,
    /// but this behavior is not guaranteed in all future versions.
    fn from(p: PathBuf) -> Self {
        p.into_boxed_path()
    }
}

#[cfg(feature = "alloc")]
impl Clone for Box<Path> {
    #[inline]
    fn clone(&self) -> Self {
        self.to_path_buf().into_boxed_path()
    }
}

#[cfg(feature = "alloc")]
impl<T: ?Sized + AsRef<UnixStr>> From<&T> for PathBuf {
    fn from(s: &T) -> Self {
        PathBuf::from(s.as_ref().to_unix_string())
    }
}

#[cfg(feature = "alloc")]
impl From<UnixString> for PathBuf {
    /// Converts a `UnixString` into a `PathBuf`
    ///
    /// This conversion does not allocate or copy memory.
    #[inline]
    fn from(s: UnixString) -> Self {
        PathBuf { inner: s }
    }
}

#[cfg(feature = "alloc")]
impl From<PathBuf> for UnixString {
    /// Converts a `PathBuf` into a `UnixString`
    ///
    /// This conversion does not allocate or copy memory.
    fn from(path_buf: PathBuf) -> Self {
        path_buf.inner
    }
}

#[cfg(feature = "alloc")]
impl From<String> for PathBuf {
    /// Converts a `String` into a `PathBuf`
    ///
    /// This conversion does not allocate or copy memory.
    fn from(s: String) -> PathBuf {
        PathBuf::from(UnixString::from(s))
    }
}

#[cfg(feature = "alloc")]
impl FromStr for PathBuf {
    type Err = core::convert::Infallible;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(PathBuf::from(s))
    }
}

#[cfg(feature = "alloc")]
impl<P: AsRef<Path>> iter::FromIterator<P> for PathBuf {
    fn from_iter<I: IntoIterator<Item = P>>(iter: I) -> PathBuf {
        let mut buf = PathBuf::new();
        buf.extend(iter);
        buf
    }
}

#[cfg(feature = "alloc")]
impl<P: AsRef<Path>> iter::Extend<P> for PathBuf {
    fn extend<I: IntoIterator<Item = P>>(&mut self, iter: I) {
        iter.into_iter().for_each(move |p| self.push(p.as_ref()));
    }
}

#[cfg(feature = "alloc")]
impl fmt::Debug for PathBuf {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&**self, formatter)
    }
}

#[cfg(feature = "alloc")]
impl ops::Deref for PathBuf {
    type Target = Path;
    #[inline]
    fn deref(&self) -> &Path {
        Path::new(&self.inner)
    }
}

#[cfg(feature = "alloc")]
impl Borrow<Path> for PathBuf {
    fn borrow(&self) -> &Path {
        self.deref()
    }
}

#[cfg(feature = "alloc")]
impl Default for PathBuf {
    fn default() -> Self {
        PathBuf::new()
    }
}

#[cfg(feature = "alloc")]
impl<'a> From<&'a Path> for Cow<'a, Path> {
    #[inline]
    fn from(s: &'a Path) -> Cow<'a, Path> {
        Cow::Borrowed(s)
    }
}

#[cfg(feature = "alloc")]
impl<'a> From<PathBuf> for Cow<'a, Path> {
    #[inline]
    fn from(s: PathBuf) -> Cow<'a, Path> {
        Cow::Owned(s)
    }
}

#[cfg(feature = "alloc")]
impl<'a> From<&'a PathBuf> for Cow<'a, Path> {
    #[inline]
    fn from(p: &'a PathBuf) -> Cow<'a, Path> {
        Cow::Borrowed(p.as_path())
    }
}

#[cfg(feature = "alloc")]
impl<'a> From<Cow<'a, Path>> for PathBuf {
    #[inline]
    fn from(p: Cow<'a, Path>) -> Self {
        p.into_owned()
    }
}

#[cfg(feature = "alloc")]
impl From<PathBuf> for Arc<Path> {
    /// Converts a `PathBuf` into an `Arc` by moving the `PathBuf` data into a new `Arc` buffer.
    #[inline]
    fn from(s: PathBuf) -> Arc<Path> {
        let arc: Arc<UnixStr> = Arc::from(s.into_unix_string());
        unsafe { Arc::from_raw(Arc::into_raw(arc) as *const Path) }
    }
}

#[cfg(feature = "alloc")]
impl From<&Path> for Arc<Path> {
    /// Converts a `Path` into an `Arc` by copying the `Path` data into a new `Arc` buffer.
    #[inline]
    fn from(s: &Path) -> Arc<Path> {
        let arc: Arc<UnixStr> = Arc::from(s.as_unix_str());
        unsafe { Arc::from_raw(Arc::into_raw(arc) as *const Path) }
    }
}

#[cfg(feature = "alloc")]
impl From<PathBuf> for Rc<Path> {
    /// Converts a `PathBuf` into an `Rc` by moving the `PathBuf` data into a new `Rc` buffer.
    #[inline]
    fn from(s: PathBuf) -> Rc<Path> {
        let rc: Rc<UnixStr> = Rc::from(s.into_unix_string());
        unsafe { Rc::from_raw(Rc::into_raw(rc) as *const Path) }
    }
}

#[cfg(feature = "alloc")]
impl From<&Path> for Rc<Path> {
    /// Converts a `Path` into an `Rc` by copying the `Path` data into a new `Rc` buffer.
    #[inline]
    fn from(s: &Path) -> Rc<Path> {
        let rc: Rc<UnixStr> = Rc::from(s.as_unix_str());
        unsafe { Rc::from_raw(Rc::into_raw(rc) as *const Path) }
    }
}

#[cfg(feature = "alloc")]
impl ToOwned for Path {
    type Owned = PathBuf;
    fn to_owned(&self) -> PathBuf {
        self.to_path_buf()
    }
}

#[cfg(feature = "alloc")]
impl cmp::PartialEq for PathBuf {
    fn eq(&self, other: &PathBuf) -> bool {
        self.components() == other.components()
    }
}

#[cfg(feature = "alloc")]
impl Hash for PathBuf {
    fn hash<H: Hasher>(&self, h: &mut H) {
        self.as_path().hash(h)
    }
}

#[cfg(feature = "alloc")]
impl cmp::Eq for PathBuf {}

#[cfg(feature = "alloc")]
impl cmp::PartialOrd for PathBuf {
    fn partial_cmp(&self, other: &PathBuf) -> Option<cmp::Ordering> {
        self.components().partial_cmp(other.components())
    }
}

#[cfg(feature = "alloc")]
impl cmp::Ord for PathBuf {
    fn cmp(&self, other: &PathBuf) -> cmp::Ordering {
        self.components().cmp(other.components())
    }
}

#[cfg(feature = "alloc")]
impl AsRef<UnixStr> for PathBuf {
    fn as_ref(&self) -> &UnixStr {
        &self.inner[..]
    }
}

/// A slice of a path (akin to `str`).
///
/// This type supports a number of operations for inspecting a path, including
/// breaking the path into its components (separated by `/` ), extracting the
/// file name, determining whether the path is absolute, and so on.
///
/// This is an *unsized* type, meaning that it must always be used behind a
/// pointer like `&` or `Box`. For an owned version of this type,
/// see [`PathBuf`].
///
/// [`PathBuf`]: struct.PathBuf.html
///
/// More details about the overall approach can be found in
/// the [crate documentation](index.html).
///
/// # Examples
///
/// ```
/// use unix_path::Path;
/// use unix_str::UnixStr;
///
/// let path = Path::new("./foo/bar.txt");
///
/// let parent = path.parent();
/// assert_eq!(parent, Some(Path::new("./foo")));
///
/// let file_stem = path.file_stem();
/// assert_eq!(file_stem, Some(UnixStr::new("bar")));
///
/// let extension = path.extension();
/// assert_eq!(extension, Some(UnixStr::new("txt")));
/// ```
pub struct Path {
    inner: UnixStr,
}

/// An error returned from [`Path::strip_prefix`][`strip_prefix`] if the prefix
/// was not found.
///
/// This `struct` is created by the [`strip_prefix`] method on [`Path`].
/// See its documentation for more.
///
/// [`strip_prefix`]: struct.Path.html#method.strip_prefix
/// [`Path`]: struct.Path.html
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct StripPrefixError(());

impl Path {
    // The following (private!) function allows construction of a path from a u8
    // slice, which is only safe when it is known to follow the OsStr encoding.
    unsafe fn from_u8_slice(s: &[u8]) -> &Path {
        Path::new(u8_slice_as_unix_str(s))
    }
    // The following (private!) function reveals the byte encoding used for OsStr.
    fn as_u8_slice(&self) -> &[u8] {
        unix_str_as_u8_slice(&self.inner)
    }

    /// Directly wraps a string slice as a `Path` slice.
    ///
    /// This is a cost-free conversion.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// Path::new("foo.txt");
    /// ```
    ///
    /// You can create `Path`s from `String`s, or even other `Path`s:
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// let string = String::from("foo.txt");
    /// let from_string = Path::new(&string);
    /// let from_path = Path::new(&from_string);
    /// assert_eq!(from_string, from_path);
    /// ```
    pub fn new<S: AsRef<UnixStr> + ?Sized>(s: &S) -> &Path {
        unsafe { &*(s.as_ref() as *const UnixStr as *const Path) }
    }

    /// Yields the underlying bytes.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    /// use unix_str::UnixStr;
    ///
    /// let os_str = Path::new("foo.txt").as_unix_str();
    /// assert_eq!(os_str, UnixStr::new("foo.txt"));
    /// ```
    pub fn as_unix_str(&self) -> &UnixStr {
        &self.inner
    }

    /// Yields a `&str` slice if the `Path` is valid unicode.
    ///
    /// This conversion may entail doing a check for UTF-8 validity.
    /// Note that validation is performed because non-UTF-8 strings are
    /// perfectly valid for some OS.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// let path = Path::new("foo.txt");
    /// assert_eq!(path.to_str(), Some("foo.txt"));
    /// ```
    pub fn to_str(&self) -> Option<&str> {
        self.inner.to_str()
    }

    /// Converts a `Path` to a `Cow<str>`.
    ///
    /// Any non-Unicode sequences are replaced with
    /// `U+FFFD REPLACEMENT CHARACTER`.
    ///
    ///
    /// # Examples
    ///
    /// Calling `to_string_lossy` on a `Path` with valid unicode:
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// let path = Path::new("foo.txt");
    /// assert_eq!(path.to_string_lossy(), "foo.txt");
    /// ```
    ///
    /// Had `path` contained invalid unicode, the `to_string_lossy` call might
    /// have returned `"fo�.txt"`.
    #[cfg(feature = "alloc")]
    pub fn to_string_lossy(&self) -> Cow<'_, str> {
        self.inner.to_string_lossy()
    }

    /// Converts a `Path` to an owned [`PathBuf`].
    ///
    /// [`PathBuf`]: struct.PathBuf.html
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// let path_buf = Path::new("foo.txt").to_path_buf();
    /// assert_eq!(path_buf, unix_path::PathBuf::from("foo.txt"));
    /// ```
    #[cfg(feature = "alloc")]
    pub fn to_path_buf(&self) -> PathBuf {
        PathBuf::from(&self.inner)
    }

    /// Returns `true` if the `Path` is absolute, i.e., if it is independent of
    /// the current directory.
    ///
    /// A path is absolute if it starts with the root, so `is_absolute` and
    /// [`has_root`] are equivalent.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// assert!(!Path::new("foo.txt").is_absolute());
    /// ```
    ///
    /// [`has_root`]: #method.has_root
    pub fn is_absolute(&self) -> bool {
        self.has_root()
    }

    /// Returns `true` if the `Path` is relative, i.e., not absolute.
    ///
    /// See [`is_absolute`]'s documentation for more details.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// assert!(Path::new("foo.txt").is_relative());
    /// ```
    ///
    /// [`is_absolute`]: #method.is_absolute
    pub fn is_relative(&self) -> bool {
        !self.is_absolute()
    }

    /// Returns `true` if the `Path` has a root.
    ///
    /// A path has a root if it begins with `/`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// assert!(Path::new("/etc/passwd").has_root());
    /// ```
    pub fn has_root(&self) -> bool {
        self.components().has_root()
    }

    /// Returns the `Path` without its final component, if there is one.
    ///
    /// Returns `None` if the path terminates in a root or prefix.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// let path = Path::new("/foo/bar");
    /// let parent = path.parent().unwrap();
    /// assert_eq!(parent, Path::new("/foo"));
    ///
    /// let grand_parent = parent.parent().unwrap();
    /// assert_eq!(grand_parent, Path::new("/"));
    /// assert_eq!(grand_parent.parent(), None);
    /// ```
    pub fn parent(&self) -> Option<&Path> {
        let mut comps = self.components();
        let comp = comps.next_back();
        comp.and_then(|p| match p {
            Component::Normal(_) | Component::CurDir | Component::ParentDir => {
                Some(comps.as_path())
            }
            _ => None,
        })
    }

    /// Produces an iterator over `Path` and its ancestors.
    ///
    /// The iterator will yield the `Path` that is returned if the [`parent`] method is used zero
    /// or more times. That means, the iterator will yield `&self`, `&self.parent().unwrap()`,
    /// `&self.parent().unwrap().parent().unwrap()` and so on. If the [`parent`] method returns
    /// `None`, the iterator will do likewise. The iterator will always yield at least one value,
    /// namely `&self`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// let mut ancestors = Path::new("/foo/bar").ancestors();
    /// assert_eq!(ancestors.next(), Some(Path::new("/foo/bar")));
    /// assert_eq!(ancestors.next(), Some(Path::new("/foo")));
    /// assert_eq!(ancestors.next(), Some(Path::new("/")));
    /// assert_eq!(ancestors.next(), None);
    /// ```
    ///
    /// [`parent`]: struct.Path.html#method.parent
    pub fn ancestors(&self) -> Ancestors<'_> {
        Ancestors { next: Some(&self) }
    }

    /// Returns the final component of the `Path`, if there is one.
    ///
    /// If the path is a normal file, this is the file name. If it's the path of a directory, this
    /// is the directory name.
    ///
    /// Returns `None` if the path terminates in `..`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    /// use unix_str::UnixStr;
    ///
    /// assert_eq!(Some(UnixStr::new("bin")), Path::new("/usr/bin/").file_name());
    /// assert_eq!(Some(UnixStr::new("foo.txt")), Path::new("tmp/foo.txt").file_name());
    /// assert_eq!(Some(UnixStr::new("foo.txt")), Path::new("foo.txt/.").file_name());
    /// assert_eq!(Some(UnixStr::new("foo.txt")), Path::new("foo.txt/.//").file_name());
    /// assert_eq!(None, Path::new("foo.txt/..").file_name());
    /// assert_eq!(None, Path::new("/").file_name());
    /// ```
    pub fn file_name(&self) -> Option<&UnixStr> {
        self.components().next_back().and_then(|p| match p {
            Component::Normal(p) => Some(p),
            _ => None,
        })
    }

    /// Returns a path that, when joined onto `base`, yields `self`.
    ///
    /// # Errors
    ///
    /// If `base` is not a prefix of `self` (i.e., [`starts_with`]
    /// returns `false`), returns `Err`.
    ///
    /// [`starts_with`]: #method.starts_with
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::{Path, PathBuf};
    ///
    /// let path = Path::new("/test/haha/foo.txt");
    ///
    /// assert_eq!(path.strip_prefix("/"), Ok(Path::new("test/haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test"), Ok(Path::new("haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test/"), Ok(Path::new("haha/foo.txt")));
    /// assert_eq!(path.strip_prefix("/test/haha/foo.txt"), Ok(Path::new("")));
    /// assert_eq!(path.strip_prefix("/test/haha/foo.txt/"), Ok(Path::new("")));
    /// assert_eq!(path.strip_prefix("test").is_ok(), false);
    /// assert_eq!(path.strip_prefix("/haha").is_ok(), false);
    ///
    /// let prefix = PathBuf::from("/test/");
    /// assert_eq!(path.strip_prefix(prefix), Ok(Path::new("haha/foo.txt")));
    /// ```
    pub fn strip_prefix<P>(&self, base: P) -> Result<&Path, StripPrefixError>
    where
        P: AsRef<Path>,
    {
        self._strip_prefix(base.as_ref())
    }

    fn _strip_prefix(&self, base: &Path) -> Result<&Path, StripPrefixError> {
        iter_after(self.components(), base.components())
            .map(|c| c.as_path())
            .ok_or(StripPrefixError(()))
    }

    /// Determines whether `base` is a prefix of `self`.
    ///
    /// Only considers whole path components to match.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// let path = Path::new("/etc/passwd");
    ///
    /// assert!(path.starts_with("/etc"));
    /// assert!(path.starts_with("/etc/"));
    /// assert!(path.starts_with("/etc/passwd"));
    /// assert!(path.starts_with("/etc/passwd/"));
    ///
    /// assert!(!path.starts_with("/e"));
    /// ```
    pub fn starts_with<P: AsRef<Path>>(&self, base: P) -> bool {
        self._starts_with(base.as_ref())
    }

    fn _starts_with(&self, base: &Path) -> bool {
        iter_after(self.components(), base.components()).is_some()
    }

    /// Determines whether `child` is a suffix of `self`.
    ///
    /// Only considers whole path components to match.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// let path = Path::new("/etc/passwd");
    ///
    /// assert!(path.ends_with("passwd"));
    /// ```
    pub fn ends_with<P: AsRef<Path>>(&self, child: P) -> bool {
        self._ends_with(child.as_ref())
    }

    fn _ends_with(&self, child: &Path) -> bool {
        iter_after(self.components().rev(), child.components().rev()).is_some()
    }

    /// Extracts the stem (non-extension) portion of [`self.file_name`].
    ///
    /// [`self.file_name`]: struct.Path.html#method.file_name
    ///
    /// The stem is:
    ///
    /// * `None`, if there is no file name;
    /// * The entire file name if there is no embedded `.`;
    /// * The entire file name if the file name begins with `.` and has no other `.`s within;
    /// * Otherwise, the portion of the file name before the final `.`
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    ///
    /// let path = Path::new("foo.rs");
    ///
    /// assert_eq!("foo", path.file_stem().unwrap());
    /// ```
    pub fn file_stem(&self) -> Option<&UnixStr> {
        self.file_name()
            .map(split_file_at_dot)
            .and_then(|(before, after)| before.or(after))
    }

    /// Extracts the extension of [`self.file_name`], if possible.
    ///
    /// The extension is:
    ///
    /// * `None`, if there is no file name;
    /// * `None`, if there is no embedded `.`;
    /// * `None`, if the file name begins with `.` and has no other `.`s within;
    /// * Otherwise, the portion of the file name after the final `.`
    ///
    /// [`self.file_name`]: struct.Path.html#method.file_name
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::Path;
    /// use unix_str::UnixStr;
    ///
    /// let path = Path::new("foo.rs");
    ///
    /// assert_eq!(UnixStr::new("rs"), path.extension().unwrap());
    /// ```
    pub fn extension(&self) -> Option<&UnixStr> {
        self.file_name()
            .map(split_file_at_dot)
            .and_then(|(before, after)| before.and(after))
    }

    /// Creates an owned [`PathBuf`] with `path` adjoined to `self`.
    ///
    /// See [`PathBuf::push`] for more details on what it means to adjoin a path.
    ///
    /// [`PathBuf`]: struct.PathBuf.html
    /// [`PathBuf::push`]: struct.PathBuf.html#method.push
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::{Path, PathBuf};
    ///
    /// assert_eq!(Path::new("/etc").join("passwd"), PathBuf::from("/etc/passwd"));
    /// ```
    #[must_use]
    #[cfg(feature = "alloc")]
    pub fn join<P: AsRef<Path>>(&self, path: P) -> PathBuf {
        self._join(path.as_ref())
    }

    #[cfg(feature = "alloc")]
    fn _join(&self, path: &Path) -> PathBuf {
        let mut buf = self.to_path_buf();
        buf.push(path);
        buf
    }

    /// Creates an owned [`PathBuf`] like `self` but with the given file name.
    ///
    /// See [`PathBuf::set_file_name`] for more details.
    ///
    /// [`PathBuf`]: struct.PathBuf.html
    /// [`PathBuf::set_file_name`]: struct.PathBuf.html#method.set_file_name
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::{Path, PathBuf};
    ///
    /// let path = Path::new("/tmp/foo.txt");
    /// assert_eq!(path.with_file_name("bar.txt"), PathBuf::from("/tmp/bar.txt"));
    ///
    /// let path = Path::new("/tmp");
    /// assert_eq!(path.with_file_name("var"), PathBuf::from("/var"));
    /// ```
    #[cfg(feature = "alloc")]
    pub fn with_file_name<S: AsRef<UnixStr>>(&self, file_name: S) -> PathBuf {
        self._with_file_name(file_name.as_ref())
    }

    #[cfg(feature = "alloc")]
    fn _with_file_name(&self, file_name: &UnixStr) -> PathBuf {
        let mut buf = self.to_path_buf();
        buf.set_file_name(file_name);
        buf
    }

    /// Creates an owned [`PathBuf`] like `self` but with the given extension.
    ///
    /// See [`PathBuf::set_extension`] for more details.
    ///
    /// [`PathBuf`]: struct.PathBuf.html
    /// [`PathBuf::set_extension`]: struct.PathBuf.html#method.set_extension
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::{Path, PathBuf};
    ///
    /// let path = Path::new("foo.rs");
    /// assert_eq!(path.with_extension("txt"), PathBuf::from("foo.txt"));
    /// ```
    #[cfg(feature = "alloc")]
    pub fn with_extension<S: AsRef<UnixStr>>(&self, extension: S) -> PathBuf {
        self._with_extension(extension.as_ref())
    }

    #[cfg(feature = "alloc")]
    fn _with_extension(&self, extension: &UnixStr) -> PathBuf {
        let mut buf = self.to_path_buf();
        buf.set_extension(extension);
        buf
    }

    /// Produces an iterator over the [`Component`]s of the path.
    ///
    /// When parsing the path, there is a small amount of normalization:
    ///
    /// * Repeated separators are ignored, so `a/b` and `a//b` both have
    ///   `a` and `b` as components.
    ///
    /// * Occurrences of `.` are normalized away, except if they are at the
    ///   beginning of the path. For example, `a/./b`, `a/b/`, `a/b/.` and
    ///   `a/b` all have `a` and `b` as components, but `./a/b` starts with
    ///   an additional [`CurDir`] component.
    ///
    /// * A trailing slash is normalized away, `/a/b` and `/a/b/` are equivalent.
    ///
    /// Note that no other normalization takes place; in particular, `a/c`
    /// and `a/b/../c` are distinct, to account for the possibility that `b`
    /// is a symbolic link (so its parent isn't `a`).
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::{Path, Component};
    /// use unix_str::UnixStr;
    ///
    /// let mut components = Path::new("/tmp/foo.txt").components();
    ///
    /// assert_eq!(components.next(), Some(Component::RootDir));
    /// assert_eq!(components.next(), Some(Component::Normal(UnixStr::new("tmp"))));
    /// assert_eq!(components.next(), Some(Component::Normal(UnixStr::new("foo.txt"))));
    /// assert_eq!(components.next(), None)
    /// ```
    ///
    /// [`Component`]: enum.Component.html
    /// [`CurDir`]: enum.Component.html#variant.CurDir
    pub fn components(&self) -> Components<'_> {
        Components {
            path: self.as_u8_slice(),
            has_physical_root: has_physical_root(self.as_u8_slice()),
            front: State::Prefix,
            back: State::Body,
        }
    }

    /// Produces an iterator over the path's components viewed as `UnixStr`
    /// slices.
    ///
    /// For more information about the particulars of how the path is separated
    /// into components, see [`components`].
    ///
    /// [`components`]: #method.components
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_path::{self, Path};
    /// use unix_str::UnixStr;
    ///
    /// let mut it = Path::new("/tmp/foo.txt").iter();
    /// assert_eq!(it.next(), Some(UnixStr::new("/")));
    /// assert_eq!(it.next(), Some(UnixStr::new("tmp")));
    /// assert_eq!(it.next(), Some(UnixStr::new("foo.txt")));
    /// assert_eq!(it.next(), None)
    /// ```
    pub fn iter(&self) -> Iter<'_> {
        Iter {
            inner: self.components(),
        }
    }

    /// Converts a `Box<Path>` into a [`PathBuf`] without copying or
    /// allocating.
    ///
    /// [`PathBuf`]: struct.PathBuf.html
    #[cfg(feature = "alloc")]
    pub fn into_path_buf(self: Box<Path>) -> PathBuf {
        let rw = Box::into_raw(self) as *mut UnixStr;
        let inner = unsafe { Box::from_raw(rw) };
        PathBuf {
            inner: UnixString::from(inner),
        }
    }

    /// Returns a newtype that implements Display for safely printing paths
    /// that may contain non-Unicode data.
    pub fn display(&self) -> Display<'_> {
        Display { path: self }
    }
}

impl AsRef<UnixStr> for Path {
    fn as_ref(&self) -> &UnixStr {
        &self.inner
    }
}

impl fmt::Debug for Path {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&self.inner, formatter)
    }
}

impl cmp::PartialEq for Path {
    fn eq(&self, other: &Path) -> bool {
        self.components().eq(other.components())
    }
}

impl Hash for Path {
    fn hash<H: Hasher>(&self, h: &mut H) {
        for component in self.components() {
            component.hash(h);
        }
    }
}

impl cmp::Eq for Path {}

impl cmp::PartialOrd for Path {
    fn partial_cmp(&self, other: &Path) -> Option<cmp::Ordering> {
        self.components().partial_cmp(other.components())
    }
}

impl cmp::Ord for Path {
    fn cmp(&self, other: &Path) -> cmp::Ordering {
        self.components().cmp(other.components())
    }
}

impl AsRef<Path> for Path {
    fn as_ref(&self) -> &Path {
        self
    }
}

impl AsRef<Path> for UnixStr {
    fn as_ref(&self) -> &Path {
        Path::new(self)
    }
}

#[cfg(feature = "alloc")]
impl AsRef<Path> for Cow<'_, UnixStr> {
    fn as_ref(&self) -> &Path {
        Path::new(self)
    }
}

#[cfg(feature = "alloc")]
impl AsRef<Path> for UnixString {
    fn as_ref(&self) -> &Path {
        Path::new(self)
    }
}

impl AsRef<Path> for str {
    #[inline]
    fn as_ref(&self) -> &Path {
        Path::new(self)
    }
}

#[cfg(feature = "alloc")]
impl AsRef<Path> for String {
    fn as_ref(&self) -> &Path {
        Path::new(self)
    }
}

#[cfg(feature = "alloc")]
impl AsRef<Path> for PathBuf {
    #[inline]
    fn as_ref(&self) -> &Path {
        self
    }
}

#[cfg(feature = "alloc")]
impl<'a> IntoIterator for &'a PathBuf {
    type Item = &'a UnixStr;
    type IntoIter = Iter<'a>;
    fn into_iter(self) -> Iter<'a> {
        self.iter()
    }
}

impl<'a> IntoIterator for &'a Path {
    type Item = &'a UnixStr;
    type IntoIter = Iter<'a>;
    fn into_iter(self) -> Iter<'a> {
        self.iter()
    }
}

#[cfg(feature = "serde")]
use serde::{
    de::{self, Deserialize, Deserializer, Unexpected, Visitor},
    ser::{self, Serialize, Serializer},
};

#[cfg(feature = "serde")]
impl Serialize for Path {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self.to_str() {
            Some(s) => s.serialize(serializer),
            None => Err(ser::Error::custom("path contains invalid UTF-8 characters")),
        }
    }
}

#[cfg(feature = "serde")]
impl Serialize for PathBuf {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        self.as_path().serialize(serializer)
    }
}

#[cfg(feature = "serde")]
struct PathVisitor;

#[cfg(feature = "serde")]
impl<'a> Visitor<'a> for PathVisitor {
    type Value = &'a Path;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a borrowed path")
    }

    fn visit_borrowed_str<E>(self, v: &'a str) -> Result<Self::Value, E>
    where
        E: de::Error,
    {
        Ok(v.as_ref())
    }

    fn visit_borrowed_bytes<E>(self, v: &'a [u8]) -> Result<Self::Value, E>
    where
        E: de::Error,
    {
        core::str::from_utf8(v)
            .map(AsRef::as_ref)
            .map_err(|_| de::Error::invalid_value(Unexpected::Bytes(v), &self))
    }
}

#[cfg(feature = "serde")]
impl<'de: 'a, 'a> Deserialize<'de> for &'a Path {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_str(PathVisitor)
    }
}

#[cfg(feature = "serde")]
struct PathBufVisitor;

#[cfg(feature = "serde")]
impl<'de> Visitor<'de> for PathBufVisitor {
    type Value = PathBuf;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("path string")
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: de::Error,
    {
        Ok(From::from(v))
    }

    fn visit_string<E>(self, v: String) -> Result<Self::Value, E>
    where
        E: de::Error,
    {
        Ok(From::from(v))
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: de::Error,
    {
        core::str::from_utf8(v)
            .map(From::from)
            .map_err(|_| de::Error::invalid_value(Unexpected::Bytes(v), &self))
    }

    fn visit_byte_buf<E>(self, v: Vec<u8>) -> Result<Self::Value, E>
    where
        E: de::Error,
    {
        String::from_utf8(v)
            .map(From::from)
            .map_err(|e| de::Error::invalid_value(Unexpected::Bytes(&e.into_bytes()), &self))
    }
}

#[cfg(feature = "serde")]
impl<'de> Deserialize<'de> for PathBuf {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_string(PathBufVisitor)
    }
}

#[cfg(feature = "serde")]
impl<'de> Deserialize<'de> for Box<Path> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        Deserialize::deserialize(deserializer).map(PathBuf::into_boxed_path)
    }
}

#[cfg(feature = "alloc")]
macro_rules! impl_cmp {
    ($lhs:ty, $rhs: ty) => {
        impl<'a, 'b> PartialEq<$rhs> for $lhs {
            #[inline]
            fn eq(&self, other: &$rhs) -> bool {
                <Path as PartialEq>::eq(self, other)
            }
        }

        impl<'a, 'b> PartialEq<$lhs> for $rhs {
            #[inline]
            fn eq(&self, other: &$lhs) -> bool {
                <Path as PartialEq>::eq(self, other)
            }
        }

        impl<'a, 'b> PartialOrd<$rhs> for $lhs {
            #[inline]
            fn partial_cmp(&self, other: &$rhs) -> Option<cmp::Ordering> {
                <Path as PartialOrd>::partial_cmp(self, other)
            }
        }

        impl<'a, 'b> PartialOrd<$lhs> for $rhs {
            #[inline]
            fn partial_cmp(&self, other: &$lhs) -> Option<cmp::Ordering> {
                <Path as PartialOrd>::partial_cmp(self, other)
            }
        }
    };
}

#[cfg(feature = "alloc")]
impl_cmp!(PathBuf, Path);
#[cfg(feature = "alloc")]
impl_cmp!(PathBuf, &'a Path);
#[cfg(feature = "alloc")]
impl_cmp!(Cow<'a, Path>, Path);
#[cfg(feature = "alloc")]
impl_cmp!(Cow<'a, Path>, &'b Path);
#[cfg(feature = "alloc")]
impl_cmp!(Cow<'a, Path>, PathBuf);

impl fmt::Display for StripPrefixError {
    #[allow(deprecated, deprecated_in_future)]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        "prefix not found".fmt(f)
    }
}

#[cfg(feature = "std")]
impl Error for StripPrefixError {
    #[allow(deprecated)]
    fn description(&self) -> &str {
        "prefix not found"
    }
}

pub struct Display<'a> {
    path: &'a Path,
}

impl fmt::Debug for Display<'_> {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&self.path, formatter)
    }
}

impl fmt::Display for Display<'_> {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(
            &lossy::Utf8Lossy::from_bytes(&self.path.as_unix_str().as_bytes()),
            formatter,
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use alloc::rc::Rc;
    use alloc::sync::Arc;

    macro_rules! t(
        ($path:expr, iter: $iter:expr) => (
            {
                let path = Path::new($path);

                // Forward iteration
                let comps = path.iter()
                    .map(|p| p.to_string_lossy().into_owned())
                    .collect::<Vec<String>>();
                let exp: &[&str] = &$iter;
                let exps = exp.iter().map(|s| s.to_string()).collect::<Vec<String>>();
                assert!(comps == exps, "iter: Expected {:?}, found {:?}",
                        exps, comps);

                // Reverse iteration
                let comps = Path::new($path).iter().rev()
                    .map(|p| p.to_string_lossy().into_owned())
                    .collect::<Vec<String>>();
                let exps = exps.into_iter().rev().collect::<Vec<String>>();
                assert!(comps == exps, "iter().rev(): Expected {:?}, found {:?}",
                        exps, comps);
            }
        );

        ($path:expr, has_root: $has_root:expr, is_absolute: $is_absolute:expr) => (
            {
                let path = Path::new($path);

                let act_root = path.has_root();
                assert!(act_root == $has_root, "has_root: Expected {:?}, found {:?}",
                        $has_root, act_root);

                let act_abs = path.is_absolute();
                assert!(act_abs == $is_absolute, "is_absolute: Expected {:?}, found {:?}",
                        $is_absolute, act_abs);
            }
        );

        ($path:expr, parent: $parent:expr, file_name: $file:expr) => (
            {
                let path = Path::new($path);

                let parent = path.parent().map(|p| p.to_str().unwrap());
                let exp_parent: Option<&str> = $parent;
                assert!(parent == exp_parent, "parent: Expected {:?}, found {:?}",
                        exp_parent, parent);

                let file = path.file_name().map(|p| p.to_str().unwrap());
                let exp_file: Option<&str> = $file;
                assert!(file == exp_file, "file_name: Expected {:?}, found {:?}",
                        exp_file, file);
            }
        );

        ($path:expr, file_stem: $file_stem:expr, extension: $extension:expr) => (
            {
                let path = Path::new($path);

                let stem = path.file_stem().map(|p| p.to_str().unwrap());
                let exp_stem: Option<&str> = $file_stem;
                assert!(stem == exp_stem, "file_stem: Expected {:?}, found {:?}",
                        exp_stem, stem);

                let ext = path.extension().map(|p| p.to_str().unwrap());
                let exp_ext: Option<&str> = $extension;
                assert!(ext == exp_ext, "extension: Expected {:?}, found {:?}",
                        exp_ext, ext);
            }
        );

        ($path:expr, iter: $iter:expr,
                     has_root: $has_root:expr, is_absolute: $is_absolute:expr,
                     parent: $parent:expr, file_name: $file:expr,
                     file_stem: $file_stem:expr, extension: $extension:expr) => (
            {
                t!($path, iter: $iter);
                t!($path, has_root: $has_root, is_absolute: $is_absolute);
                t!($path, parent: $parent, file_name: $file);
                t!($path, file_stem: $file_stem, extension: $extension);
            }
        );
    );

    #[test]
    fn into() {
        use alloc::borrow::Cow;

        let static_path = Path::new("/home/foo");
        let static_cow_path: Cow<'static, Path> = static_path.into();
        let pathbuf = PathBuf::from("/home/foo");

        {
            let path: &Path = &pathbuf;
            let borrowed_cow_path: Cow<'_, Path> = path.into();

            assert_eq!(static_cow_path, borrowed_cow_path);
        }

        let owned_cow_path: Cow<'static, Path> = pathbuf.into();

        assert_eq!(static_cow_path, owned_cow_path);
    }

    #[test]
    pub fn test_decompositions_unix() {
        t!("",
        iter: [],
        has_root: false,
        is_absolute: false,
        parent: None,
        file_name: None,
        file_stem: None,
        extension: None
        );

        t!("foo",
        iter: ["foo"],
        has_root: false,
        is_absolute: false,
        parent: Some(""),
        file_name: Some("foo"),
        file_stem: Some("foo"),
        extension: None
        );

        t!("/",
        iter: ["/"],
        has_root: true,
        is_absolute: true,
        parent: None,
        file_name: None,
        file_stem: None,
        extension: None
        );

        t!("/foo",
        iter: ["/", "foo"],
        has_root: true,
        is_absolute: true,
        parent: Some("/"),
        file_name: Some("foo"),
        file_stem: Some("foo"),
        extension: None
        );

        t!("foo/",
        iter: ["foo"],
        has_root: false,
        is_absolute: false,
        parent: Some(""),
        file_name: Some("foo"),
        file_stem: Some("foo"),
        extension: None
        );

        t!("/foo/",
        iter: ["/", "foo"],
        has_root: true,
        is_absolute: true,
        parent: Some("/"),
        file_name: Some("foo"),
        file_stem: Some("foo"),
        extension: None
        );

        t!("foo/bar",
        iter: ["foo", "bar"],
        has_root: false,
        is_absolute: false,
        parent: Some("foo"),
        file_name: Some("bar"),
        file_stem: Some("bar"),
        extension: None
        );

        t!("/foo/bar",
        iter: ["/", "foo", "bar"],
        has_root: true,
        is_absolute: true,
        parent: Some("/foo"),
        file_name: Some("bar"),
        file_stem: Some("bar"),
        extension: None
        );

        t!("///foo///",
        iter: ["/", "foo"],
        has_root: true,
        is_absolute: true,
        parent: Some("/"),
        file_name: Some("foo"),
        file_stem: Some("foo"),
        extension: None
        );

        t!("///foo///bar",
        iter: ["/", "foo", "bar"],
        has_root: true,
        is_absolute: true,
        parent: Some("///foo"),
        file_name: Some("bar"),
        file_stem: Some("bar"),
        extension: None
        );

        t!("./.",
        iter: ["."],
        has_root: false,
        is_absolute: false,
        parent: Some(""),
        file_name: None,
        file_stem: None,
        extension: None
        );

        t!("/..",
        iter: ["/", ".."],
        has_root: true,
        is_absolute: true,
        parent: Some("/"),
        file_name: None,
        file_stem: None,
        extension: None
        );

        t!("../",
        iter: [".."],
        has_root: false,
        is_absolute: false,
        parent: Some(""),
        file_name: None,
        file_stem: None,
        extension: None
        );

        t!("foo/.",
        iter: ["foo"],
        has_root: false,
        is_absolute: false,
        parent: Some(""),
        file_name: Some("foo"),
        file_stem: Some("foo"),
        extension: None
        );

        t!("foo/..",
        iter: ["foo", ".."],
        has_root: false,
        is_absolute: false,
        parent: Some("foo"),
        file_name: None,
        file_stem: None,
        extension: None
        );

        t!("foo/./",
        iter: ["foo"],
        has_root: false,
        is_absolute: false,
        parent: Some(""),
        file_name: Some("foo"),
        file_stem: Some("foo"),
        extension: None
        );

        t!("foo/./bar",
        iter: ["foo", "bar"],
        has_root: false,
        is_absolute: false,
        parent: Some("foo"),
        file_name: Some("bar"),
        file_stem: Some("bar"),
        extension: None
        );

        t!("foo/../",
        iter: ["foo", ".."],
        has_root: false,
        is_absolute: false,
        parent: Some("foo"),
        file_name: None,
        file_stem: None,
        extension: None
        );

        t!("foo/../bar",
        iter: ["foo", "..", "bar"],
        has_root: false,
        is_absolute: false,
        parent: Some("foo/.."),
        file_name: Some("bar"),
        file_stem: Some("bar"),
        extension: None
        );

        t!("./a",
        iter: [".", "a"],
        has_root: false,
        is_absolute: false,
        parent: Some("."),
        file_name: Some("a"),
        file_stem: Some("a"),
        extension: None
        );

        t!(".",
        iter: ["."],
        has_root: false,
        is_absolute: false,
        parent: Some(""),
        file_name: None,
        file_stem: None,
        extension: None
        );

        t!("./",
        iter: ["."],
        has_root: false,
        is_absolute: false,
        parent: Some(""),
        file_name: None,
        file_stem: None,
        extension: None
        );

        t!("a/b",
        iter: ["a", "b"],
        has_root: false,
        is_absolute: false,
        parent: Some("a"),
        file_name: Some("b"),
        file_stem: Some("b"),
        extension: None
        );

        t!("a//b",
        iter: ["a", "b"],
        has_root: false,
        is_absolute: false,
        parent: Some("a"),
        file_name: Some("b"),
        file_stem: Some("b"),
        extension: None
        );

        t!("a/./b",
        iter: ["a", "b"],
        has_root: false,
        is_absolute: false,
        parent: Some("a"),
        file_name: Some("b"),
        file_stem: Some("b"),
        extension: None
        );

        t!("a/b/c",
        iter: ["a", "b", "c"],
        has_root: false,
        is_absolute: false,
        parent: Some("a/b"),
        file_name: Some("c"),
        file_stem: Some("c"),
        extension: None
        );

        t!(".foo",
        iter: [".foo"],
        has_root: false,
        is_absolute: false,
        parent: Some(""),
        file_name: Some(".foo"),
        file_stem: Some(".foo"),
        extension: None
        );
    }

    #[test]
    pub fn test_stem_ext() {
        t!("foo",
        file_stem: Some("foo"),
        extension: None
        );

        t!("foo.",
        file_stem: Some("foo"),
        extension: Some("")
        );

        t!(".foo",
        file_stem: Some(".foo"),
        extension: None
        );

        t!("foo.txt",
        file_stem: Some("foo"),
        extension: Some("txt")
        );

        t!("foo.bar.txt",
        file_stem: Some("foo.bar"),
        extension: Some("txt")
        );

        t!("foo.bar.",
        file_stem: Some("foo.bar"),
        extension: Some("")
        );

        t!(".", file_stem: None, extension: None);

        t!("..", file_stem: None, extension: None);

        t!("", file_stem: None, extension: None);
    }

    #[test]
    pub fn test_push() {
        macro_rules! tp(
            ($path:expr, $push:expr, $expected:expr) => ( {
                let mut actual = PathBuf::from($path);
                actual.push($push);
                assert!(actual.to_str() == Some($expected),
                        "pushing {:?} onto {:?}: Expected {:?}, got {:?}",
                        $push, $path, $expected, actual.to_str().unwrap());
            });
        );

        tp!("", "foo", "foo");
        tp!("foo", "bar", "foo/bar");
        tp!("foo/", "bar", "foo/bar");
        tp!("foo//", "bar", "foo//bar");
        tp!("foo/.", "bar", "foo/./bar");
        tp!("foo./.", "bar", "foo././bar");
        tp!("foo", "", "foo/");
        tp!("foo", ".", "foo/.");
        tp!("foo", "..", "foo/..");
        tp!("foo", "/", "/");
        tp!("/foo/bar", "/", "/");
        tp!("/foo/bar", "/baz", "/baz");
        tp!("/foo/bar", "./baz", "/foo/bar/./baz");
    }

    #[test]
    pub fn test_pop() {
        macro_rules! tp(
            ($path:expr, $expected:expr, $output:expr) => ( {
                let mut actual = PathBuf::from($path);
                let output = actual.pop();
                assert!(actual.to_str() == Some($expected) && output == $output,
                        "popping from {:?}: Expected {:?}/{:?}, got {:?}/{:?}",
                        $path, $expected, $output,
                        actual.to_str().unwrap(), output);
            });
        );

        tp!("", "", false);
        tp!("/", "/", false);
        tp!("foo", "", true);
        tp!(".", "", true);
        tp!("/foo", "/", true);
        tp!("/foo/bar", "/foo", true);
        tp!("foo/bar", "foo", true);
        tp!("foo/.", "", true);
        tp!("foo//bar", "foo", true);
    }

    #[test]
    pub fn test_set_file_name() {
        macro_rules! tfn(
                ($path:expr, $file:expr, $expected:expr) => ( {
                let mut p = PathBuf::from($path);
                p.set_file_name($file);
                assert!(p.to_str() == Some($expected),
                        "setting file name of {:?} to {:?}: Expected {:?}, got {:?}",
                        $path, $file, $expected,
                        p.to_str().unwrap());
            });
        );

        tfn!("foo", "foo", "foo");
        tfn!("foo", "bar", "bar");
        tfn!("foo", "", "");
        tfn!("", "foo", "foo");
        tfn!(".", "foo", "./foo");
        tfn!("foo/", "bar", "bar");
        tfn!("foo/.", "bar", "bar");
        tfn!("..", "foo", "../foo");
        tfn!("foo/..", "bar", "foo/../bar");
        tfn!("/", "foo", "/foo");
    }

    #[test]
    pub fn test_set_extension() {
        macro_rules! tfe(
                ($path:expr, $ext:expr, $expected:expr, $output:expr) => ( {
                let mut p = PathBuf::from($path);
                let output = p.set_extension($ext);
                assert!(p.to_str() == Some($expected) && output == $output,
                        "setting extension of {:?} to {:?}: Expected {:?}/{:?}, got {:?}/{:?}",
                        $path, $ext, $expected, $output,
                        p.to_str().unwrap(), output);
            });
        );

        tfe!("foo", "txt", "foo.txt", true);
        tfe!("foo.bar", "txt", "foo.txt", true);
        tfe!("foo.bar.baz", "txt", "foo.bar.txt", true);
        tfe!(".test", "txt", ".test.txt", true);
        tfe!("foo.txt", "", "foo", true);
        tfe!("foo", "", "foo", true);
        tfe!("", "foo", "", false);
        tfe!(".", "foo", ".", false);
        tfe!("foo/", "bar", "foo.bar", true);
        tfe!("foo/.", "bar", "foo.bar", true);
        tfe!("..", "foo", "..", false);
        tfe!("foo/..", "bar", "foo/..", false);
        tfe!("/", "foo", "/", false);
    }

    #[test]
    fn test_eq_receivers() {
        use alloc::borrow::Cow;

        let borrowed: &Path = Path::new("foo/bar");
        let mut owned: PathBuf = PathBuf::new();
        owned.push("foo");
        owned.push("bar");
        let borrowed_cow: Cow<'_, Path> = borrowed.into();
        let owned_cow: Cow<'_, Path> = owned.clone().into();

        macro_rules! t {
            ($($current:expr),+) => {
                $(
                    assert_eq!($current, borrowed);
                    assert_eq!($current, owned);
                    assert_eq!($current, borrowed_cow);
                    assert_eq!($current, owned_cow);
                )+
            }
        }

        t!(borrowed, owned, borrowed_cow, owned_cow);
    }

    #[test]
    pub fn test_compare() {
        use std::collections::hash_map::DefaultHasher;
        use std::hash::{Hash, Hasher};

        fn hash<T: Hash>(t: T) -> u64 {
            let mut s = DefaultHasher::new();
            t.hash(&mut s);
            s.finish()
        }

        macro_rules! tc(
            ($path1:expr, $path2:expr, eq: $eq:expr,
             starts_with: $starts_with:expr, ends_with: $ends_with:expr,
             relative_from: $relative_from:expr) => ({
                 let path1 = Path::new($path1);
                 let path2 = Path::new($path2);

                 let eq = path1 == path2;
                 assert!(eq == $eq, "{:?} == {:?}, expected {:?}, got {:?}",
                         $path1, $path2, $eq, eq);
                 assert!($eq == (hash(path1) == hash(path2)),
                         "{:?} == {:?}, expected {:?}, got {} and {}",
                         $path1, $path2, $eq, hash(path1), hash(path2));

                 let starts_with = path1.starts_with(path2);
                 assert!(starts_with == $starts_with,
                         "{:?}.starts_with({:?}), expected {:?}, got {:?}", $path1, $path2,
                         $starts_with, starts_with);

                 let ends_with = path1.ends_with(path2);
                 assert!(ends_with == $ends_with,
                         "{:?}.ends_with({:?}), expected {:?}, got {:?}", $path1, $path2,
                         $ends_with, ends_with);

                 let relative_from = path1.strip_prefix(path2)
                                          .map(|p| p.to_str().unwrap())
                                          .ok();
                 let exp: Option<&str> = $relative_from;
                 assert!(relative_from == exp,
                         "{:?}.strip_prefix({:?}), expected {:?}, got {:?}",
                         $path1, $path2, exp, relative_from);
            });
        );

        tc!("", "",
        eq: true,
        starts_with: true,
        ends_with: true,
        relative_from: Some("")
        );

        tc!("foo", "",
        eq: false,
        starts_with: true,
        ends_with: true,
        relative_from: Some("foo")
        );

        tc!("", "foo",
        eq: false,
        starts_with: false,
        ends_with: false,
        relative_from: None
        );

        tc!("foo", "foo",
        eq: true,
        starts_with: true,
        ends_with: true,
        relative_from: Some("")
        );

        tc!("foo/", "foo",
        eq: true,
        starts_with: true,
        ends_with: true,
        relative_from: Some("")
        );

        tc!("foo/bar", "foo",
        eq: false,
        starts_with: true,
        ends_with: false,
        relative_from: Some("bar")
        );

        tc!("foo/bar/baz", "foo/bar",
        eq: false,
        starts_with: true,
        ends_with: false,
        relative_from: Some("baz")
        );

        tc!("foo/bar", "foo/bar/baz",
        eq: false,
        starts_with: false,
        ends_with: false,
        relative_from: None
        );

        tc!("./foo/bar/", ".",
        eq: false,
        starts_with: true,
        ends_with: false,
        relative_from: Some("foo/bar")
        );
    }

    #[test]
    fn test_components_debug() {
        let path = Path::new("/tmp");

        let mut components = path.components();

        let expected = "Components([RootDir, Normal(\"tmp\")])";
        let actual = format!("{:?}", components);
        assert_eq!(expected, actual);

        let _ = components.next().unwrap();
        let expected = "Components([Normal(\"tmp\")])";
        let actual = format!("{:?}", components);
        assert_eq!(expected, actual);

        let _ = components.next().unwrap();
        let expected = "Components([])";
        let actual = format!("{:?}", components);
        assert_eq!(expected, actual);
    }

    #[test]
    fn test_iter_debug() {
        let path = Path::new("/tmp");

        let mut iter = path.iter();

        let expected = "Iter([\"/\", \"tmp\"])";
        let actual = format!("{:?}", iter);
        assert_eq!(expected, actual);

        let _ = iter.next().unwrap();
        let expected = "Iter([\"tmp\"])";
        let actual = format!("{:?}", iter);
        assert_eq!(expected, actual);

        let _ = iter.next().unwrap();
        let expected = "Iter([])";
        let actual = format!("{:?}", iter);
        assert_eq!(expected, actual);
    }

    #[test]
    fn into_boxed() {
        let orig: &str = "some/sort/of/path";
        let path = Path::new(orig);
        let boxed: Box<Path> = Box::from(path);
        let path_buf = path.to_owned().into_boxed_path().into_path_buf();
        assert_eq!(path, &*boxed);
        assert_eq!(&*boxed, &*path_buf);
        assert_eq!(&*path_buf, path);
    }

    #[test]
    fn into_rc() {
        let orig = "hello/world";
        let path = Path::new(orig);
        let rc: Rc<Path> = Rc::from(path);
        let arc: Arc<Path> = Arc::from(path);

        assert_eq!(&*rc, path);
        assert_eq!(&*arc, path);

        let rc2: Rc<Path> = Rc::from(path.to_owned());
        let arc2: Arc<Path> = Arc::from(path.to_owned());

        assert_eq!(&*rc2, path);
        assert_eq!(&*arc2, path);
    }
}
