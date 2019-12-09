// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! A module containing glean-core code for supporting data migration
//! (i.e. sequence numbers) from glean-ac. This is a temporary module
//! planned to be removed in 2020, after the transition from glean-ac
//! is complete.

use crate::util::truncate_string_at_boundary;
use std::collections::HashMap;

use super::Glean;
use super::PingMaker;

const GLEAN_AC_SEQUENCE_SUFFIX: &str = "_seq";

/// Stores the sequence numbers from glean-ac in glean-core.
pub(super) fn migrate_sequence_numbers(glean: &Glean, seq_numbers: HashMap<String, i32>) {
    let ping_maker = PingMaker::new();

    for (store_name_with_suffix, next_seq) in seq_numbers.into_iter() {
        // Note: glean-ac stores the sequence numbers as '<ping_name>_seq',
        // glean-core requires '<ping_name>#sequence'.
        if !store_name_with_suffix.ends_with(GLEAN_AC_SEQUENCE_SUFFIX) {
            continue;
        }

        // Negative or 0 counters are definitively not worth importing.
        if next_seq <= 0 {
            continue;
        }

        let truncated_len = store_name_with_suffix
            .len()
            .saturating_sub(GLEAN_AC_SEQUENCE_SUFFIX.len());
        let store_name = truncate_string_at_boundary(store_name_with_suffix, truncated_len);

        ping_maker.set_ping_seq(glean, &store_name, next_seq);
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::tests::new_glean;

    #[test]
    fn invalid_storage_names_must_not_be_migrated() {
        let (glean, _) = new_glean();

        let mut ac_seq_numbers = HashMap::new();
        ac_seq_numbers.insert(String::from("control_seq"), 3);
        ac_seq_numbers.insert(String::from("ignored_seq-lol"), 85);

        let ping_maker = PingMaker::new();
        migrate_sequence_numbers(&glean, ac_seq_numbers);

        assert_eq!(3, ping_maker.get_ping_seq(&glean, "control"));
        // The next one should not have been migrated, so we expect
        // it to start from 0 instead of 85.
        assert_eq!(0, ping_maker.get_ping_seq(&glean, "ignored"));
    }

    #[test]
    fn invalid_sequence_numbers_must_not_be_migrated() {
        let (glean, _) = new_glean();

        let mut ac_seq_numbers = HashMap::new();
        ac_seq_numbers.insert(String::from("control_seq"), 3);
        ac_seq_numbers.insert(String::from("ignored_seq"), -85);

        let ping_maker = PingMaker::new();
        migrate_sequence_numbers(&glean, ac_seq_numbers);

        assert_eq!(3, ping_maker.get_ping_seq(&glean, "control"));
        // The next one should not have been migrated, so we expect
        // it to start from 0 instead of 85.
        assert_eq!(0, ping_maker.get_ping_seq(&glean, "ignored"));
    }

    #[test]
    fn valid_sequence_numbers_must_be_migrated() {
        let (glean, _) = new_glean();

        let mut ac_seq_numbers = HashMap::new();
        ac_seq_numbers.insert(String::from("custom_seq"), 3);
        ac_seq_numbers.insert(String::from("other_seq"), 7);
        ac_seq_numbers.insert(String::from("ignored_seq-lol"), 85);

        let ping_maker = PingMaker::new();
        migrate_sequence_numbers(&glean, ac_seq_numbers);

        assert_eq!(3, ping_maker.get_ping_seq(&glean, "custom"));
        assert_eq!(7, ping_maker.get_ping_seq(&glean, "other"));
        // The next one should not have been migrated, so we expect
        // it to start from 0 instead of 85.
        assert_eq!(0, ping_maker.get_ping_seq(&glean, "ignored"));
    }
}
