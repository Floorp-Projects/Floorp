/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub mod error;
pub use error::{RemoteSettingsError, Result};
use std::{fs::File, io::prelude::Write};
pub mod client;
pub use client::{
    Attachment, Client, GetItemsOptions, RemoteSettingsRecord, RemoteSettingsResponse,
    RsJsonObject, SortOrder,
};
pub mod config;
pub use config::RemoteSettingsConfig;

uniffi::include_scaffolding!("remote_settings");

pub struct RemoteSettings {
    pub config: RemoteSettingsConfig,
    client: Client,
}

impl RemoteSettings {
    pub fn new(config: RemoteSettingsConfig) -> Result<Self> {
        Ok(RemoteSettings {
            config: config.clone(),
            client: Client::new(config)?,
        })
    }

    pub fn get_records(&self) -> Result<RemoteSettingsResponse> {
        let resp = self.client.get_records()?;
        Ok(resp)
    }

    pub fn get_records_since(&self, timestamp: u64) -> Result<RemoteSettingsResponse> {
        let resp = self.client.get_records_since(timestamp)?;
        Ok(resp)
    }

    pub fn download_attachment_to_path(
        &self,
        attachment_location: String,
        path: String,
    ) -> Result<()> {
        let resp = self.client.get_attachment(&attachment_location)?;
        let mut file = File::create(path)?;
        file.write_all(&resp)?;
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::RemoteSettingsRecord;
    use mockito::{mock, Matcher};

    #[test]
    fn test_get_records() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .with_body(response_body())
        .with_status(200)
        .with_header("content-type", "application/json")
        .with_header("etag", "\"1000\"")
        .create();

        let config = RemoteSettingsConfig {
            server_url: Some(mockito::server_url()),
            bucket_name: Some(String::from("the-bucket")),
            collection_name: String::from("the-collection"),
        };
        let remote_settings = RemoteSettings::new(config).unwrap();

        let resp = remote_settings.get_records().unwrap();

        assert!(are_equal_json(JPG_ATTACHMENT, &resp.records[0]));
        assert_eq!(1000, resp.last_modified);
        m.expect(1).assert();
    }

    #[test]
    fn test_get_records_since() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .match_query(Matcher::UrlEncoded("gt_last_modified".into(), "500".into()))
        .with_body(response_body())
        .with_status(200)
        .with_header("content-type", "application/json")
        .with_header("etag", "\"1000\"")
        .create();

        let config = RemoteSettingsConfig {
            server_url: Some(mockito::server_url()),
            bucket_name: Some(String::from("the-bucket")),
            collection_name: String::from("the-collection"),
        };
        let remote_settings = RemoteSettings::new(config).unwrap();

        let resp = remote_settings.get_records_since(500).unwrap();
        assert!(are_equal_json(JPG_ATTACHMENT, &resp.records[0]));
        assert_eq!(1000, resp.last_modified);
        m.expect(1).assert();
    }

    // This test was designed as a proof-of-concept and requires a locally-run Remote Settings server.
    // If this were to be included in CI, it would require pulling the RS docker image and scripting
    // its configuration, as well as dynamically finding the attachment id, which would more closely
    // mimic a real world usecase.
    // #[test]
    #[allow(dead_code)]
    fn test_download() {
        viaduct_reqwest::use_reqwest_backend();
        let config = RemoteSettingsConfig {
            server_url: Some("http://localhost:8888".to_string()),
            bucket_name: Some(String::from("the-bucket")),
            collection_name: String::from("the-collection"),
        };
        let remote_settings = RemoteSettings::new(config).unwrap();

        remote_settings
            .download_attachment_to_path(
                "d3a5eccc-f0ca-42c3-b0bb-c0d4408c21c9.jpg".to_string(),
                "test.jpg".to_string(),
            )
            .unwrap();
    }

    fn are_equal_json(str: &str, rec: &RemoteSettingsRecord) -> bool {
        let r1: RemoteSettingsRecord = serde_json::from_str(str).unwrap();
        &r1 == rec
    }

    fn response_body() -> String {
        format!(
            r#"
        {{
            "data": [
                {},
                {},
                {}
            ]
          }}"#,
            JPG_ATTACHMENT, PDF_ATTACHMENT, NO_ATTACHMENT
        )
    }

    const JPG_ATTACHMENT: &str = r#"
          {
            "title": "jpg-attachment",
            "content": "content",
            "attachment": {
            "filename": "jgp-attachment.jpg",
            "location": "the-bucket/the-collection/d3a5eccc-f0ca-42c3-b0bb-c0d4408c21c9.jpg",
            "hash": "2cbd593f3fd5f1585f92265433a6696a863bc98726f03e7222135ff0d8e83543",
            "mimetype": "image/jpeg",
            "size": 1374325
            },
            "id": "c5dcd1da-7126-4abb-846b-ec85b0d4d0d7",
            "schema": 1677694447771,
            "last_modified": 1677694949407
          }
        "#;

    const PDF_ATTACHMENT: &str = r#"
          {
            "title": "with-attachment",
            "content": "content",
            "attachment": {
                "filename": "pdf-attachment.pdf",
                "location": "the-bucket/the-collection/5f7347c2-af92-411d-a65b-f794f9b5084c.pdf",
                "hash": "de1cde3571ef3faa77ea0493276de9231acaa6f6651602e93aa1036f51181e9b",
                "mimetype": "application/pdf",
                "size": 157
            },
            "id": "ff301910-6bf5-4cfe-bc4c-5c80308661a5",
            "schema": 1677694447771,
            "last_modified": 1677694470354
          }
        "#;

    const NO_ATTACHMENT: &str = r#"
          {
            "title": "no-attachment",
            "content": "content",
            "schema": 1677694447771,
            "id": "7403c6f9-79be-4e0c-a37a-8f2b5bd7ad58",
            "last_modified": 1677694455368
          }
        "#;
}
