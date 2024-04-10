// Reference:
//   https://learn.microsoft.com/en-us/windows-hardware/drivers/install/authenticode
//   https://download.microsoft.com/download/9/c/5/9c5b2167-8017-4bae-9fde-d599bac8184a/Authenticode_PE.docx

// Authenticode works by omiting sections of the PE binary from the digest
// those sections are:
//   - checksum
//   - data directory entry for certtable
//   - certtable

use core::ops::Range;

use super::PE;

impl PE<'_> {
    /// [`authenticode_ranges`] returns the various ranges of the binary that are relevant for
    /// signature.
    pub fn authenticode_ranges(&self) -> ExcludedSectionsIter<'_> {
        ExcludedSectionsIter {
            pe: self,
            state: IterState::default(),
        }
    }
}

/// [`ExcludedSections`] holds the various ranges of the binary that are expected to be
/// excluded from the authenticode computation.
#[derive(Debug, Clone, Default)]
pub(super) struct ExcludedSections {
    checksum: Range<usize>,
    datadir_entry_certtable: Range<usize>,
    certtable: Option<Range<usize>>,
}

impl ExcludedSections {
    pub(super) fn new(
        checksum: Range<usize>,
        datadir_entry_certtable: Range<usize>,
        certtable: Option<Range<usize>>,
    ) -> Self {
        Self {
            checksum,
            datadir_entry_certtable,
            certtable,
        }
    }
}

pub struct ExcludedSectionsIter<'s> {
    pe: &'s PE<'s>,
    state: IterState,
}

#[derive(Debug, PartialEq)]
enum IterState {
    Initial,
    DatadirEntry(usize),
    CertTable(usize),
    Final(usize),
    Done,
}

impl Default for IterState {
    fn default() -> Self {
        Self::Initial
    }
}

impl<'s> Iterator for ExcludedSectionsIter<'s> {
    type Item = &'s [u8];

    fn next(&mut self) -> Option<Self::Item> {
        let bytes = &self.pe.bytes;

        if let Some(sections) = self.pe.authenticode_excluded_sections.as_ref() {
            loop {
                match self.state {
                    IterState::Initial => {
                        self.state = IterState::DatadirEntry(sections.checksum.end);
                        return Some(&bytes[..sections.checksum.start]);
                    }
                    IterState::DatadirEntry(start) => {
                        self.state = IterState::CertTable(sections.datadir_entry_certtable.end);
                        return Some(&bytes[start..sections.datadir_entry_certtable.start]);
                    }
                    IterState::CertTable(start) => {
                        if let Some(certtable) = sections.certtable.as_ref() {
                            self.state = IterState::Final(certtable.end);
                            return Some(&bytes[start..certtable.start]);
                        } else {
                            self.state = IterState::Final(start)
                        }
                    }
                    IterState::Final(start) => {
                        self.state = IterState::Done;
                        return Some(&bytes[start..]);
                    }
                    IterState::Done => return None,
                }
            }
        } else {
            loop {
                match self.state {
                    IterState::Initial => {
                        self.state = IterState::Done;
                        return Some(bytes);
                    }
                    IterState::Done => return None,
                    _ => {
                        self.state = IterState::Done;
                    }
                }
            }
        }
    }
}
