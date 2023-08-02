// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::error::{DataError, DataErrorKind};
use crate::helpers;

use alloc::borrow::Cow;
use core::fmt;
use core::fmt::Write;
use core::ops::Deref;
use writeable::{LengthHint, Writeable};
use zerovec::ule::*;

#[doc(hidden)]
#[macro_export]
macro_rules! leading_tag {
    () => {
        "\nicu4x_key_tag"
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! trailing_tag {
    () => {
        "\n"
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! tagged {
    ($without_tags:expr) => {
        concat!(
            $crate::leading_tag!(),
            $without_tags,
            $crate::trailing_tag!()
        )
    };
}

/// A compact hash of a [`DataKey`]. Useful for keys in maps.
///
/// The hash will be stable over time within major releases.
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Copy, Clone, Hash, ULE)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(transparent)]
pub struct DataKeyHash([u8; 4]);

impl DataKeyHash {
    const fn compute_from_path(path: DataKeyPath) -> Self {
        let hash = helpers::fxhash_32(
            path.tagged.as_bytes(),
            leading_tag!().len(),
            trailing_tag!().len(),
        );
        Self(hash.to_le_bytes())
    }

    /// Gets the hash value as a byte array.
    pub const fn to_bytes(self) -> [u8; 4] {
        self.0
    }
}

impl<'a> zerovec::maps::ZeroMapKV<'a> for DataKeyHash {
    type Container = zerovec::ZeroVec<'a, DataKeyHash>;
    type Slice = zerovec::ZeroSlice<DataKeyHash>;
    type GetType = <DataKeyHash as AsULE>::ULE;
    type OwnedType = DataKeyHash;
}

impl AsULE for DataKeyHash {
    type ULE = Self;
    #[inline]
    fn to_unaligned(self) -> Self::ULE {
        self
    }
    #[inline]
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        unaligned
    }
}

// Safe since the ULE type is `self`.
unsafe impl EqULE for DataKeyHash {}

/// Hint for what to prioritize during fallback when data is unavailable.
///
/// For example, if `"en-US"` is requested, but we have no data for that specific locale,
/// fallback may take us to `"en"` or `"und-US"` to check for data.
#[derive(Debug, PartialEq, Eq, Copy, Clone, PartialOrd, Ord)]
#[non_exhaustive]
pub enum FallbackPriority {
    /// Prioritize the language. This is the default behavior.
    ///
    /// For example, `"en-US"` should go to `"en"` and then `"und"`.
    Language,
    /// Prioritize the region.
    ///
    /// For example, `"en-US"` should go to `"und-US"` and then `"und"`.
    Region,
    /// Collation-specific fallback rules. Similar to language priority.
    ///
    /// For example, `"zh-Hant"` goes to `"zh"` before `"und"`.
    Collation,
}

impl FallbackPriority {
    /// Const-friendly version of [`Default::default`].
    pub const fn const_default() -> Self {
        Self::Language
    }
}

impl Default for FallbackPriority {
    fn default() -> Self {
        Self::const_default()
    }
}

/// What additional data to load when performing fallback.
#[derive(Debug, PartialEq, Eq, Copy, Clone, PartialOrd, Ord)]
#[non_exhaustive]
pub enum FallbackSupplement {
    /// Collation supplement; see `CollationFallbackSupplementV1Marker`
    Collation,
}

/// The string path of a data key. For example, "foo@1"
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct DataKeyPath {
    // This string literal is wrapped in leading_tag!() and trailing_tag!() to make it detectable
    // in a compiled binary.
    tagged: &'static str,
}

impl DataKeyPath {
    /// Gets the path as a static string slice.
    #[inline]
    pub const fn get(self) -> &'static str {
        unsafe {
            // Safe due to invariant that self.path is tagged correctly
            core::str::from_utf8_unchecked(core::mem::transmute((
                self.tagged.as_ptr().add(leading_tag!().len()),
                self.tagged.len() - trailing_tag!().len() - leading_tag!().len(),
            )))
        }
    }
}

