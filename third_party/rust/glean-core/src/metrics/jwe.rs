// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::fmt;
use std::str::FromStr;

use serde::Serialize;

use crate::error_recording::{record_error, ErrorType};
use crate::metrics::{Metric, MetricType};
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

const DEFAULT_MAX_CHARS_PER_VARIABLE_SIZE_ELEMENT: usize = 1024;

/// Verifies if a string is [`BASE64URL`](https://tools.ietf.org/html/rfc4648#section-5) compliant.
///
/// As such, the string must match the regex: `[a-zA-Z0-9\-\_]*`.
///
/// > **Note** As described in the [JWS specification](https://tools.ietf.org/html/rfc7515#section-2),
/// > the BASE64URL encoding used by JWE discards any padding,
/// > that is why we can ignore that for this validation.
///
/// The regex crate isn't used here because it adds to the binary size,
/// and the Glean SDK doesn't use regular expressions anywhere else.
fn validate_base64url_encoding(value: &str) -> bool {
    let mut iter = value.chars();

    loop {
        match iter.next() {
            // We are done, so the whole expression is valid.
            None => return true,
            // Valid characters.
            Some('_') | Some('-') | Some('a'..='z') | Some('A'..='Z') | Some('0'..='9') => (),
            // An invalid character.
            Some(_) => return false,
        }
    }
}

/// Representation of a [JWE](https://tools.ietf.org/html/rfc7516).
///
/// **Note** Variable sized elements will be constrained to a length of DEFAULT_MAX_CHARS_PER_VARIABLE_SIZE_ELEMENT,
/// this is a constraint introduced by Glean to prevent abuses and not part of the spec.
#[derive(Serialize)]
struct Jwe {
    /// A variable-size JWE protected header.
    header: String,
    /// A variable-size [encrypted key](https://tools.ietf.org/html/rfc7516#appendix-A.1.3).
    /// This can be an empty octet sequence.
    key: String,
    /// A fixed-size, 96-bit, base64 encoded [JWE Initialization vector](https://tools.ietf.org/html/rfc7516#appendix-A.1.4) (e.g. “48V1_ALb6US04U3b”).
    /// If not required by the encryption algorithm, can be an empty octet sequence.
    init_vector: String,
    /// The variable-size base64 encoded cipher text.
    cipher_text: String,
    /// A fixed-size, 132-bit, base64 encoded authentication tag.
    /// Can be an empty octet sequence.
    auth_tag: String,
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl Jwe {
    /// Create a new JWE struct.
    fn new<S: Into<String>>(
        header: S,
        key: S,
        init_vector: S,
        cipher_text: S,
        auth_tag: S,
    ) -> Result<Self, (ErrorType, String)> {
        let mut header = header.into();
        header = Self::validate_non_empty("header", header)?;
        header = Self::validate_max_size("header", header)?;
        header = Self::validate_base64url_encoding("header", header)?;

        let mut key = key.into();
        key = Self::validate_max_size("key", key)?;
        key = Self::validate_base64url_encoding("key", key)?;

        let mut init_vector = init_vector.into();
        init_vector = Self::validate_fixed_size_or_empty("init_vector", init_vector, 96)?;
        init_vector = Self::validate_base64url_encoding("init_vector", init_vector)?;

        let mut cipher_text = cipher_text.into();
        cipher_text = Self::validate_non_empty("cipher_text", cipher_text)?;
        cipher_text = Self::validate_max_size("cipher_text", cipher_text)?;
        cipher_text = Self::validate_base64url_encoding("cipher_text", cipher_text)?;

        let mut auth_tag = auth_tag.into();
        auth_tag = Self::validate_fixed_size_or_empty("auth_tag", auth_tag, 128)?;
        auth_tag = Self::validate_base64url_encoding("auth_tag", auth_tag)?;

        Ok(Self {
            header,
            key,
            init_vector,
            cipher_text,
            auth_tag,
        })
    }

    fn validate_base64url_encoding(
        name: &str,
        value: String,
    ) -> Result<String, (ErrorType, String)> {
        if !validate_base64url_encoding(&value) {
            return Err((
                ErrorType::InvalidValue,
                format!("`{}` element in JWE value is not valid BASE64URL.", name),
            ));
        }

        Ok(value)
    }

    fn validate_non_empty(name: &str, value: String) -> Result<String, (ErrorType, String)> {
        if value.is_empty() {
            return Err((
                ErrorType::InvalidValue,
                format!("`{}` element in JWE value must not be empty.", name),
            ));
        }

        Ok(value)
    }

    fn validate_max_size(name: &str, value: String) -> Result<String, (ErrorType, String)> {
        if value.len() > DEFAULT_MAX_CHARS_PER_VARIABLE_SIZE_ELEMENT {
            return Err((
                ErrorType::InvalidOverflow,
                format!(
                    "`{}` element in JWE value must not exceed {} characters.",
                    name, DEFAULT_MAX_CHARS_PER_VARIABLE_SIZE_ELEMENT
                ),
            ));
        }

        Ok(value)
    }

    fn validate_fixed_size_or_empty(
        name: &str,
        value: String,
        size_in_bits: usize,
    ) -> Result<String, (ErrorType, String)> {
        // Each Base64 digit represents exactly 6 bits of data.
        // By dividing the size_in_bits by 6 and ceiling the result,
        // we get the amount of characters the value should have.
        let num_chars = (size_in_bits as f32 / 6f32).ceil() as usize;
        if !value.is_empty() && value.len() != num_chars {
            return Err((
                ErrorType::InvalidOverflow,
                format!(
                    "`{}` element in JWE value must have exactly {}-bits or be empty.",
                    name, size_in_bits
                ),
            ));
        }

        Ok(value)
    }
}

/// Trait implementation to convert a JWE [`compact representation`](https://tools.ietf.org/html/rfc7516#appendix-A.2.7)
/// string into a Jwe struct.
impl FromStr for Jwe {
    type Err = (ErrorType, String);

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut elements: Vec<&str> = s.split('.').collect();

