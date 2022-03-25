use super::structs::*;
use crate::queries;

/// Tests bundle generation with a required resource that is missing from all locales.
/// Since the resource is required, we cannot generate a bundle because no solution exists.
/// Lookups for all values should fail, because no bundle will be generated.
pub fn get_scenario() -> Scenario {
    Scenario::new(
        "missing_required_all_locales",
        vec![
            FileSource::new("browser", "browser/{locale}/", vec!["en-US", "pl"]),
            FileSource::new("missing", "missing-resource/{locale}/", vec!["en-US", "pl"]),
        ],
        vec!["en-US", "pl"],
        vec![
            "browser/sanitize.ftl",
            "missing/missing-one.ftl",
            "missing/missing-all.ftl",
        ],
        queries![
            (
                "history-section-label",
                "history-section-label",
                ExceptionalContext::None,
            ),
            (
                "missing-one",
                "missing-one",
                ExceptionalContext::RequiredResourceMissingFromLocale,
            ),
            (
                "missing-all",
                "missing-all",
                ExceptionalContext::RequiredResourceMissingFromAllLocales,
            )
        ],
    )
}
