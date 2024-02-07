use serde::{Deserialize, Serialize};

use crate::rs::{DownloadedGlobalConfig, DownloadedWeatherData};

/// Global Suggest configuration data.
#[derive(Clone, Default, Debug, Deserialize, Serialize)]
pub struct SuggestGlobalConfig {
    pub show_less_frequently_cap: i32,
}

impl From<&DownloadedGlobalConfig> for SuggestGlobalConfig {
    fn from(config: &DownloadedGlobalConfig) -> Self {
        Self {
            show_less_frequently_cap: config.configuration.show_less_frequently_cap,
        }
    }
}

/// Per-provider configuration data.
#[derive(Clone, Debug, Deserialize, Serialize)]
pub enum SuggestProviderConfig {
    Weather { min_keyword_length: i32 },
}

impl From<&DownloadedWeatherData> for SuggestProviderConfig {
    fn from(data: &DownloadedWeatherData) -> Self {
        Self::Weather {
            min_keyword_length: data.weather.min_keyword_length,
        }
    }
}
