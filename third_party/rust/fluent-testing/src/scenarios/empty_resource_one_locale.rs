use super::structs::*;
use crate::queries;

/// Tests bundle generation with a queried value that is missing in only one resource
/// in the primary locale. This should cause the bundle to fallback to another locale
/// only for that value.
pub fn get_scenario() -> Scenario {
    Scenario::new(
        "empty_resource_one_locale",
        vec![
            FileSource::new("browser", "browser/{locale}/", vec!["en-US", "pl"]),
            FileSource::new("empty", "empty-resource/{locale}/", vec!["en-US", "pl"]),
        ],
        vec!["en-US", "pl"],
        vec!["browser/sanitize.ftl", "empty/empty-one.ftl"],
        queries![
            ("history-section-label", "History", ExceptionalContext::None),
            (
                "empty-one",
                "pusty",
                ExceptionalContext::ValueMissingFromResource,
            )
        ],
    )
}