impl Deref for DataKeyPath {
    type Target = str;
    #[inline]
    fn deref(&self) -> &Self::Target {
        self.get()
    }
}

/// Metadata statically associated with a particular [`DataKey`].
#[derive(Debug, PartialEq, Eq, Copy, Clone, PartialOrd, Ord)]
#[non_exhaustive]
pub struct DataKeyMetadata {
    /// What to prioritize when fallbacking on this [`DataKey`].
    pub fallback_priority: FallbackPriority,
    /// A Unicode extension keyword to consider when loading data for this [`DataKey`].
    pub extension_key: Option<icu_locid::extensions::unicode::Key>,
    /// Optional choice for additional fallbacking data required for loading this marker.
    ///
    /// For more information, see `LocaleFallbackConfig::fallback_supplement`.
    pub fallback_supplement: Option<FallbackSupplement>,
}

impl DataKeyMetadata {
    /// Const-friendly version of [`Default::default`].
    pub const fn const_default() -> Self {
        Self {
            fallback_priority: FallbackPriority::const_default(),
            extension_key: None,
            fallback_supplement: None,
        }
    }

    #[doc(hidden)]
    pub const fn construct_internal(
        fallback_priority: FallbackPriority,
        extension_key: Option<icu_locid::extensions::unicode::Key>,
        fallback_supplement: Option<FallbackSupplement>,
    ) -> Self {
        Self {
            fallback_priority,
            extension_key,
            fallback_supplement,
        }
    }
}

impl Default for DataKeyMetadata {
    #[inline]
    fn default() -> Self {
        Self::const_default()
    }
}

/// Used for loading data from an ICU4X data provider.
///
/// A resource key is tightly coupled with the code that uses it to load data at runtime.
/// Executables can be searched for `DataKey` instances to produce optimized data files.
/// Therefore, users should not generally create DataKey instances; they should instead use
/// the ones exported by a component.
///
/// `DataKey`s are created with the [`data_key!`](crate::data_key) macro:
///
/// ```
/// # use icu_provider::DataKey;
/// const K: DataKey = icu_provider::data_key!("foo/bar@1");
/// ```
///
/// The human-readable path string ends with `@` followed by one or more digits (the version
/// number). Paths do not contain characters other than ASCII letters and digits, `_`, `/`.
///
/// Invalid paths are compile-time errors (as [`data_key!`](crate::data_key) uses `const`).
///
/// ```compile_fail,E0080
/// # use icu_provider::DataKey;
/// const K: DataKey = icu_provider::data_key!("foo/../bar@1");
/// ```
#[derive(Copy, Clone)]
pub struct DataKey {
    path: DataKeyPath,
    hash: DataKeyHash,
    metadata: DataKeyMetadata,
}

impl PartialEq for DataKey {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.hash == other.hash && self.path == other.path && self.metadata == other.metadata
    }
}

impl Eq for DataKey {}

impl Ord for DataKey {
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        self.path
            .cmp(&other.path)
            .then_with(|| self.metadata.cmp(&other.metadata))
    }
}

impl PartialOrd for DataKey {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl core::hash::Hash for DataKey {
    #[inline]
    fn hash<H: core::hash::Hasher>(&self, state: &mut H) {
        self.hash.hash(state)
    }
}

impl DataKey {
    /// Gets a human-readable representation of a [`DataKey`].
    ///
    /// The human-readable path string ends with `@` followed by one or more digits (the version
    /// number). Paths do not contain characters other than ASCII letters and digits, `_`, `/`.
    ///
    /// Useful for reading and writing data to a file system.
    #[inline]
    pub const fn path(self) -> DataKeyPath {
        self.path
    }