        if elements.len() != 5 {
            return Err((
                ErrorType::InvalidValue,
                "JWE value is not formatted as expected.".into(),
            ));
        }

        // Consume the vector extracting each part of the JWE from it.
        //
        // Safe unwraps, we already defined that the slice has five elements.
        let auth_tag = elements.pop().unwrap();
        let cipher_text = elements.pop().unwrap();
        let init_vector = elements.pop().unwrap();
        let key = elements.pop().unwrap();
        let header = elements.pop().unwrap();

        Self::new(header, key, init_vector, cipher_text, auth_tag)
    }
}

/// Trait implementation to print the Jwe struct as the proper JWE [`compact representation`](https://tools.ietf.org/html/rfc7516#appendix-A.2.7).
impl fmt::Display for Jwe {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{}.{}.{}.{}.{}",
            self.header, self.key, self.init_vector, self.cipher_text, self.auth_tag
        )
    }
}

/// A JWE metric.
///
/// This metric will be work as a "transport" for JWE encrypted data.
///
/// The actual encrypti on is done somewhere else,
/// Glean must only make sure the data is valid JWE.
#[derive(Clone, Debug)]
pub struct JweMetric {
    meta: CommonMetricData,
}

impl MetricType for JweMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }

    fn meta_mut(&mut self) -> &mut CommonMetricData {
        &mut self.meta
    }
}

impl JweMetric {
    /// Creates a new JWE metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self { meta }
    }

    /// Sets to the specified JWE value.
    ///
    /// # Arguments
    ///
    /// * `glean` - the Glean instance this metric belongs to.
    /// * `value` - the [`compact representation`](https://tools.ietf.org/html/rfc7516#appendix-A.2.7) of a JWE value.
    pub fn set_with_compact_representation<S: Into<String>>(&self, glean: &Glean, value: S) {
        if !self.should_record(glean) {
            return;
        }

        let value = value.into();
        match Jwe::from_str(&value) {
            Ok(_) => glean
                .storage()
                .record(glean, &self.meta, &Metric::Jwe(value)),
            Err((error_type, msg)) => record_error(glean, &self.meta, error_type, msg, None),
        };
    }

    /// Builds a JWE value from its elements and set to it.
    ///
    /// # Arguments
    ///
    /// * `glean` - the Glean instance this metric belongs to.
    /// * `header` - the JWE Protected Header element.
    /// * `key` - the JWE Encrypted Key element.
    /// * `init_vector` - the JWE Initialization Vector element.
    /// * `cipher_text` - the JWE Ciphertext element.
    /// * `auth_tag` - the JWE Authentication Tag element.
    pub fn set<S: Into<String>>(
        &self,
        glean: &Glean,
        header: S,
        key: S,
        init_vector: S,
        cipher_text: S,
        auth_tag: S,
    ) {
        if !self.should_record(glean) {
            return;
        }

        match Jwe::new(header, key, init_vector, cipher_text, auth_tag) {
            Ok(jwe) => glean
                .storage()
                .record(glean, &self.meta, &Metric::Jwe(jwe.to_string())),
            Err((error_type, msg)) => record_error(glean, &self.meta, error_type, msg, None),
        };
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as a string.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, glean: &Glean, storage_name: &str) -> Option<String> {
        match StorageManager.snapshot_metric(
            glean.storage(),
            storage_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Jwe(b)) => Some(b),
            _ => None,
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored JWE as a JSON String of the serialized value.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value_as_json_string(
        &self,
        glean: &Glean,
        storage_name: &str,
    ) -> Option<String> {
        self.test_get_value(glean, storage_name).map(|snapshot| {
            serde_json::to_string(
                &Jwe::from_str(&snapshot).expect("Stored JWE metric should be valid JWE value."),
            )
            .unwrap()
        })
    }
}

