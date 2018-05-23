use std::cmp::Ordering;
use std::result;

use ucd_util::{self, PropertyValues};

use hir;
use unicode_tables::age;
use unicode_tables::case_folding_simple::CASE_FOLDING_SIMPLE;
use unicode_tables::general_category;
use unicode_tables::property_bool;
use unicode_tables::property_names::PROPERTY_NAMES;
use unicode_tables::property_values::PROPERTY_VALUES;
use unicode_tables::script;
use unicode_tables::script_extension;

type Result<T> = result::Result<T, Error>;

/// An error that occurs when dealing with Unicode.
///
/// We don't impl the Error trait here because these always get converted
/// into other public errors. (This error type isn't exported.)
#[derive(Debug)]
pub enum Error {
    PropertyNotFound,
    PropertyValueNotFound,
}

/// An iterator over a codepoint's simple case equivalence class.
#[derive(Debug)]
pub struct SimpleFoldIter(::std::slice::Iter<'static, char>);

impl Iterator for SimpleFoldIter {
    type Item = char;

    fn next(&mut self) -> Option<char> {
        self.0.next().map(|c| *c)
    }
}

/// Return an iterator over the equivalence class of simple case mappings
/// for the given codepoint. The equivalence class does not include the
/// given codepoint.
///
/// If the equivalence class is empty, then this returns the next scalar
/// value that has a non-empty equivalence class, if it exists. If no such
/// scalar value exists, then `None` is returned. The point of this behavior
/// is to permit callers to avoid calling `simple_fold` more than they need
/// to, since there is some cost to fetching the equivalence class.
pub fn simple_fold(c: char) -> result::Result<SimpleFoldIter, Option<char>> {
    CASE_FOLDING_SIMPLE
        .binary_search_by_key(&c, |&(c1, _)| c1)
        .map(|i| SimpleFoldIter(CASE_FOLDING_SIMPLE[i].1.iter()))
        .map_err(|i| {
            if i >= CASE_FOLDING_SIMPLE.len() {
                None
            } else {
                Some(CASE_FOLDING_SIMPLE[i].0)
            }
        })
}

/// Returns true if and only if the given (inclusive) range contains at least
/// one Unicode scalar value that has a non-empty non-trivial simple case
/// mapping.
///
/// This function panics if `end < start`.
pub fn contains_simple_case_mapping(start: char, end: char) -> bool {
    assert!(start <= end);
    CASE_FOLDING_SIMPLE
        .binary_search_by(|&(c, _)| {
            if start <= c && c <= end {
                Ordering::Equal
            } else if c > end {
                Ordering::Greater
            } else {
                Ordering::Less
            }
        }).is_ok()
}

/// A query for finding a character class defined by Unicode. This supports
/// either use of a property name directly, or lookup by property value. The
/// former generally refers to Binary properties (see UTS#44, Table 8), but
/// as a special exception (see UTS#18, Section 1.2) both general categories
/// (an enumeration) and scripts (a catalog) are supported as if each of their
/// possible values were a binary property.
///
/// In all circumstances, property names and values are normalized and
/// canonicalized. That is, `GC == gc == GeneralCategory == general_category`.
///
/// The lifetime `'a` refers to the shorter of the lifetimes of property name
/// and property value.
#[derive(Debug)]
pub enum ClassQuery<'a> {
    /// Return a class corresponding to a Unicode binary property, named by
    /// a single letter.
    OneLetter(char),
    /// Return a class corresponding to a Unicode binary property.
    ///
    /// Note that, by special exception (see UTS#18, Section 1.2), both
    /// general category values and script values are permitted here as if
    /// they were a binary property.
    Binary(&'a str),
    /// Return a class corresponding to all codepoints whose property
    /// (identified by `property_name`) corresponds to the given value
    /// (identified by `property_value`).
    ByValue {
        /// A property name.
        property_name: &'a str,
        /// A property value.
        property_value: &'a str,
    },
}

impl<'a> ClassQuery<'a> {
    fn canonicalize(&self) -> Result<CanonicalClassQuery> {
        match *self {
            ClassQuery::OneLetter(c) => self.canonical_binary(&c.to_string()),
            ClassQuery::Binary(name) => self.canonical_binary(name),
            ClassQuery::ByValue { property_name, property_value } => {
                let property_name = normalize(property_name);
                let property_value = normalize(property_value);

                let canon_name = match canonical_prop(&property_name) {
                    None => return Err(Error::PropertyNotFound),
                    Some(canon_name) => canon_name,
                };
                Ok(match canon_name {
                    "General_Category" => {
                        let canon = match canonical_gencat(&property_value) {
                            None => return Err(Error::PropertyValueNotFound),
                            Some(canon) => canon,
                        };
                        CanonicalClassQuery::GeneralCategory(canon)
                    }
                    "Script" => {
                        let canon = match canonical_script(&property_value) {
                            None => return Err(Error::PropertyValueNotFound),
                            Some(canon) => canon,
                        };
                        CanonicalClassQuery::Script(canon)
                    }
                    _ => {
                        let vals = match property_values(canon_name) {
                            None => return Err(Error::PropertyValueNotFound),
                            Some(vals) => vals,
                        };
                        let canon_val = match canonical_value(
                            vals,
                            &property_value,
                        ) {
                            None => return Err(Error::PropertyValueNotFound),
                            Some(canon_val) => canon_val,
                        };
                        CanonicalClassQuery::ByValue {
                            property_name: canon_name,
                            property_value: canon_val,
                        }
                    }
                })
            }
        }
    }

    fn canonical_binary(&self, name: &str) -> Result<CanonicalClassQuery> {
        let norm = normalize(name);

        if let Some(canon) = canonical_prop(&norm) {
            return Ok(CanonicalClassQuery::Binary(canon));
        }
        if let Some(canon) = canonical_gencat(&norm) {
            return Ok(CanonicalClassQuery::GeneralCategory(canon));
        }
        if let Some(canon) = canonical_script(&norm) {
            return Ok(CanonicalClassQuery::Script(canon));
        }
        Err(Error::PropertyNotFound)
    }
}

/// Like ClassQuery, but its parameters have been canonicalized. This also
/// differentiates binary properties from flattened general categories and
/// scripts.
#[derive(Debug, Eq, PartialEq)]
enum CanonicalClassQuery {
    /// The canonical binary property name.
    Binary(&'static str),
    /// The canonical general category name.
    GeneralCategory(&'static str),
    /// The canonical script name.
    Script(&'static str),
    /// An arbitrary association between property and value, both of which
    /// have been canonicalized.
    ///
    /// Note that by construction, the property name of ByValue will never
    /// be General_Category or Script. Those two cases are subsumed by the
    /// eponymous variants.
    ByValue {
        /// The canonical property name.
        property_name: &'static str,
        /// The canonical property value.
        property_value: &'static str,
    },
}

/// Looks up a Unicode class given a query. If one doesn't exist, then
/// `None` is returned.
pub fn class<'a>(query: ClassQuery<'a>) -> Result<hir::ClassUnicode> {
    use self::CanonicalClassQuery::*;

    match query.canonicalize()? {
        Binary(name) => {
            property_set(property_bool::BY_NAME, name)
                .map(hir_class)
                .ok_or(Error::PropertyNotFound)
        }
        GeneralCategory("Any") => {
            Ok(hir_class(&[('\0', '\u{10FFFF}')]))
        }
        GeneralCategory("Assigned") => {
            let mut cls =
                property_set(general_category::BY_NAME, "Unassigned")
                    .map(hir_class)
                    .ok_or(Error::PropertyNotFound)?;
            cls.negate();
            Ok(cls)
        }
        GeneralCategory("ASCII") => {
            Ok(hir_class(&[('\0', '\x7F')]))
        }
        GeneralCategory(name) => {
            property_set(general_category::BY_NAME, name)
                .map(hir_class)
                .ok_or(Error::PropertyValueNotFound)
        }
        Script(name) => {
            property_set(script::BY_NAME, name)
                .map(hir_class)
                .ok_or(Error::PropertyValueNotFound)
        }
        ByValue { property_name: "Age", property_value } => {
            let mut class = hir::ClassUnicode::empty();
            for set in ages(property_value)? {
                class.union(&hir_class(set));
            }
            Ok(class)
        }
        ByValue { property_name: "Script_Extensions", property_value } => {
            property_set(script_extension::BY_NAME, property_value)
                .map(hir_class)
                .ok_or(Error::PropertyValueNotFound)
        }
        _ => {
            // What else should we support?
            Err(Error::PropertyNotFound)
        }
    }
}

/// Build a Unicode HIR class from a sequence of Unicode scalar value ranges.
pub fn hir_class(ranges: &[(char, char)]) -> hir::ClassUnicode {
    let hir_ranges: Vec<hir::ClassUnicodeRange> = ranges
        .iter()
        .map(|&(s, e)| hir::ClassUnicodeRange::new(s, e))
        .collect();
    hir::ClassUnicode::new(hir_ranges)
}

fn canonical_prop(normalized_name: &str) -> Option<&'static str> {
    ucd_util::canonical_property_name(PROPERTY_NAMES, normalized_name)
}

fn canonical_gencat(normalized_value: &str) -> Option<&'static str> {
    match normalized_value {
        "any" => Some("Any"),
        "assigned" => Some("Assigned"),
        "ascii" => Some("ASCII"),
        _ => {
            let gencats = property_values("General_Category").unwrap();
            canonical_value(gencats, normalized_value)
        }
    }
}

fn canonical_script(normalized_value: &str) -> Option<&'static str> {
    let scripts = property_values("Script").unwrap();
    canonical_value(scripts, normalized_value)
}

