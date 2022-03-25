use fluent_fallback::types::{ResourceType, ToResourceId};

use super::structs::*;
use crate::queries;

/// Tests bundle generation with an optional resource that is missing from all locales.
/// Since the resource is optional, we should still be able to generate a bundle and
/// look up other present values, but we will fail to look up a value from the missing resource.
pub fn get_scenario() -> Scenario {
    Scenario::new(
        "missing_optional_all_locales",
        vec![
            FileSource::new("browser", "browser/{locale}/", vec!["en-US", "pl"]),
            FileSource::new("missing", "missing-resource/{locale}/", vec!["en-US", "pl"]),
        ],
        vec!["en-US", "pl"],
        vec![
            "browser/sanitize.ftl".into(),
            "missing/missing-one.ftl".to_resource_id(ResourceType::Optional),
            "missing/missing-all.ftl".to_resource_id(ResourceType::Optional),
        ],
        queries![
            ("history-section-label", "History", ExceptionalContext::None),
            (
                "missing-one",
                "zaginiony",
                ExceptionalContext::OptionalResourceMissingFromLocale,
            ),
            (
                "missing-all",
                "missing-all",
                ExceptionalContext::OptionalResourceMissingFromAllLocales,
            )
        ],
    )
}
