mod browser;
mod empty_resource_all_locales;
mod empty_resource_one_locale;
mod missing_optional_all_locales;
mod missing_optional_one_locale;
mod missing_required_all_locales;
mod missing_required_one_locale;
mod preferences;
mod simple;
pub mod structs;

use structs::*;

#[macro_export]
macro_rules! queries {
    ( $( $x:expr ),* ) => {
        {
            Queries(vec![
                $(
                    $x.into(),
                )*
            ])
        }
    };
}

pub fn get_scenarios() -> Vec<Scenario> {
    vec![
        simple::get_scenario(),
        browser::get_scenario(),
        preferences::get_scenario(),
        empty_resource_one_locale::get_scenario(),
        empty_resource_all_locales::get_scenario(),
        missing_optional_one_locale::get_scenario(),
        missing_optional_all_locales::get_scenario(),
        missing_required_one_locale::get_scenario(),
        missing_required_all_locales::get_scenario(),
    ]
}
