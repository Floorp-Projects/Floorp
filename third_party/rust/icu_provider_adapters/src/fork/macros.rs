// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

/// Make a forking data provider with an arbitrary number of inner providers
/// that are known at build time.
///
/// # Examples
///
/// ```
/// use icu_provider_adapters::fork::ForkByKeyProvider;
///
/// // Some empty example providers:
/// #[derive(Default, PartialEq, Debug)]
/// struct Provider1;
/// #[derive(Default, PartialEq, Debug)]
/// struct Provider2;
/// #[derive(Default, PartialEq, Debug)]
/// struct Provider3;
///
/// // Combine them into one:
/// let forking1 = icu_provider_adapters::make_forking_provider!(
///     ForkByKeyProvider::new,
///     [
///         Provider1::default(),
///         Provider2::default(),
///         Provider3::default(),
///     ]
/// );
///
/// // This is equivalent to:
/// let forking2 = ForkByKeyProvider::new(
///     Provider1::default(),
///     ForkByKeyProvider::new(Provider2::default(), Provider3::default()),
/// );
///
/// assert_eq!(forking1, forking2);
/// ```
#[macro_export]
macro_rules! make_forking_provider {
    // Base case:
    ($combo_p:path, [ $a:expr, $b:expr, ]) => {
        $combo_p($a, $b)
    };
    // General list:
    ($combo_p:path, [ $a:expr, $b:expr, $($c:expr),+, ]) => {
        $combo_p($a, $crate::make_forking_provider!($combo_p, [ $b, $($c),+, ]))
    };
}

#[cfg(test)]
mod test {
    #[derive(Default)]
    struct Provider1;
    #[derive(Default)]
    struct Provider2;
    #[derive(Default)]
    struct Provider3;

    #[test]
    fn test_make_forking_provider() {
        make_forking_provider!(
            crate::fork::ForkByKeyProvider::new,
            [
                Provider1::default(),
                Provider2::default(),
                Provider3::default(),
            ]
        );
    }
}