fn canonical_value(
    vals: PropertyValues,
    normalized_value: &str,
) -> Option<&'static str> {
    ucd_util::canonical_property_value(vals, normalized_value)
}

fn normalize(x: &str) -> String {
    let mut x = x.to_string();
    ucd_util::symbolic_name_normalize(&mut x);
    x
}

fn property_values(
    canonical_property_name: &'static str,
) -> Option<PropertyValues>
{
    ucd_util::property_values(PROPERTY_VALUES, canonical_property_name)
}

fn property_set(
    name_map: &'static [(&'static str, &'static [(char, char)])],
    canonical: &'static str,
) -> Option<&'static [(char, char)]> {
    name_map
        .binary_search_by_key(&canonical, |x| x.0)
        .ok()
        .map(|i| name_map[i].1)
}

/// An iterator over Unicode Age sets. Each item corresponds to a set of
/// codepoints that were added in a particular revision of Unicode. The
/// iterator yields items in chronological order.
#[derive(Debug)]
struct AgeIter {
    ages: &'static [(&'static str, &'static [(char, char)])],
}

fn ages(canonical_age: &str) -> Result<AgeIter> {
    const AGES: &'static [(&'static str, &'static [(char, char)])] = &[
        ("V1_1", age::V1_1),
        ("V2_0", age::V2_0),
        ("V2_1", age::V2_1),
        ("V3_0", age::V3_0),
        ("V3_1", age::V3_1),
        ("V3_2", age::V3_2),
        ("V4_0", age::V4_0),
        ("V4_1", age::V4_1),
        ("V5_0", age::V5_0),
        ("V5_1", age::V5_1),
        ("V5_2", age::V5_2),
        ("V6_0", age::V6_0),
        ("V6_1", age::V6_1),
        ("V6_2", age::V6_2),
        ("V6_3", age::V6_3),
        ("V7_0", age::V7_0),
        ("V8_0", age::V8_0),
        ("V9_0", age::V9_0),
        ("V10_0", age::V10_0),
    ];
    assert_eq!(AGES.len(), age::BY_NAME.len(), "ages are out of sync");

    let pos = AGES.iter().position(|&(age, _)| canonical_age == age);
    match pos {
        None => Err(Error::PropertyValueNotFound),
        Some(i) => Ok(AgeIter { ages: &AGES[..i+1] }),
    }
}

