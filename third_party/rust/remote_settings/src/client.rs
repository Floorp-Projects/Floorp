/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::config::RemoteSettingsConfig;
use crate::error::{RemoteSettingsError, Result};
use crate::UniffiCustomTypeConverter;
use parking_lot::Mutex;
use serde::Deserialize;
use std::{
    borrow::Cow,
    time::{Duration, Instant},
};
use url::Url;
use viaduct::{Request, Response};

const HEADER_BACKOFF: &str = "Backoff";
const HEADER_ETAG: &str = "ETag";
const HEADER_RETRY_AFTER: &str = "Retry-After";

/// A simple HTTP client that can retrieve Remote Settings data using the properties by [ClientConfig].
/// Methods defined on this will fetch data from
/// <base_url>/v1/buckets/<bucket_name>/collections/<collection_name>/
pub struct Client {
    pub(crate) base_url: Url,
    pub(crate) bucket_name: String,
    pub(crate) collection_name: String,
    pub(crate) remote_state: Mutex<RemoteState>,
}

impl Client {
    /// Create a new [Client] with properties matching config.
    pub fn new(config: RemoteSettingsConfig) -> Result<Self> {
        let server_url = config
            .server_url
            .unwrap_or_else(|| String::from("https://firefox.settings.services.mozilla.com"));
        let bucket_name = config.bucket_name.unwrap_or_else(|| String::from("main"));
        let base_url = Url::parse(&server_url)?;

        Ok(Self {
            base_url,
            bucket_name,
            collection_name: config.collection_name,
            remote_state: Default::default(),
        })
    }

    /// Fetches all records for a collection that can be found in the server,
    /// bucket, and collection defined by the [ClientConfig] used to generate
    /// this [Client].
    pub fn get_records(&self) -> Result<RemoteSettingsResponse> {
        self.get_records_with_options(&GetItemsOptions::new())
    }

    /// Fetches all records for a collection that can be found in the server,
    /// bucket, and collection defined by the [ClientConfig] used to generate
    /// this [Client]. This function will return the raw network [Response].
    pub fn get_records_raw(&self) -> Result<Response> {
        self.get_records_raw_with_options(&GetItemsOptions::new())
    }

    /// Fetches all records that have been published since provided timestamp
    /// for a collection that can be found in the server, bucket, and
    /// collection defined by the [ClientConfig] used to generate this [Client].
    pub fn get_records_since(&self, timestamp: u64) -> Result<RemoteSettingsResponse> {
        self.get_records_with_options(
            GetItemsOptions::new().gt("last_modified", timestamp.to_string()),
        )
    }

    /// Fetches records from this client's collection with the given options.
    pub fn get_records_with_options(
        &self,
        options: &GetItemsOptions,
    ) -> Result<RemoteSettingsResponse> {
        let resp = self.get_records_raw_with_options(options)?;
        let records = resp.json::<RecordsResponse>()?.data;
        let etag = resp
            .headers
            .get(HEADER_ETAG)
            .ok_or_else(|| RemoteSettingsError::ResponseError("no etag header".into()))?;
        // Per https://docs.kinto-storage.org/en/stable/api/1.x/timestamps.html,
        // the `ETag` header value is a quoted integer. Trim the quotes before
        // parsing.
        let last_modified = etag.trim_matches('"').parse().map_err(|_| {
            RemoteSettingsError::ResponseError(format!(
                "expected quoted integer in etag header; got `{}`",
                etag
            ))
        })?;
        Ok(RemoteSettingsResponse {
            records,
            last_modified,
        })
    }

    /// Fetches a raw network [Response] for records from this client's
    /// collection with the given options.
    pub fn get_records_raw_with_options(&self, options: &GetItemsOptions) -> Result<Response> {
        let path = format!(
            "v1/buckets/{}/collections/{}/records",
            &self.bucket_name, &self.collection_name
        );
        let mut url = self.base_url.join(&path)?;
        for (name, value) in options.iter_query_pairs() {
            url.query_pairs_mut().append_pair(&name, &value);
        }
        self.make_request(url)
    }

