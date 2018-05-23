/// The type of a property name table.
///
/// A property name table is a sequence of sorted tuples, where the first
/// value in each tuple is a normalized property name and the second value of
/// each tuple is the corresponding canonical property name.
pub type PropertyTable = &'static [(&'static str, &'static str)];

/// Find the canonical property name for the given normalized property name.
///
/// If no such property exists, then `None` is returned.
///
/// The normalized property name must have been normalized according to
/// UAX44 LM3, which can be done using `symbolic_name_normalize`.
pub fn canonical_property_name(
    property_table: PropertyTable,
    normalized_property_name: &str,
) -> Option<&'static str> {
    property_table
        .binary_search_by_key(&normalized_property_name, |&(n, _)| n)
        .ok()
        .map(|i| property_table[i].1)
}

/// Type of a property value table.
///
/// A property value table maps property names to a mapping of property values,
/// where the mapping of property values is represented by a sequence of
/// tuples. The first element of each tuple is a normalized property value
/// while the second element of each tuple is the corresponding canonical
/// property value.
///
/// Note that a property value table only includes values for properties that
/// are catalogs, enumerations or binary properties. Properties that have
/// string values (such as case or decomposition mappings), numeric values
/// or are miscellaneous are not represented in this table.
pub type PropertyValueTable = &'static [(&'static str, PropertyValues)];

/// A mapping of property values for a specific property.
///
/// The first element of each tuple is a normalized property value while the
/// second element of each tuple is the corresponding canonical property
/// value.
pub type PropertyValues = &'static [(&'static str, &'static str)];

/// Find the set of possible property values for a given property.
///
/// The set returned is a mapping expressed as a sorted list of tuples.
/// The first element of each tuple is a normalized property value while the
/// second element of each tuple is the corresponding canonical property
/// value.
///
/// If no such property exists, then `None` is returned.
///
/// The given property name must be in its canonical form, which can be
/// found using `canonical_property_name`.
pub fn property_values(
    property_value_table: PropertyValueTable,
    canonical_property_name: &str,
) -> Option<PropertyValues> {
    property_value_table
        .binary_search_by_key(&canonical_property_name, |&(n, _)| n)
        .ok()
        .map(|i| property_value_table[i].1)
}

/// Find the canonical property value for the given normalized property
/// value.
///
/// The given property values should correspond to the values for the property
/// under question, which can be found using `property_values`.
///
/// If no such property value exists, then `None` is returned.
///
/// The normalized property value must have been normalized according to
/// UAX44 LM3, which can be done using `symbolic_name_normalize`.
pub fn canonical_property_value(
    property_values: PropertyValues,
    normalized_property_value: &str,
) -> Option<&'static str> {
    // This is cute. The types line up, so why not?
    canonical_property_name(property_values, normalized_property_value)
}


#[cfg(test)]
mod tests {
    use unicode_tables::property_names::PROPERTY_NAMES;
    use unicode_tables::property_values::PROPERTY_VALUES;

    use super::{
        canonical_property_name, property_values, canonical_property_value,
    };

    #[test]
    fn canonical_property_name_1() {
        assert_eq!(
            canonical_property_name(PROPERTY_NAMES, "gc"),
            Some("General_Category"));
        assert_eq!(
            canonical_property_name(PROPERTY_NAMES, "generalcategory"),
            Some("General_Category"));
        assert_eq!(
            canonical_property_name(PROPERTY_NAMES, "g c"),
            None);
    }

    #[test]
    fn property_values_1() {
        assert_eq!(
            property_values(PROPERTY_VALUES, "White_Space"),
            Some(&[
                ("f", "No"), ("false", "No"), ("n", "No"), ("no", "No"),
                ("t", "Yes"), ("true", "Yes"), ("y", "Yes"), ("yes", "Yes"),
            ][..]));
    }

    #[test]
    fn canonical_property_value_1() {
        let values = property_values(PROPERTY_VALUES, "White_Space").unwrap();
        assert_eq!(canonical_property_value(values, "false"), Some("No"));
        assert_eq!(canonical_property_value(values, "t"), Some("Yes"));
        assert_eq!(canonical_property_value(values, "F"), None);
    }
}
