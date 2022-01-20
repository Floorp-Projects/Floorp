// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::common_metric_data::CommonMetricData;
use crate::error_recording::{record_error, ErrorType};
use crate::metrics::{Metric, MetricType};
use crate::Glean;

const MAX_LABELS: usize = 16;
const OTHER_LABEL: &str = "__other__";
const MAX_LABEL_LENGTH: usize = 61;

/// Checks whether the given label is sane.
///
/// The check corresponds to the following regular expression:
///
/// ```text
/// ^[a-z_][a-z0-9_-]{0,29}(\\.[a-z_][a-z0-9_-]{0,29})*$
/// ```
///
/// We do a manul check here instead of using a regex,
/// because the regex crate adds to the binary size significantly,
/// and the Glean SDK doesn't use regular expressions anywhere else.
///
/// Some examples of good and bad labels:
///
/// Good:
/// * `this.is.fine`
/// * `this_is_fine_too`
/// * `this.is_still_fine`
/// * `thisisfine`
/// * `_.is_fine`
/// * `this.is-fine`
/// * `this-is-fine`
/// Bad:
/// * `this.is.not_fine_due_tu_the_length_being_too_long_i_thing.i.guess`
/// * `1.not_fine`
/// * `this.$isnotfine`
/// * `-.not_fine`
fn matches_label_regex(value: &str) -> bool {
    let mut iter = value.chars();

    loop {
        // Match the first letter in the word.
        match iter.next() {
            Some('_') | Some('a'..='z') => (),
            _ => return false,
        };

        // Match subsequent letters in the word.
        let mut count = 0;
        loop {
            match iter.next() {
                // We are done, so the whole expression is valid.
                None => return true,
                // Valid characters.
                Some('_') | Some('-') | Some('a'..='z') | Some('0'..='9') => (),
                // We ended a word, so iterate over the outer loop again.
                Some('.') => break,
                // An invalid character
                _ => return false,
            }
            count += 1;
            // We allow 30 characters per word, but the first one is handled
            // above outside of this loop, so we have a maximum of 29 here.
            if count == 29 {
                return false;
            }
        }
    }
}

/// A labeled metric.
///
/// Labeled metrics allow to record multiple sub-metrics of the same type under different string labels.
#[derive(Clone, Debug)]
pub struct LabeledMetric<T> {
    labels: Option<Vec<String>>,
    /// Type of the underlying metric
    /// We hold on to an instance of it, which is cloned to create new modified instances.
    submetric: T,
}

