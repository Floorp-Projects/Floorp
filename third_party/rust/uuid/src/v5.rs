use crate::prelude::*;
use sha1;

impl Uuid {
    /// Creates a UUID using a name from a namespace, based on the SHA-1 hash.
    ///
    /// A number of namespaces are available as constants in this crate:
    ///
    /// * [`NAMESPACE_DNS`]
    /// * [`NAMESPACE_OID`]
    /// * [`NAMESPACE_URL`]
    /// * [`NAMESPACE_X500`]
    ///
    /// Note that usage of this method requires the `v5` feature of this crate
    /// to be enabled.
    ///
    /// [`NAMESPACE_DNS`]: struct.Uuid.html#associatedconst.NAMESPACE_DNS
    /// [`NAMESPACE_OID`]: struct.Uuid.html#associatedconst.NAMESPACE_OID
    /// [`NAMESPACE_URL`]: struct.Uuid.html#associatedconst.NAMESPACE_URL
    /// [`NAMESPACE_X500`]: struct.Uuid.html#associatedconst.NAMESPACE_X500
    pub fn new_v5(namespace: &Uuid, name: &[u8]) -> Uuid {
        let mut hash = sha1::Sha1::new();

        hash.update(namespace.as_bytes());
        hash.update(name);

        let buffer = hash.digest().bytes();

        let mut bytes = crate::Bytes::default();
        bytes.copy_from_slice(&buffer[..16]);

        let mut builder = crate::builder::Builder::from_bytes(bytes);
        builder
            .set_variant(Variant::RFC4122)
            .set_version(Version::Sha1);

        builder.build()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::std::string::ToString;

    static FIXTURE: &'static [(&'static Uuid, &'static str, &'static str)] = &[
        (
            &Uuid::NAMESPACE_DNS,
            "example.org",
            "aad03681-8b63-5304-89e0-8ca8f49461b5",
        ),
        (
            &Uuid::NAMESPACE_DNS,
            "rust-lang.org",
            "c66bbb60-d62e-5f17-a399-3a0bd237c503",
        ),
        (
            &Uuid::NAMESPACE_DNS,
            "42",
            "7c411b5e-9d3f-50b5-9c28-62096e41c4ed",
        ),
        (
            &Uuid::NAMESPACE_DNS,
            "lorem ipsum",
            "97886a05-8a68-5743-ad55-56ab2d61cf7b",
        ),
        (
            &Uuid::NAMESPACE_URL,
            "example.org",
            "54a35416-963c-5dd6-a1e2-5ab7bb5bafc7",
        ),
        (
            &Uuid::NAMESPACE_URL,
            "rust-lang.org",
            "c48d927f-4122-5413-968c-598b1780e749",
        ),
        (
            &Uuid::NAMESPACE_URL,
            "42",
            "5c2b23de-4bad-58ee-a4b3-f22f3b9cfd7d",
        ),
        (
            &Uuid::NAMESPACE_URL,
            "lorem ipsum",
            "15c67689-4b85-5253-86b4-49fbb138569f",
        ),
        (
            &Uuid::NAMESPACE_OID,
            "example.org",
            "34784df9-b065-5094-92c7-00bb3da97a30",
        ),
        (
            &Uuid::NAMESPACE_OID,
            "rust-lang.org",
            "8ef61ecb-977a-5844-ab0f-c25ef9b8d5d6",
        ),
        (
            &Uuid::NAMESPACE_OID,
            "42",
            "ba293c61-ad33-57b9-9671-f3319f57d789",
        ),
        (
            &Uuid::NAMESPACE_OID,
            "lorem ipsum",
            "6485290d-f79e-5380-9e64-cb4312c7b4a6",
        ),
        (
            &Uuid::NAMESPACE_X500,
            "example.org",
            "e3635e86-f82b-5bbc-a54a-da97923e5c76",
        ),
        (
            &Uuid::NAMESPACE_X500,
            "rust-lang.org",
            "26c9c3e9-49b7-56da-8b9f-a0fb916a71a3",
        ),
        (
            &Uuid::NAMESPACE_X500,
            "42",
            "e4b88014-47c6-5fe0-a195-13710e5f6e27",
        ),
        (
            &Uuid::NAMESPACE_X500,
            "lorem ipsum",
            "b11f79a5-1e6d-57ce-a4b5-ba8531ea03d0",
        ),
    ];

    #[test]
    fn test_get_version() {
        let uuid =
            Uuid::new_v5(&Uuid::NAMESPACE_DNS, "rust-lang.org".as_bytes());

        assert_eq!(uuid.get_version(), Some(Version::Sha1));
        assert_eq!(uuid.get_version_num(), 5);
    }

    #[test]
    fn test_hyphenated() {
        for &(ref ns, ref name, ref expected) in FIXTURE {
            let uuid = Uuid::new_v5(*ns, name.as_bytes());

            assert_eq!(uuid.to_hyphenated().to_string(), *expected)
        }
    }

    #[test]
    fn test_new() {
        for &(ref ns, ref name, ref u) in FIXTURE {
            let uuid = Uuid::new_v5(*ns, name.as_bytes());

            assert_eq!(uuid.get_variant(), Some(Variant::RFC4122));
            assert_eq!(uuid.get_version(), Some(Version::Sha1));
            assert_eq!(Ok(uuid), u.parse());
        }
    }
}