impl Iterator for AgeIter {
    type Item = &'static [(char, char)];

    fn next(&mut self) -> Option<&'static [(char, char)]> {
        if self.ages.is_empty() {
            None
        } else {
            let set = self.ages[0];
            self.ages = &self.ages[1..];
            Some(set.1)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{contains_simple_case_mapping, simple_fold};

    #[test]
    fn simple_fold_k() {
        let xs: Vec<char> = simple_fold('k').unwrap().collect();
        assert_eq!(xs, vec!['K', 'K']);

        let xs: Vec<char> = simple_fold('K').unwrap().collect();
        assert_eq!(xs, vec!['k', 'K']);

        let xs: Vec<char> = simple_fold('K').unwrap().collect();
        assert_eq!(xs, vec!['K', 'k']);
    }

    #[test]
    fn simple_fold_a() {
        let xs: Vec<char> = simple_fold('a').unwrap().collect();
        assert_eq!(xs, vec!['A']);

        let xs: Vec<char> = simple_fold('A').unwrap().collect();
        assert_eq!(xs, vec!['a']);
    }

    #[test]
    fn simple_fold_empty() {
        assert_eq!(Some('A'), simple_fold('?').unwrap_err());
        assert_eq!(Some('A'), simple_fold('@').unwrap_err());
        assert_eq!(Some('a'), simple_fold('[').unwrap_err());
        assert_eq!(Some('Ⰰ'), simple_fold('☃').unwrap_err());
    }

    #[test]
    fn simple_fold_max() {
        assert_eq!(None, simple_fold('\u{10FFFE}').unwrap_err());
        assert_eq!(None, simple_fold('\u{10FFFF}').unwrap_err());
    }

    #[test]
    fn range_contains() {
        assert!(contains_simple_case_mapping('A', 'A'));
        assert!(contains_simple_case_mapping('Z', 'Z'));
        assert!(contains_simple_case_mapping('A', 'Z'));
        assert!(contains_simple_case_mapping('@', 'A'));
        assert!(contains_simple_case_mapping('Z', '['));
        assert!(contains_simple_case_mapping('☃', 'Ⰰ'));

        assert!(!contains_simple_case_mapping('[', '['));
        assert!(!contains_simple_case_mapping('[', '`'));

        assert!(!contains_simple_case_mapping('☃', '☃'));
    }

    #[test]
    fn regression_466() {
        use super::{CanonicalClassQuery, ClassQuery};

        let q = ClassQuery::OneLetter('C');
        assert_eq!(
            q.canonicalize().unwrap(),
            CanonicalClassQuery::GeneralCategory("Other"));
    }
}