#[cfg(test)]
mod test {
    use super::*;

    const HEADER: &str = "eyJhbGciOiJSU0EtT0FFUCIsImVuYyI6IkEyNTZHQ00ifQ";
    const KEY: &str = "OKOawDo13gRp2ojaHV7LFpZcgV7T6DVZKTyKOMTYUmKoTCVJRgckCL9kiMT03JGeipsEdY3mx_etLbbWSrFr05kLzcSr4qKAq7YN7e9jwQRb23nfa6c9d-StnImGyFDbSv04uVuxIp5Zms1gNxKKK2Da14B8S4rzVRltdYwam_lDp5XnZAYpQdb76FdIKLaVmqgfwX7XWRxv2322i-vDxRfqNzo_tETKzpVLzfiwQyeyPGLBIO56YJ7eObdv0je81860ppamavo35UgoRdbYaBcoh9QcfylQr66oc6vFWXRcZ_ZT2LawVCWTIy3brGPi6UklfCpIMfIjf7iGdXKHzg";
    const INIT_VECTOR: &str = "48V1_ALb6US04U3b";
    const CIPHER_TEXT: &str =
        "5eym8TW_c8SuK0ltJ3rpYIzOeDQz7TALvtu6UG9oMo4vpzs9tX_EFShS8iB7j6jiSdiwkIr3ajwQzaBtQD_A";
    const AUTH_TAG: &str = "XFBoMYUZodetZdvTiFvSkQ";
    const JWE: &str = "eyJhbGciOiJSU0EtT0FFUCIsImVuYyI6IkEyNTZHQ00ifQ.OKOawDo13gRp2ojaHV7LFpZcgV7T6DVZKTyKOMTYUmKoTCVJRgckCL9kiMT03JGeipsEdY3mx_etLbbWSrFr05kLzcSr4qKAq7YN7e9jwQRb23nfa6c9d-StnImGyFDbSv04uVuxIp5Zms1gNxKKK2Da14B8S4rzVRltdYwam_lDp5XnZAYpQdb76FdIKLaVmqgfwX7XWRxv2322i-vDxRfqNzo_tETKzpVLzfiwQyeyPGLBIO56YJ7eObdv0je81860ppamavo35UgoRdbYaBcoh9QcfylQr66oc6vFWXRcZ_ZT2LawVCWTIy3brGPi6UklfCpIMfIjf7iGdXKHzg.48V1_ALb6US04U3b.5eym8TW_c8SuK0ltJ3rpYIzOeDQz7TALvtu6UG9oMo4vpzs9tX_EFShS8iB7j6jiSdiwkIr3ajwQzaBtQD_A.XFBoMYUZodetZdvTiFvSkQ";

    #[test]
    fn generates_jwe_from_correct_input() {
        let jwe = Jwe::from_str(JWE).unwrap();
        assert_eq!(jwe.header, HEADER);
        assert_eq!(jwe.key, KEY);
        assert_eq!(jwe.init_vector, INIT_VECTOR);
        assert_eq!(jwe.cipher_text, CIPHER_TEXT);
        assert_eq!(jwe.auth_tag, AUTH_TAG);

        assert!(Jwe::new(HEADER, KEY, INIT_VECTOR, CIPHER_TEXT, AUTH_TAG).is_ok());
    }

    #[test]
    fn jwe_validates_header_value_correctly() {
        // When header is empty, correct error is returned
        match Jwe::new("", KEY, INIT_VECTOR, CIPHER_TEXT, AUTH_TAG) {
            Ok(_) => panic!("Should not have built JWE successfully."),
            Err((error_type, _)) => assert_eq!(error_type, ErrorType::InvalidValue),
        }

        // When header is bigger than max size, correct error is returned
        let too_long = (0..1025).map(|_| "X").collect::<String>();
        match Jwe::new(
            too_long,
            KEY.into(),
            INIT_VECTOR.into(),
            CIPHER_TEXT.into(),
            AUTH_TAG.into(),
        ) {
            Ok(_) => panic!("Should not have built JWE successfully."),
            Err((error_type, _)) => assert_eq!(error_type, ErrorType::InvalidOverflow),
        }

        // When header is not valid BASE64URL, correct error is returned
        let not64 = "inv@alid value!";
        match Jwe::new(not64, KEY, INIT_VECTOR, CIPHER_TEXT, AUTH_TAG) {
            Ok(_) => panic!("Should not have built JWE successfully."),
            Err((error_type, _)) => assert_eq!(error_type, ErrorType::InvalidValue),
        }
    }

