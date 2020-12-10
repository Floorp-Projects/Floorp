// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

/// A description for the [`JweMetric`](crate::metrics::JweMetric) type.
///
/// When changing this trait, make sure all the operations are
/// implemented in the related type in `../metrics/`.
pub trait Jwe {
    /// Sets to the specified JWE value.
    ///
    /// # Arguments
    ///
    /// * `value` - the [`compact representation`](https://tools.ietf.org/html/rfc7516#appendix-A.2.7) of a JWE value.
    fn set_with_compact_representation<S: Into<String>>(&self, value: S);

    /// Builds a JWE value from its elements and set to it.
    ///
    /// # Arguments
    ///
    /// * `header` - the JWE Protected Header element.
    /// * `key` - the JWE Encrypted Key element.
    /// * `init_vector` - the JWE Initialization Vector element.
    /// * `cipher_text` - the JWE Ciphertext element.
    /// * `auth_tag` - the JWE Authentication Tag element.
    fn set<S: Into<String>>(&self, header: S, key: S, init_vector: S, cipher_text: S, auth_tag: S);

    /// **Exported for test purposes.**
    ///
    /// Gets the currently stored value as a string.
    ///
    /// This doesn't clear the stored value.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    fn test_get_value<'a, S: Into<Option<&'a str>>>(&self, ping_name: S) -> Option<String>;

    /// **Exported for test purposes.**
    ///
    /// Gets the currently stored JWE as a JSON String of the serialized value.
    ///
    /// This doesn't clear the stored value.
    ///
    /// # Arguments
    ///
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    fn test_get_value_as_json_string<'a, S: Into<Option<&'a str>>>(
        &self,
        ping_name: S,
    ) -> Option<String>;
}
