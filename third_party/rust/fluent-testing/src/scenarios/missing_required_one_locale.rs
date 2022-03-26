use super::structs::*;
use crate::queries;

/// Tests bundle generation with a required resource that is missing from only the primary locale.
/// Since the resource is required, we should only fallback entirely to the next locale for all resources.
pub fn get_scenario() -> Scenario {
    Scenario::new(
        "missing_required_one_locale",
        vec![
            FileSource::new("browser", "browser/{locale}/", vec!["en-US", "pl"]),
            FileSource::new("missing", "missing-resource/{locale}/", vec!["en-US", "pl"]),
        ],
        vec!["en-US", "pl"],
        vec!["browser/sanitize.ftl", "missing/missing-one.ftl"],
        queries![
            (
                "history-section-label",
                "Historia",
                ExceptionalContext::None
            ),
            (
                "missing-one",
                "zaginiony",
                ExceptionalContext::RequiredResourceMissingFromLocale,
            )
        ],
    )
}