    #[test]
    fn jwe_validates_key_value_correctly() {
        // When key is empty,JWE is created
        assert!(Jwe::new(HEADER, "", INIT_VECTOR, CIPHER_TEXT, AUTH_TAG).is_ok());

        // When key is bigger than max size, correct error is returned
        let too_long = (0..1025).map(|_| "X").collect::<String>();
        match Jwe::new(HEADER, &too_long, INIT_VECTOR, CIPHER_TEXT, AUTH_TAG) {
            Ok(_) => panic!("Should not have built JWE successfully."),
            Err((error_type, _)) => assert_eq!(error_type, ErrorType::InvalidOverflow),
        }

        // When key is not valid BASE64URL, correct error is returned
        let not64 = "inv@alid value!";
        match Jwe::new(HEADER, not64, INIT_VECTOR, CIPHER_TEXT, AUTH_TAG) {
            Ok(_) => panic!("Should not have built JWE successfully."),
            Err((error_type, _)) => assert_eq!(error_type, ErrorType::InvalidValue),
        }
    }

    #[test]
    fn jwe_validates_init_vector_value_correctly() {
        // When init_vector is empty, JWE is created
        assert!(Jwe::new(HEADER, KEY, "", CIPHER_TEXT, AUTH_TAG).is_ok());

        // When init_vector is not the correct size, correct error is returned
        match Jwe::new(HEADER, KEY, "foo", CIPHER_TEXT, AUTH_TAG) {
            Ok(_) => panic!("Should not have built JWE successfully."),
            Err((error_type, _)) => assert_eq!(error_type, ErrorType::InvalidOverflow),
        }

        // When init_vector is not valid BASE64URL, correct error is returned
        let not64 = "inv@alid value!!";
        match Jwe::new(HEADER, KEY, not64, CIPHER_TEXT, AUTH_TAG) {
            Ok(_) => panic!("Should not have built JWE successfully."),
            Err((error_type, _)) => assert_eq!(error_type, ErrorType::InvalidValue),
        }
    }

    #[test]
    fn jwe_validates_cipher_text_value_correctly() {
        // When cipher_text is empty, correct error is returned
        match Jwe::new(HEADER, KEY, INIT_VECTOR, "", AUTH_TAG) {
            Ok(_) => panic!("Should not have built JWE successfully."),
            Err((error_type, _)) => assert_eq!(error_type, ErrorType::InvalidValue),
        }

        // When cipher_text is bigger than max size, correct error is returned
        let too_long = (0..1025).map(|_| "X").collect::<String>();
        match Jwe::new(HEADER, KEY, INIT_VECTOR, &too_long, AUTH_TAG) {
            Ok(_) => panic!("Should not have built JWE successfully."),
            Err((error_type, _)) => assert_eq!(error_type, ErrorType::InvalidOverflow),
        }

        // When cipher_text is not valid BASE64URL, correct error is returned
        let not64 = "inv@alid value!";
        match Jwe::new(HEADER, KEY, INIT_VECTOR, not64, AUTH_TAG) {
            Ok(_) => panic!("Should not have built JWE successfully."),
            Err((error_type, _)) => assert_eq!(error_type, ErrorType::InvalidValue),
        }
    }

    #[test]
    fn jwe_validates_auth_tag_value_correctly() {
        // When auth_tag is empty, JWE is created
        assert!(Jwe::new(HEADER, KEY, INIT_VECTOR, CIPHER_TEXT, "").is_ok());

        // When auth_tag is not the correct size, correct error is returned
        match Jwe::new(HEADER, KEY, INIT_VECTOR, CIPHER_TEXT, "foo") {
            Ok(_) => panic!("Should not have built JWE successfully."),
            Err((error_type, _)) => assert_eq!(error_type, ErrorType::InvalidOverflow),
        }

        // When auth_tag is not valid BASE64URL, correct error is returned
        let not64 = "inv@alid value!!!!!!!!";
        match Jwe::new(HEADER, KEY, INIT_VECTOR, CIPHER_TEXT, not64) {
            Ok(_) => panic!("Should not have built JWE successfully."),
            Err((error_type, _)) => assert_eq!(error_type, ErrorType::InvalidValue),
        }
    }

    #[test]
    fn tranforms_jwe_struct_to_string_correctly() {
        let jwe = Jwe::from_str(JWE).unwrap();
        assert_eq!(jwe.to_string(), JWE);
    }

    #[test]
    fn validates_base64url_correctly() {
        assert!(validate_base64url_encoding(
            "0987654321AaBbCcDdEeFfGgHhIiKkLlMmNnOoPpQqRrSsTtUuVvXxWwYyZz-_"
        ));
        assert!(validate_base64url_encoding(""));
        assert!(!validate_base64url_encoding("aa aa"));
        assert!(!validate_base64url_encoding("aa.aa"));
        assert!(!validate_base64url_encoding("!nv@lid-val*e"));
    }
}
