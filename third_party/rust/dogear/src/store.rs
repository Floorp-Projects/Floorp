// Copyright 2018-2019 Mozilla

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use std::{time::Duration, time::Instant};

use crate::driver::{
    AbortSignal, DefaultAbortSignal, DefaultDriver, Driver, TelemetryEvent, TreeStats,
};
use crate::error::Error;
use crate::merge::{MergedRoot, Merger};
use crate::tree::Tree;

/// A store is the main interface to Dogear. It implements methods for building
/// local and remote trees from a storage backend, fetching content info for
/// matching items with similar contents, and persisting the merged tree.
pub trait Store<E: From<Error>> {
    /// Builds a fully rooted, consistent tree from the items and tombstones in
    /// the local store.
    fn fetch_local_tree(&self) -> Result<Tree, E>;

    /// Builds a fully rooted, consistent tree from the items and tombstones in
    /// the mirror.
    fn fetch_remote_tree(&self) -> Result<Tree, E>;

    /// Applies the merged root to the local store, and stages items for
    /// upload. On Desktop, this method inserts the merged tree into a temp
    /// table, updates Places, and inserts outgoing items into another
    /// temp table.
    fn apply<'t>(&mut self, root: MergedRoot<'t>) -> Result<(), E>;

    /// Builds and applies a merged tree using the default merge driver.
    fn merge(&mut self) -> Result<(), E> {
        self.merge_with_driver(&DefaultDriver, &DefaultAbortSignal)
    }

    /// Builds a complete merged tree from the local and remote trees, resolves
    /// conflicts, dedupes local items, and applies the merged tree using the
    /// given driver.
    fn merge_with_driver(
        &mut self,
        driver: &impl Driver,
        signal: &impl AbortSignal,
    ) -> Result<(), E> {
        signal.err_if_aborted()?;
        debug!(driver, "Building local tree");
        let (local_tree, time) = with_timing(|| self.fetch_local_tree())?;
        driver.record_telemetry_event(TelemetryEvent::FetchLocalTree(TreeStats {
            items: local_tree.size(),
            problems: local_tree.problems().counts(),
            time,
        }));
        trace!(driver, "Built local tree from mirror\n{}", local_tree);

        signal.err_if_aborted()?;
        debug!(driver, "Building remote tree");
        let (remote_tree, time) = with_timing(|| self.fetch_remote_tree())?;
        driver.record_telemetry_event(TelemetryEvent::FetchRemoteTree(TreeStats {
            items: remote_tree.size(),
            problems: remote_tree.problems().counts(),
            time,
        }));
        trace!(driver, "Built remote tree from mirror\n{}", remote_tree);

        signal.err_if_aborted()?;
        debug!(driver, "Building merged tree");
        let merger = Merger::with_driver(driver, signal, &local_tree, &remote_tree);
        let (merged_root, time) = with_timing(|| merger.merge())?;
        driver.record_telemetry_event(TelemetryEvent::Merge(time, *merged_root.counts()));
        trace!(
            driver,
            "Built new merged tree\n{}\nDelete Locally: [{}]\nDelete Remotely: [{}]",
            merged_root.node().to_ascii_string(),
            merged_root
                .local_deletions()
                .map(|d| d.guid.as_str())
                .collect::<Vec<_>>()
                .join(", "),
            merged_root
                .remote_deletions()
                .map(|d| d.guid.as_str())
                .collect::<Vec<_>>()
                .join(", ")
        );

        signal.err_if_aborted()?;
        debug!(driver, "Applying merged tree");
        let ((), time) = with_timing(|| self.apply(merged_root))?;
        driver.record_telemetry_event(TelemetryEvent::Apply(time));

        Ok(())
    }
}

fn with_timing<T, E>(run: impl FnOnce() -> Result<T, E>) -> Result<(T, Duration), E> {
    let now = Instant::now();
    run().map(|value| (value, now.elapsed()))
}
