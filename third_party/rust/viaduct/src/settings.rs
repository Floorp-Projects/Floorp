/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use once_cell::sync::Lazy;
use parking_lot::RwLock;
use std::time::Duration;
use url::Url;

/// Note: reqwest allows these only to be specified per-Client. concept-fetch
/// allows these to be specified on each call to fetch. I think it's worth
/// keeping a single global reqwest::Client in the reqwest backend, to simplify
/// the way we abstract away from these.
///
/// In the future, should we need it, we might be able to add a CustomClient type
/// with custom settings. In the reqwest backend this would store a Client, and
/// in the concept-fetch backend it would only store the settings, and populate
/// things on the fly.
#[derive(Debug)]
#[non_exhaustive]
pub struct Settings {
    pub read_timeout: Option<Duration>,
    pub connect_timeout: Option<Duration>,
    pub follow_redirects: bool,
    pub use_caches: bool,
    // For testing purposes, we allow exactly one additional Url which is
    // allowed to not be https.
    pub addn_allowed_insecure_url: Option<Url>,
}

#[cfg(target_os = "ios")]
const TIMEOUT_DURATION: Duration = Duration::from_secs(7);

#[cfg(not(target_os = "ios"))]
const TIMEOUT_DURATION: Duration = Duration::from_secs(10);

// The singleton instance of our settings.
pub static GLOBAL_SETTINGS: Lazy<RwLock<Settings>> = Lazy::new(|| {
    RwLock::new(Settings {
        read_timeout: Some(TIMEOUT_DURATION),
        connect_timeout: Some(TIMEOUT_DURATION),
        follow_redirects: true,
        use_caches: false,
        addn_allowed_insecure_url: None,
    })
});
