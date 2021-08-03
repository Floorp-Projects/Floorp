// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use glean::net::{PingUploader, UploadResult};
use url::Url;
use viaduct::Request;

/// An uploader that uses [Viaduct](https://github.com/mozilla/application-services/tree/main/components/viaduct).
#[derive(Debug)]
pub(crate) struct ViaductUploader;

impl PingUploader for ViaductUploader {
    /// Uploads a ping to a server.
    ///
    /// # Arguments
    ///
    /// * `url` - the URL path to upload the data to.
    /// * `body` - the serialized text data to send.
    /// * `headers` - a vector of tuples containing the headers to send with
    ///   the request, i.e. (Name, Value).
    fn upload(&self, url: String, body: Vec<u8>, headers: Vec<(String, String)>) -> UploadResult {
        log::trace!("FOG Ping Uploader uploading to {}", url);
        let url_clone = url.clone();
        let result: std::result::Result<UploadResult, viaduct::Error> = (move || {
            let debug_tagged = headers.iter().any(|(name, _)| name == "X-Debug-ID");
            let localhost_port = static_prefs::pref!("telemetry.fog.test.localhost_port");
            if localhost_port < 0
                || (localhost_port == 0 && !debug_tagged && cfg!(feature = "disable_upload"))
            {
                log::info!("FOG Ping uploader faking success");
                return Ok(UploadResult::HttpStatus(200));
            }
            let parsed_url = Url::parse(&url_clone)?;

            log::info!("FOG Ping uploader uploading to {:?}", parsed_url);

            let mut req = Request::post(parsed_url.clone()).body(body.clone());
            for (header_key, header_value) in &headers {
                req = req.header(header_key.to_owned(), header_value)?;
            }

            log::trace!("FOG Ping Uploader sending ping to {}", parsed_url);
            let res = req.send()?;
            Ok(UploadResult::HttpStatus(res.status.into()))
        })();
        log::trace!(
            "FOG Ping Uploader completed uploading to {} (Result {:?})",
            url,
            result
        );
        match result {
            Ok(result) => result,
            _ => UploadResult::UnrecoverableFailure,
        }
    }
}
