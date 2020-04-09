/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::sync::Arc;

use nsstring::nsCString;
use storage_variant::VariantType;
use xpcom::{interfaces::nsIVariant, RefPtr};

/// An operation that runs on the background thread, and optionally passes a
/// result to its callback.
pub enum Ferry<S> {
    Initialize,
    LastSync,
    SetLastSync(i64),
    SyncId,
    ResetSyncId,
    EnsureCurrentSyncId(String),
    StoreIncoming(Vec<String>, Arc<S>),
    SetUploaded(i64, Vec<String>, Arc<S>),
    SyncFinished(Arc<S>),
    Reset,
    Wipe,
    Finalize,
}

impl<S> Ferry<S> {
    /// Returns the operation name for debugging and labeling the task
    /// runnable.
    pub fn name(&self) -> &'static str {
        match self {
            Ferry::Initialize => concat!(module_path!(), "initialize"),
            Ferry::LastSync => concat!(module_path!(), "getLastSync"),
            Ferry::SetLastSync(_) => concat!(module_path!(), "setLastSync"),
            Ferry::SyncId => concat!(module_path!(), "getSyncId"),
            Ferry::ResetSyncId => concat!(module_path!(), "resetSyncId"),
            Ferry::EnsureCurrentSyncId(_) => concat!(module_path!(), "ensureCurrentSyncId"),
            Ferry::StoreIncoming { .. } => concat!(module_path!(), "storeIncoming"),
            Ferry::SetUploaded { .. } => concat!(module_path!(), "setUploaded"),
            Ferry::SyncFinished(_) => concat!(module_path!(), "sync"),
            Ferry::Reset => concat!(module_path!(), "reset"),
            Ferry::Wipe => concat!(module_path!(), "wipe"),
            Ferry::Finalize => concat!(module_path!(), "finalize"),
        }
    }
}

/// The result of a ferry task, sent from the background thread back to the
/// main thread. Results are converted to variants, and passed as arguments to
/// `mozIBridgedSyncEngineCallback`s.
pub enum FerryResult {
    LastSync(i64),
    SyncId(Option<String>),
    AssignedSyncId(String),
    Null,
}

impl From<()> for FerryResult {
    fn from(_: ()) -> FerryResult {
        FerryResult::Null
    }
}

impl FerryResult {
    /// Converts the result to an `nsIVariant` that can be passed as an
    /// argument to `callback.handleResult()`.
    pub fn into_variant(self) -> RefPtr<nsIVariant> {
        match self {
            FerryResult::LastSync(v) => v.into_variant(),
            FerryResult::SyncId(Some(v)) => nsCString::from(v).into_variant(),
            FerryResult::SyncId(None) => ().into_variant(),
            FerryResult::AssignedSyncId(v) => nsCString::from(v).into_variant(),
            FerryResult::Null => ().into_variant(),
        }
    }
}