impl<T> LabeledMetric<T>
where
    T: MetricType + Clone,
{
    /// Creates a new labeled metric from the given metric instance and optional list of labels.
    ///
    /// See [`get`](LabeledMetric::get) for information on how static or dynamic labels are handled.
    pub fn new(submetric: T, labels: Option<Vec<String>>) -> LabeledMetric<T> {
        LabeledMetric { labels, submetric }
    }

    /// Creates a new metric with a specific label.
    ///
    /// This is used for static labels where we can just set the name to be `name/label`.
    fn new_metric_with_name(&self, name: String) -> T {
        let mut t = self.submetric.clone();
        t.meta_mut().name = name;
        t
    }

    /// Creates a new metric with a specific label.
    ///
    /// This is used for dynamic labels where we have to actually validate and correct the
    /// label later when we have a Glean object.
    fn new_metric_with_dynamic_label(&self, label: String) -> T {
        let mut t = self.submetric.clone();
        t.meta_mut().dynamic_label = Some(label);
        t
    }

    /// Creates a static label.
    ///
    /// # Safety
    ///
    /// Should only be called when static labels are available on this metric.
    ///
    /// # Arguments
    ///
    /// * `label` - The requested label
    ///
    /// # Returns
    ///
    /// The requested label if it is in the list of allowed labels.
    /// Otherwise `OTHER_LABEL` is returned.
    fn static_label<'a>(&self, label: &'a str) -> &'a str {
        debug_assert!(self.labels.is_some());
        let labels = self.labels.as_ref().unwrap();
        if labels.iter().any(|l| l == label) {
            label
        } else {
            OTHER_LABEL
        }
    }

    /// Gets a specific metric for a given label.
    ///
    /// If a set of acceptable labels were specified in the `metrics.yaml` file,
    /// and the given label is not in the set, it will be recorded under the special `OTHER_LABEL` label.
    ///
    /// If a set of acceptable labels was not specified in the `metrics.yaml` file,
    /// only the first 16 unique labels will be used.
    /// After that, any additional labels will be recorded under the special `OTHER_LABEL` label.
    ///
    /// Labels must be `snake_case` and less than 30 characters.
    /// If an invalid label is used, the metric will be recorded in the special `OTHER_LABEL` label.
    pub fn get(&self, label: &str) -> T {
        // We have 2 scenarios to consider:
        // * Static labels. No database access needed. We just look at what is in memory.
        // * Dynamic labels. We look up in the database all previously stored
        //   labels in order to keep a maximum of allowed labels. This is done later
        //   when the specific metric is actually recorded, when we are guaranteed to have
        //   an initialized Glean object.
        match self.labels {
            Some(_) => {
                let label = self.static_label(label);
                self.new_metric_with_name(combine_base_identifier_and_label(
                    &self.submetric.meta().name,
                    label,
                ))
            }
            None => self.new_metric_with_dynamic_label(label.to_string()),
        }
    }

    /// Gets the template submetric.
    ///
    /// The template submetric is the actual metric that is cloned and modified
    /// to record for a specific label.
    pub fn get_submetric(&self) -> &T {
        &self.submetric
    }
}

/// Combines a metric's base identifier and label
pub fn combine_base_identifier_and_label(base_identifer: &str, label: &str) -> String {
    format!("{}/{}", base_identifer, label)
}

/// Strips the label off of a complete identifier
pub fn strip_label(identifier: &str) -> &str {
    // safe unwrap, first field of a split always valid
    identifier.splitn(2, '/').next().unwrap()
}

/// Validates a dynamic label, changing it to `OTHER_LABEL` if it's invalid.
///
/// Checks the requested label against limitations, such as the label length and allowed
/// characters.
///
/// # Arguments
///
/// * `label` - The requested label
///
/// # Returns
///
/// The entire identifier for the metric, including the base identifier and the corrected label.
/// The errors are logged.
pub fn validate_dynamic_label(
    glean: &Glean,
    meta: &CommonMetricData,
    base_identifier: &str,
    label: &str,
) -> String {
    let key = combine_base_identifier_and_label(base_identifier, label);
    for store in &meta.send_in_pings {
        if glean.storage().has_metric(meta.lifetime, store, &key) {
            return key;
        }
    }

    let mut label_count = 0;
    let prefix = &key[..=base_identifier.len()];
    let mut snapshotter = |_: &[u8], _: &Metric| {
        label_count += 1;
    };

    let lifetime = meta.lifetime;
    for store in &meta.send_in_pings {
        glean
            .storage()
            .iter_store_from(lifetime, store, Some(prefix), &mut snapshotter);
    }

    let error = if label_count >= MAX_LABELS {
        true
    } else if label.len() > MAX_LABEL_LENGTH {
        let msg = format!(
            "label length {} exceeds maximum of {}",
            label.len(),
            MAX_LABEL_LENGTH
        );
        record_error(glean, meta, ErrorType::InvalidLabel, msg, None);
        true
    } else if !matches_label_regex(label) {
        let msg = format!("label must be snake_case, got '{}'", label);
        record_error(glean, meta, ErrorType::InvalidLabel, msg, None);
        true
    } else {
        false
    };

    if error {
        combine_base_identifier_and_label(base_identifier, OTHER_LABEL)
    } else {
        key
    }
}
