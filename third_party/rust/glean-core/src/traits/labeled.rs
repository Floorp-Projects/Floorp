// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::metrics::MetricType;

/// A description for the `LabeledMetric` type.
///
/// When changing this trait, make sure all the operations are
/// implemented in the related type in `../metrics/`.
pub trait Labeled<T>
where
    T: MetricType + Clone,
{
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
    fn get(&self, label: &str) -> T;

    /// Gets the template submetric.
    ///
    /// The template submetric is the actual metric that is cloned and modified
    /// to record for a specific label.
    fn get_submetric(&self) -> &T;
}