    /// Gets a platform-independent hash of a [`DataKey`].
    ///
    /// The hash is 4 bytes and allows for fast key comparison.
    ///
    /// # Example
    ///
    /// ```
    /// use icu_provider::DataKey;
    /// use icu_provider::DataKeyHash;
    ///
    /// const KEY: DataKey = icu_provider::data_key!("foo@1");
    /// const KEY_HASH: DataKeyHash = KEY.hashed();
    ///
    /// assert_eq!(KEY_HASH.to_bytes(), [0xe2, 0xb6, 0x17, 0x71]);
    /// ```
    #[inline]
    pub const fn hashed(self) -> DataKeyHash {
        self.hash
    }

    /// Gets the metadata associated with this [`DataKey`].
    #[inline]
    pub const fn metadata(self) -> DataKeyMetadata {
        self.metadata
    }

    /// Constructs a [`DataKey`] from a path and metadata.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_provider::data_key;
    /// use icu_provider::DataKey;
    ///
    /// const CONST_KEY: DataKey = data_key!("foo@1");
    ///
    /// let runtime_key =
    ///     DataKey::from_path_and_metadata(CONST_KEY.path(), CONST_KEY.metadata());
    ///
    /// assert_eq!(CONST_KEY, runtime_key);
    /// ```
    #[inline]
    pub const fn from_path_and_metadata(path: DataKeyPath, metadata: DataKeyMetadata) -> Self {
        Self {
            path,
            hash: DataKeyHash::compute_from_path(path),
            metadata,
        }
    }

    #[doc(hidden)]
    // Error is a str of the expected character class and the index where it wasn't encountered
    // The indexing operations in this function have been reviewed in detail and won't panic.
    #[allow(clippy::indexing_slicing)]
    pub const fn construct_internal(
        path: &'static str,
        metadata: DataKeyMetadata,
    ) -> Result<Self, (&'static str, usize)> {
        if path.len() < leading_tag!().len() + trailing_tag!().len() {
            return Err(("tag", 0));
        }
        // Start and end of the untagged part
        let start = leading_tag!().len();
        let end = path.len() - trailing_tag!().len();

        // Check tags
        let mut i = 0;
        while i < leading_tag!().len() {
            if path.as_bytes()[i] != leading_tag!().as_bytes()[i] {
                return Err(("tag", 0));
            }
            i += 1;
        }
        i = 0;
        while i < trailing_tag!().len() {
            if path.as_bytes()[end + i] != trailing_tag!().as_bytes()[i] {
                return Err(("tag", end + 1));
            }
            i += 1;
        }

        match Self::validate_path_manual_slice(path, start, end) {
            Ok(()) => (),
            Err(e) => return Err(e),
        };

        let path = DataKeyPath { tagged: path };

        Ok(Self {
            path,
            hash: DataKeyHash::compute_from_path(path),
            metadata,
        })
    }

    const fn validate_path_manual_slice(
        path: &'static str,
        start: usize,
        end: usize,
    ) -> Result<(), (&'static str, usize)> {
        debug_assert!(start <= end);
        debug_assert!(end <= path.len());
        // Regex: [a-zA-Z0-9_][a-zA-Z0-9_/]*@[0-9]+
        enum State {
            Empty,
            Body,
            At,
            Version,
        }
        use State::*;
        let mut i = start;
        let mut state = Empty;
        loop {
            let byte = if i < end {
                #[allow(clippy::indexing_slicing)] // protected by debug assertion
                Some(path.as_bytes()[i])
            } else {
                None
            };
            state = match (state, byte) {
                (Empty | Body, Some(b'a'..=b'z' | b'A'..=b'Z' | b'0'..=b'9' | b'_')) => Body,
                (Body, Some(b'/')) => Body,
                (Body, Some(b'@')) => At,
                (At | Version, Some(b'0'..=b'9')) => Version,
                // One of these cases will be hit at the latest when i == end, so the loop converges.
                (Version, None) => {
                    return Ok(());
                }

                (Empty, _) => return Err(("[a-zA-Z0-9_]", i)),
                (Body, _) => return Err(("[a-zA-z0-9_/@]", i)),
                (At, _) => return Err(("[0-9]", i)),
                (Version, _) => return Err(("[0-9]", i)),
            };
            i += 1;
        }
    }