    /// Downloads an attachment from [attachment_location]. NOTE: there are no
    /// guarantees about a maximum size, so use care when fetching potentially
    /// large attachments.
    pub fn get_attachment(&self, attachment_location: &str) -> Result<Vec<u8>> {
        Ok(self.get_attachment_raw(attachment_location)?.body)
    }

    /// Fetches a raw network [Response] for an attachment.
    pub fn get_attachment_raw(&self, attachment_location: &str) -> Result<Response> {
        // Important: We use a `let` binding here to ensure that the mutex is
        // unlocked immediately after cloning the URL. If we matched directly on
        // the `.lock()` expression, the mutex would stay locked until the end
        // of the `match`, causing a deadlock.
        let maybe_attachments_base_url = self.remote_state.lock().attachments_base_url.clone();

        let attachments_base_url = match maybe_attachments_base_url {
            Some(attachments_base_url) => attachments_base_url,
            None => {
                let server_info = self
                    .make_request(self.base_url.clone())?
                    .json::<ServerInfo>()?;
                let attachments_base_url = match server_info.capabilities.attachments {
                    Some(capability) => Url::parse(&capability.base_url)?,
                    None => Err(RemoteSettingsError::AttachmentsUnsupportedError)?,
                };
                self.remote_state.lock().attachments_base_url = Some(attachments_base_url.clone());
                attachments_base_url
            }
        };

        self.make_request(attachments_base_url.join(attachment_location)?)
    }

    fn make_request(&self, url: Url) -> Result<Response> {
        let mut current_remote_state = self.remote_state.lock();
        self.ensure_no_backoff(&mut current_remote_state.backoff)?;
        drop(current_remote_state);

        let req = Request::get(url);
        let resp = req.send()?;

        let mut current_remote_state = self.remote_state.lock();
        self.handle_backoff_hint(&resp, &mut current_remote_state.backoff)?;

        if resp.is_success() {
            Ok(resp)
        } else {
            Err(RemoteSettingsError::ResponseError(resp.text().to_string()))
        }
    }

    fn ensure_no_backoff(&self, current_state: &mut BackoffState) -> Result<()> {
        if let BackoffState::Backoff {
            observed_at,
            duration,
        } = *current_state
        {
            let elapsed_time = observed_at.elapsed();
            if elapsed_time >= duration {
                *current_state = BackoffState::Ok;
            } else {
                let remaining = duration - elapsed_time;
                return Err(RemoteSettingsError::BackoffError(remaining.as_secs()));
            }
        }
        Ok(())
    }

    fn handle_backoff_hint(
        &self,
        response: &Response,
        current_state: &mut BackoffState,
    ) -> Result<()> {
        let extract_backoff_header = |header| -> Result<u64> {
            Ok(response
                .headers
                .get_as::<u64, _>(header)
                .transpose()
                .unwrap_or_default() // Ignore number parsing errors.
                .unwrap_or(0))
        };
        // In practice these two headers are mutually exclusive.
        let backoff = extract_backoff_header(HEADER_BACKOFF)?;
        let retry_after = extract_backoff_header(HEADER_RETRY_AFTER)?;
        let max_backoff = backoff.max(retry_after);

        if max_backoff > 0 {
            *current_state = BackoffState::Backoff {
                observed_at: Instant::now(),
                duration: Duration::from_secs(max_backoff),
            };
        }
        Ok(())
    }
}

/// Data structure representing the top-level response from the Remote Settings.
/// [last_modified] will be extracted from the etag header of the response.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RemoteSettingsResponse {
    pub records: Vec<RemoteSettingsRecord>,
    pub last_modified: u64,
}

#[derive(Deserialize)]
struct RecordsResponse {
    data: Vec<RemoteSettingsRecord>,
}

