use prelude::*;
use rand;

impl Uuid {
    /// Creates a random [`Uuid`].
    ///
    /// This uses the [`rand`] crate's default task RNG as the source of random
    /// numbers. If you'd like to use a custom generator, don't use this
    /// method: use the `rand::Rand trait`'s `rand()` method instead.
    ///
    /// Note that usage of this method requires the `v4` feature of this crate
    /// to be enabled.
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let uuid = Uuid::new_v4();
    /// ```
    ///
    /// [`rand`]: https://crates.io/crates/rand
    /// [`Uuid`]: ../struct.Uuid.html
    pub fn new_v4() -> Self {
        use rand::RngCore;

        let mut rng = rand::thread_rng();
        let mut bytes = [0; 16];

        rng.fill_bytes(&mut bytes);

        Builder::from_bytes(bytes)
            .set_variant(Variant::RFC4122)
            .set_version(Version::Random)
            .build()
    }
}

#[cfg(test)]
mod tests {
    use prelude::*;

    #[test]
    fn test_new() {
        let uuid = Uuid::new_v4();

        assert_eq!(uuid.get_version(), Some(Version::Random));
        assert_eq!(uuid.get_variant(), Some(Variant::RFC4122));
    }

    #[test]
    fn test_get_version() {
        let uuid = Uuid::new_v4();

        assert_eq!(uuid.get_version(), Some(Version::Random));
        assert_eq!(uuid.get_version_num(), 4)
    }
}