    /// Returns [`Ok`] if this data key matches the argument, or the appropriate error.
    ///
    /// Convenience method for data providers that support a single [`DataKey`].
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_provider::prelude::*;
    ///
    /// const FOO_BAR: DataKey = icu_provider::data_key!("foo/bar@1");
    /// const FOO_BAZ: DataKey = icu_provider::data_key!("foo/baz@1");
    /// const BAR_BAZ: DataKey = icu_provider::data_key!("bar/baz@1");
    ///
    /// assert!(matches!(FOO_BAR.match_key(FOO_BAR), Ok(())));
    /// assert!(matches!(
    ///     FOO_BAR.match_key(FOO_BAZ),
    ///     Err(DataError {
    ///         kind: DataErrorKind::MissingDataKey,
    ///         ..
    ///     })
    /// ));
    /// assert!(matches!(
    ///     FOO_BAR.match_key(BAR_BAZ),
    ///     Err(DataError {
    ///         kind: DataErrorKind::MissingDataKey,
    ///         ..
    ///     })
    /// ));
    ///
    /// // The error context contains the argument:
    /// assert_eq!(FOO_BAR.match_key(BAR_BAZ).unwrap_err().key, Some(BAR_BAZ));
    /// ```
    pub fn match_key(self, key: Self) -> Result<(), DataError> {
        if self == key {
            Ok(())
        } else {
            Err(DataErrorKind::MissingDataKey.with_key(key))
        }
    }
}

/// See [`DataKey`].
#[macro_export]
macro_rules! data_key {
    ($path:expr) => {{
        $crate::data_key!($path, $crate::DataKeyMetadata::const_default())
    }};
    ($path:expr, $metadata:expr) => {{
        // Force the DataKey into a const context
        const RESOURCE_KEY_MACRO_CONST: $crate::DataKey = {
            match $crate::DataKey::construct_internal($crate::tagged!($path), $metadata) {
                Ok(v) => v,
                #[allow(clippy::panic)] // Const context
                Err(_) => panic!(concat!("Invalid resource key: ", $path)),
                // TODO Once formatting is const:
                // Err((expected, index)) => panic!(
                //     "Invalid resource key {:?}: expected {:?}, found {:?} ",
                //     $path,
                //     expected,
                //     $crate::tagged!($path).get(index..))
                // );
            }
        };
        RESOURCE_KEY_MACRO_CONST
    }};
}

impl fmt::Debug for DataKey {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("DataKey{")?;
        fmt::Display::fmt(self, f)?;
        f.write_char('}')?;
        Ok(())
    }
}

impl Writeable for DataKey {
    fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
        self.path().write_to(sink)
    }

    fn writeable_length_hint(&self) -> LengthHint {
        self.path().writeable_length_hint()
    }

    fn write_to_string(&self) -> Cow<str> {
        Cow::Borrowed(self.path().get())
    }
}

writeable::impl_display_with_writeable!(DataKey);