/// A parsed Remote Settings record. Records can contain arbitrary fields, so clients
/// are required to further extract expected values from the [fields] member.
#[derive(Clone, Debug, Deserialize, Eq, PartialEq)]
pub struct RemoteSettingsRecord {
    pub id: String,
    pub last_modified: u64,
    #[serde(default)]
    pub deleted: bool,
    pub attachment: Option<Attachment>,
    #[serde(flatten)]
    pub fields: RsJsonObject,
}

/// Attachment metadata that can be optionally attached to a [Record]. The [location] should
/// included in calls to [Client::get_attachment].
#[derive(Clone, Debug, Deserialize, Eq, PartialEq)]
pub struct Attachment {
    pub filename: String,
    pub mimetype: String,
    pub location: String,
    pub hash: String,
    pub size: u64,
}

// At time of writing, UniFFI cannot rename iOS bindings and JsonObject conflicted with the declaration in Nimbus.
// This shouldn't really impact Android, since the type is converted into the platform
// JsonObject thanks to the UniFFI binding.
pub type RsJsonObject = serde_json::Map<String, serde_json::Value>;
impl UniffiCustomTypeConverter for RsJsonObject {
    type Builtin = String;
    fn into_custom(val: Self::Builtin) -> uniffi::Result<Self> {
        let json: serde_json::Value = serde_json::from_str(&val)?;

        match json {
            serde_json::Value::Object(obj) => Ok(obj),
            _ => Err(uniffi::deps::anyhow::anyhow!(
                "Unexpected JSON-non-object in the bagging area"
            )),
        }
    }

    fn from_custom(obj: Self) -> Self::Builtin {
        serde_json::Value::Object(obj).to_string()
    }
}

#[derive(Clone, Debug)]
pub(crate) struct RemoteState {
    attachments_base_url: Option<Url>,
    backoff: BackoffState,
}

impl Default for RemoteState {
    fn default() -> Self {
        Self {
            attachments_base_url: None,
            backoff: BackoffState::Ok,
        }
    }
}

/// Used in handling backoff responses from the Remote Settings server.
#[derive(Clone, Copy, Debug)]
pub(crate) enum BackoffState {
    Ok,
    Backoff {
        observed_at: Instant,
        duration: Duration,
    },
}

#[derive(Deserialize)]
struct ServerInfo {
    capabilities: Capabilities,
}

#[derive(Deserialize)]
struct Capabilities {
    attachments: Option<AttachmentsCapability>,
}

#[derive(Deserialize)]
struct AttachmentsCapability {
    base_url: String,
}

/// Options for requests to endpoints that return multiple items.
#[derive(Clone, Debug, Default)]
pub struct GetItemsOptions {
    filters: Vec<Filter>,
    sort: Vec<Sort>,
    fields: Vec<String>,
    limit: Option<u64>,
}

impl GetItemsOptions {
    /// Creates an empty option set.
    pub fn new() -> Self {
        Self::default()
    }

