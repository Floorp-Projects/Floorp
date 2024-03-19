/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use thiserror::Error;

#[derive(Debug, Error)]
pub enum AnnotationsRetrievalError {
    #[error("Address was out of bounds")]
    InvalidAddress,
    #[error("Corrupt or wrong annotation table")]
    InvalidAnnotationTable,
    #[error("The data read from the target process is invalid")]
    InvalidData,
    #[error("Could not execute operation on the target process")]
    ProcessReaderError(#[from] process_reader::error::ProcessReaderError),
}