#[test]
fn test_path_syntax() {
    // Valid keys:
    DataKey::construct_internal(tagged!("hello/world@1"), Default::default()).unwrap();
    DataKey::construct_internal(tagged!("hello/world/foo@1"), Default::default()).unwrap();
    DataKey::construct_internal(tagged!("hello/world@999"), Default::default()).unwrap();
    DataKey::construct_internal(tagged!("hello_world/foo@1"), Default::default()).unwrap();
    DataKey::construct_internal(tagged!("hello_458/world@1"), Default::default()).unwrap();
    DataKey::construct_internal(tagged!("hello_world@1"), Default::default()).unwrap();

    // No version:
    assert_eq!(
        DataKey::construct_internal(tagged!("hello/world"), Default::default()),
        Err((
            "[a-zA-z0-9_/@]",
            concat!(leading_tag!(), "hello/world").len()
        ))
    );

    assert_eq!(
        DataKey::construct_internal(tagged!("hello/world@"), Default::default()),
        Err(("[0-9]", concat!(leading_tag!(), "hello/world@").len()))
    );
    assert_eq!(
        DataKey::construct_internal(tagged!("hello/world@foo"), Default::default()),
        Err(("[0-9]", concat!(leading_tag!(), "hello/world@").len()))
    );
    assert_eq!(
        DataKey::construct_internal(tagged!("hello/world@1foo"), Default::default()),
        Err(("[0-9]", concat!(leading_tag!(), "hello/world@1").len()))
    );

    // Meta no longer accepted:
    assert_eq!(
        DataKey::construct_internal(tagged!("foo@1[R]"), Default::default()),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );
    assert_eq!(
        DataKey::construct_internal(tagged!("foo@1[u-ca]"), Default::default()),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );
    assert_eq!(
        DataKey::construct_internal(tagged!("foo@1[R][u-ca]"), Default::default()),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );

    // Invalid meta:
    assert_eq!(
        DataKey::construct_internal(tagged!("foo@1[U]"), Default::default()),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );
    assert_eq!(
        DataKey::construct_internal(tagged!("foo@1[uca]"), Default::default()),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );
    assert_eq!(
        DataKey::construct_internal(tagged!("foo@1[u-"), Default::default()),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );
    assert_eq!(
        DataKey::construct_internal(tagged!("foo@1[u-caa]"), Default::default()),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );
    assert_eq!(
        DataKey::construct_internal(tagged!("foo@1[R"), Default::default()),
        Err(("[0-9]", concat!(leading_tag!(), "foo@1").len()))
    );

    // Invalid characters:
    assert_eq!(
        DataKey::construct_internal(tagged!("你好/世界@1"), Default::default()),
        Err(("[a-zA-Z0-9_]", leading_tag!().len()))
    );

    // Invalid tag:
    assert_eq!(
        DataKey::construct_internal(
            concat!("hello/world@1", trailing_tag!()),
            Default::default()
        ),
        Err(("tag", 0))
    );
    assert_eq!(
        DataKey::construct_internal(concat!(leading_tag!(), "hello/world@1"), Default::default()),
        Err(("tag", concat!(leading_tag!(), "hello/world@1").len()))
    );
    assert_eq!(
        DataKey::construct_internal("hello/world@1", Default::default()),
        Err(("tag", 0))
    );
}

#[test]
fn test_key_to_string() {
    struct KeyTestCase {
        pub key: DataKey,
        pub expected: &'static str,
    }

    for cas in [
        KeyTestCase {
            key: data_key!("core/cardinal@1"),
            expected: "core/cardinal@1",
        },
        KeyTestCase {
            key: data_key!("core/maxlengthsubcatg@1"),
            expected: "core/maxlengthsubcatg@1",
        },
        KeyTestCase {
            key: data_key!("core/cardinal@65535"),
            expected: "core/cardinal@65535",
        },
    ] {
        writeable::assert_writeable_eq!(&cas.key, cas.expected);
    }
}

#[test]
fn test_key_hash() {
    struct KeyTestCase {
        pub key: DataKey,
        pub hash: DataKeyHash,
        pub path: &'static str,
    }

    for cas in [
        KeyTestCase {
            key: data_key!("core/cardinal@1"),
            hash: DataKeyHash([172, 207, 42, 236]),
            path: "core/cardinal@1",
        },
        KeyTestCase {
            key: data_key!("core/maxlengthsubcatg@1"),
            hash: DataKeyHash([193, 6, 79, 61]),
            path: "core/maxlengthsubcatg@1",
        },
        KeyTestCase {
            key: data_key!("core/cardinal@65535"),
            hash: DataKeyHash([176, 131, 182, 223]),
            path: "core/cardinal@65535",
        },
    ] {
        assert_eq!(cas.hash, cas.key.hashed(), "{}", cas.path);
        assert_eq!(cas.path, &*cas.key.path(), "{}", cas.path);
    }
}