    /// Sets an option to only return items whose `field` is equal to the given
    /// `value`.
    ///
    /// `field` can be a simple or dotted field name, like `author` or
    /// `author.name`. `value` can be a bare number or string (like
    /// `2` or `Ben`), or a stringified JSON value (`"2.0"`, `[1, 2]`,
    /// `{"checked": true}`).
    pub fn eq(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Eq(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is not equal to the
    /// given `value`.
    pub fn not(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Not(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is an array that
    /// contains the given `value`. If `value` is a stringified JSON array, the
    /// field must contain all its elements.
    pub fn contains(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters
            .push(Filter::Contains(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is strictly less
    /// than the given `value`.
    pub fn lt(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Lt(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is strictly greater
    /// than the given `value`.
    pub fn gt(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Gt(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is less than or equal
    /// to the given `value`.
    pub fn max(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Max(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is greater than or
    /// equal to the given `value`.
    pub fn min(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Min(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items whose `field` is a string that
    /// contains the substring `value`. `value` can contain `*` wildcards.
    pub fn like(&mut self, field: impl Into<String>, value: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Like(field.into(), value.into()));
        self
    }

    /// Sets an option to only return items that have the given `field`.
    pub fn has(&mut self, field: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::Has(field.into()));
        self
    }

    /// Sets an option to only return items that do not have the given `field`.
    pub fn has_not(&mut self, field: impl Into<String>) -> &mut Self {
        self.filters.push(Filter::HasNot(field.into()));
        self
    }

    /// Sets an option to return items in `order` for the given `field`.
    pub fn sort(&mut self, field: impl Into<String>, order: SortOrder) -> &mut Self {
        self.sort.push(Sort(field.into(), order));
        self
    }

    /// Sets an option to only return the given `field` of each item.
    ///
    /// The special `id` and `last_modified` fields are always returned.
    pub fn field(&mut self, field: impl Into<String>) -> &mut Self {
        self.fields.push(field.into());
        self
    }

    /// Sets the option to return at most `count` items.
    pub fn limit(&mut self, count: u64) -> &mut Self {
        self.limit = Some(count);
        self
    }

    /// Returns an iterator of (name, value) query pairs for these options.
    pub fn iter_query_pairs(&self) -> impl Iterator<Item = (Cow<str>, Cow<str>)> {
        self.filters
            .iter()
            .map(Filter::as_query_pair)
            .chain({
                // For sorting (https://docs.kinto-storage.org/en/latest/api/1.x/sorting.html),
                // the query pair syntax is `_sort=field1,-field2`, where the
                // fields to sort by are specified in a comma-separated ordered
                // list, and `-` indicates descending order.
                (!self.sort.is_empty()).then(|| {
                    (
                        "_sort".into(),
                        (self
                            .sort
                            .iter()
                            .map(Sort::as_query_value)
                            .collect::<Vec<_>>()
                            .join(","))
                        .into(),
                    )
                })
            })
            .chain({
                // For selecting fields (https://docs.kinto-storage.org/en/latest/api/1.x/selecting_fields.html),
                // the query pair syntax is `_fields=field1,field2`.
                (!self.fields.is_empty()).then(|| ("_fields".into(), self.fields.join(",").into()))
            })
            .chain({
                // For pagination (https://docs.kinto-storage.org/en/latest/api/1.x/pagination.html),
                // the query pair syntax is `_limit={count}`.
                self.limit
                    .map(|count| ("_limit".into(), count.to_string().into()))
            })
    }
}

/// The order in which to return items.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum SortOrder {
    /// Smaller values first.
    Ascending,
    /// Larger values first.
    Descending,
}

#[derive(Clone, Debug)]
enum Filter {
    Eq(String, String),
    Not(String, String),
    Contains(String, String),
    Lt(String, String),
    Gt(String, String),
    Max(String, String),
    Min(String, String),
    Like(String, String),
    Has(String),
    HasNot(String),
}

impl Filter {
    fn as_query_pair(&self) -> (Cow<str>, Cow<str>) {
        // For filters (https://docs.kinto-storage.org/en/latest/api/1.x/filtering.html),
        // the query pair syntax is `[operator_]field=value` for each field.
        match self {
            Filter::Eq(field, value) => (field.into(), value.into()),
            Filter::Not(field, value) => (format!("not_{field}").into(), value.into()),
            Filter::Contains(field, value) => (format!("contains_{field}").into(), value.into()),
            Filter::Lt(field, value) => (format!("lt_{field}").into(), value.into()),
            Filter::Gt(field, value) => (format!("gt_{field}").into(), value.into()),
            Filter::Max(field, value) => (format!("max_{field}").into(), value.into()),
            Filter::Min(field, value) => (format!("min_{field}").into(), value.into()),
            Filter::Like(field, value) => (format!("like_{field}").into(), value.into()),
            Filter::Has(field) => (format!("has_{field}").into(), "true".into()),
            Filter::HasNot(field) => (format!("has_{field}").into(), "false".into()),
        }
    }
}

#[derive(Clone, Debug)]
struct Sort(String, SortOrder);

impl Sort {
    fn as_query_value(&self) -> Cow<str> {
        match self.1 {
            SortOrder::Ascending => self.0.as_str().into(),
            SortOrder::Descending => format!("-{}", self.0).into(),
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use expect_test::expect;
    use mockito::{mock, Matcher};
    #[test]
    fn test_defaults() {
        let config = RemoteSettingsConfig {
            server_url: None,
            bucket_name: None,
            collection_name: String::from("the-collection"),
        };
        let client = Client::new(config).unwrap();
        assert_eq!(
            Url::parse("https://firefox.settings.services.mozilla.com").unwrap(),
            client.base_url
        );
        assert_eq!(String::from("main"), client.bucket_name);
    }

    #[test]
    fn test_attachment_can_be_downloaded() {
        viaduct_reqwest::use_reqwest_backend();
        let server_info_m = mock("GET", "/")
            .with_body(attachment_metadata(mockito::server_url()))
            .with_status(200)
            .with_header("content-type", "application/json")
            .create();

        let attachment_location = "123.jpg";
        let attachment_bytes: Vec<u8> = "I'm a JPG, I swear".into();
        let attachment_m = mock(
            "GET",
            format!("/attachments/{}", attachment_location).as_str(),
        )
        .with_body(attachment_bytes.clone())
        .with_status(200)
        .with_header("content-type", "application/json")
        .create();

        let config = RemoteSettingsConfig {
            server_url: Some(mockito::server_url()),
            collection_name: String::from("the-collection"),
            bucket_name: None,
        };

        let client = Client::new(config).unwrap();
        let first_resp = client.get_attachment(attachment_location).unwrap();
        let second_resp = client.get_attachment(attachment_location).unwrap();

        server_info_m.expect(1).assert();
        attachment_m.expect(2).assert();
        assert_eq!(first_resp, attachment_bytes);
        assert_eq!(second_resp, attachment_bytes);
    }

    #[test]
    fn test_attachment_errors_if_server_not_configured_for_attachments() {
        viaduct_reqwest::use_reqwest_backend();
        let server_info_m = mock("GET", "/")
            .with_body(NO_ATTACHMENTS_METADATA)
            .with_status(200)
            .with_header("content-type", "application/json")
            .create();

        let attachment_location = "123.jpg";
        let attachment_bytes: Vec<u8> = "I'm a JPG, I swear".into();
        let attachment_m = mock(
            "GET",
            format!("/attachments/{}", attachment_location).as_str(),
        )
        .with_body(attachment_bytes)
        .with_status(200)
        .with_header("content-type", "application/json")
        .create();

        let config = RemoteSettingsConfig {
            server_url: Some(mockito::server_url()),
            collection_name: String::from("the-collection"),
            bucket_name: None,
        };

        let client = Client::new(config).unwrap();
        let resp = client.get_attachment(attachment_location);
        server_info_m.expect(1).assert();
        attachment_m.expect(0).assert();
        assert!(matches!(
            resp,
            Err(RemoteSettingsError::AttachmentsUnsupportedError)
        ))
    }

    #[test]
    fn test_backoff() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .with_body(response_body())
        .with_status(200)
        .with_header("content-type", "application/json")
        .with_header("Backoff", "60")
        .with_header("etag", "\"1000\"")
        .create();
        let config = RemoteSettingsConfig {
            server_url: Some(mockito::server_url()),
            collection_name: String::from("the-collection"),
            bucket_name: Some(String::from("the-bucket")),
        };
        let http_client = Client::new(config).unwrap();

        assert!(http_client.get_records().is_ok());
        let second_resp = http_client.get_records();
        assert!(matches!(
            second_resp,
            Err(RemoteSettingsError::BackoffError(_))
        ));
        m.expect(1).assert();
    }

    #[test]
    fn test_500_retry_after() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .with_body("Boom!")
        .with_status(500)
        .with_header("Retry-After", "60")
        .create();
        let config = RemoteSettingsConfig {
            server_url: Some(mockito::server_url()),
            collection_name: String::from("the-collection"),
            bucket_name: Some(String::from("the-bucket")),
        };
        let http_client = Client::new(config).unwrap();
        assert!(http_client.get_records().is_err());
        let second_request = http_client.get_records();
        assert!(matches!(
            second_request,
            Err(RemoteSettingsError::BackoffError(_))
        ));
        m.expect(1).assert();
    }

    #[test]
    fn test_options() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .match_query(Matcher::AllOf(vec![
            Matcher::UrlEncoded("a".into(), "b".into()),
            Matcher::UrlEncoded("lt_c.d".into(), "5".into()),
            Matcher::UrlEncoded("gt_e".into(), "15".into()),
            Matcher::UrlEncoded("max_f".into(), "20".into()),
            Matcher::UrlEncoded("min_g".into(), "10".into()),
            Matcher::UrlEncoded("not_h".into(), "i".into()),
            Matcher::UrlEncoded("like_j".into(), "*k*".into()),
            Matcher::UrlEncoded("has_l".into(), "true".into()),
            Matcher::UrlEncoded("has_m".into(), "false".into()),
            Matcher::UrlEncoded("contains_n".into(), "o".into()),
            Matcher::UrlEncoded("_sort".into(), "-b,a".into()),
            Matcher::UrlEncoded("_fields".into(), "a,c,b".into()),
            Matcher::UrlEncoded("_limit".into(), "3".into()),
        ]))
        .with_body(response_body())
        .with_status(200)
        .with_header("content-type", "application/json")
        .with_header("etag", "\"1000\"")
        .create();
        let config = RemoteSettingsConfig {
            server_url: Some(mockito::server_url()),
            collection_name: String::from("the-collection"),
            bucket_name: Some(String::from("the-bucket")),
        };
        let http_client = Client::new(config).unwrap();
        let mut options = GetItemsOptions::new();
        options
            .field("a")
            .field("c")
            .field("b")
            .eq("a", "b")
            .lt("c.d", "5")
            .gt("e", "15")
            .max("f", "20")
            .min("g", "10")
            .not("h", "i")
            .like("j", "*k*")
            .has("l")
            .has_not("m")
            .contains("n", "o")
            .sort("b", SortOrder::Descending)
            .sort("a", SortOrder::Ascending)
            .limit(3);

        assert!(http_client.get_records_raw_with_options(&options).is_ok());
        expect![[r#"
            RemoteSettingsResponse {
                records: [
                    RemoteSettingsRecord {
                        id: "c5dcd1da-7126-4abb-846b-ec85b0d4d0d7",
                        last_modified: 1677694949407,
                        deleted: false,
                        attachment: Some(
                            Attachment {
                                filename: "jgp-attachment.jpg",
                                mimetype: "image/jpeg",
                                location: "the-bucket/the-collection/d3a5eccc-f0ca-42c3-b0bb-c0d4408c21c9.jpg",
                                hash: "2cbd593f3fd5f1585f92265433a6696a863bc98726f03e7222135ff0d8e83543",
                                size: 1374325,
                            },
                        ),
                        fields: {
                            "content": String(
                                "content",
                            ),
                            "schema": Number(
                                1677694447771,
                            ),
                            "title": String(
                                "jpg-attachment",
                            ),
                        },
                    },
                    RemoteSettingsRecord {
                        id: "ff301910-6bf5-4cfe-bc4c-5c80308661a5",
                        last_modified: 1677694470354,
                        deleted: false,
                        attachment: Some(
                            Attachment {
                                filename: "pdf-attachment.pdf",
                                mimetype: "application/pdf",
                                location: "the-bucket/the-collection/5f7347c2-af92-411d-a65b-f794f9b5084c.pdf",
                                hash: "de1cde3571ef3faa77ea0493276de9231acaa6f6651602e93aa1036f51181e9b",
                                size: 157,
                            },
                        ),
                        fields: {
                            "content": String(
                                "content",
                            ),
                            "schema": Number(
                                1677694447771,
                            ),
                            "title": String(
                                "with-attachment",
                            ),
                        },
                    },
                    RemoteSettingsRecord {
                        id: "7403c6f9-79be-4e0c-a37a-8f2b5bd7ad58",
                        last_modified: 1677694455368,
                        deleted: false,
                        attachment: None,
                        fields: {
                            "content": String(
                                "content",
                            ),
                            "schema": Number(
                                1677694447771,
                            ),
                            "title": String(
                                "no-attachment",
                            ),
                        },
                    },
                    RemoteSettingsRecord {
                        id: "9320f53c-0a39-4997-9120-62ff597ffb26",
                        last_modified: 1690921847416,
                        deleted: true,
                        attachment: None,
                        fields: {},
                    },
                ],
                last_modified: 1000,
            }
        "#]].assert_debug_eq(&http_client
            .get_records_with_options(&options)
            .unwrap());
        m.expect(2).assert();
    }

    #[test]
    fn test_backoff_recovery() {
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
            collection_name: String::from("the-collection"),
            bucket_name: Some(String::from("the-bucket")),
        };
        let http_client = Client::new(config).unwrap();
        // First, sanity check that manipulating the remote state does something.
        let mut current_remote_state = http_client.remote_state.lock();
        current_remote_state.backoff = BackoffState::Backoff {
            observed_at: Instant::now(),
            duration: Duration::from_secs(30),
        };
        drop(current_remote_state);
        assert!(matches!(
            http_client.get_records(),
            Err(RemoteSettingsError::BackoffError(_))
        ));
        // Then do the actual test.
        let mut current_remote_state = http_client.remote_state.lock();
        current_remote_state.backoff = BackoffState::Backoff {
            observed_at: Instant::now() - Duration::from_secs(31),
            duration: Duration::from_secs(30),
        };
        drop(current_remote_state);
        assert!(http_client.get_records().is_ok());
        m.expect(1).assert();
    }

    #[test]
    fn test_record_fields() {
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
            collection_name: String::from("the-collection"),
            bucket_name: Some(String::from("the-bucket")),
        };
        let http_client = Client::new(config).unwrap();
        let response = http_client.get_records().unwrap();
        expect![[r#"
            RemoteSettingsResponse {
                records: [
                    RemoteSettingsRecord {
                        id: "c5dcd1da-7126-4abb-846b-ec85b0d4d0d7",
                        last_modified: 1677694949407,
                        deleted: false,
                        attachment: Some(
                            Attachment {
                                filename: "jgp-attachment.jpg",
                                mimetype: "image/jpeg",
                                location: "the-bucket/the-collection/d3a5eccc-f0ca-42c3-b0bb-c0d4408c21c9.jpg",
                                hash: "2cbd593f3fd5f1585f92265433a6696a863bc98726f03e7222135ff0d8e83543",
                                size: 1374325,
                            },
                        ),
                        fields: {
                            "content": String(
                                "content",
                            ),
                            "schema": Number(
                                1677694447771,
                            ),
                            "title": String(
                                "jpg-attachment",
                            ),
                        },
                    },
                    RemoteSettingsRecord {
                        id: "ff301910-6bf5-4cfe-bc4c-5c80308661a5",
                        last_modified: 1677694470354,
                        deleted: false,
                        attachment: Some(
                            Attachment {
                                filename: "pdf-attachment.pdf",
                                mimetype: "application/pdf",
                                location: "the-bucket/the-collection/5f7347c2-af92-411d-a65b-f794f9b5084c.pdf",
                                hash: "de1cde3571ef3faa77ea0493276de9231acaa6f6651602e93aa1036f51181e9b",
                                size: 157,
                            },
                        ),
                        fields: {
                            "content": String(
                                "content",
                            ),
                            "schema": Number(
                                1677694447771,
                            ),
                            "title": String(
                                "with-attachment",
                            ),
                        },
                    },
                    RemoteSettingsRecord {
                        id: "7403c6f9-79be-4e0c-a37a-8f2b5bd7ad58",
                        last_modified: 1677694455368,
                        deleted: false,
                        attachment: None,
                        fields: {
                            "content": String(
                                "content",
                            ),
                            "schema": Number(
                                1677694447771,
                            ),
                            "title": String(
                                "no-attachment",
                            ),
                        },
                    },
                    RemoteSettingsRecord {
                        id: "9320f53c-0a39-4997-9120-62ff597ffb26",
                        last_modified: 1690921847416,
                        deleted: true,
                        attachment: None,
                        fields: {},
                    },
                ],
                last_modified: 1000,
            }
        "#]].assert_debug_eq(&response);
        m.expect(1).assert();
    }

    #[test]
    fn test_missing_etag() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .with_body(response_body())
        .with_status(200)
        .with_header("content-type", "application/json")
        .create();

        let config = RemoteSettingsConfig {
            server_url: Some(mockito::server_url()),
            bucket_name: Some(String::from("the-bucket")),
            collection_name: String::from("the-collection"),
        };
        let client = Client::new(config).unwrap();

        let err = client.get_records().unwrap_err();
        assert!(
            matches!(err, RemoteSettingsError::ResponseError(_)),
            "Want response error for missing `ETag`; got {}",
            err
        );
        m.expect(1).assert();
    }

    #[test]
    fn test_invalid_etag() {
        viaduct_reqwest::use_reqwest_backend();
        let m = mock(
            "GET",
            "/v1/buckets/the-bucket/collections/the-collection/records",
        )
        .with_body(response_body())
        .with_status(200)
        .with_header("content-type", "application/json")
        .with_header("etag", "bad!")
        .create();

        let config = RemoteSettingsConfig {
            server_url: Some(mockito::server_url()),
            bucket_name: Some(String::from("the-bucket")),
            collection_name: String::from("the-collection"),
        };
        let client = Client::new(config).unwrap();

        let err = client.get_records().unwrap_err();
        assert!(
            matches!(err, RemoteSettingsError::ResponseError(_)),
            "Want response error for invalid `ETag`; got {}",
            err
        );
        m.expect(1).assert();
    }

    fn attachment_metadata(base_url: String) -> String {
        format!(
            r#"
            {{
                "capabilities": {{
                    "admin": {{
                        "description": "Serves the admin console.",
                        "url": "https://github.com/Kinto/kinto-admin/",
                        "version": "2.0.0"
                    }},
                    "attachments": {{
                        "description": "Add file attachments to records",
                        "url": "https://github.com/Kinto/kinto-attachment/",
                        "version": "6.3.1",
                        "base_url": "{}/attachments/"
                    }}
                }}
            }}
    "#,
            base_url
        )
    }

    const NO_ATTACHMENTS_METADATA: &str = r#"
    {
      "capabilities": {
          "admin": {
            "description": "Serves the admin console.",
            "url": "https://github.com/Kinto/kinto-admin/",
            "version": "2.0.0"
          }
      }
    }
  "#;

    fn response_body() -> String {
        format!(
            r#"
        {{
            "data": [
                {},
                {},
                {},
                {}
            ]
          }}"#,
            JPG_ATTACHMENT, PDF_ATTACHMENT, NO_ATTACHMENT, TOMBSTONE
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

    const TOMBSTONE: &str = r#"
    {
      "id": "9320f53c-0a39-4997-9120-62ff597ffb26",
      "last_modified": 1690921847416,
      "deleted": true
    }
  "#;
}
