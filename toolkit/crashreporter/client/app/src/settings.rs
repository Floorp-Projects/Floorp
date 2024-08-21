/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Persistent settings of the application.

#[derive(Debug, serde::Serialize, serde::Deserialize)]
pub struct Settings {
    /// Whether a crash report should be sent.
    pub submit_report: bool,
    /// Whether the URL that was open should be included in a sent report.
    pub include_url: bool,
}

impl Default for Settings {
    fn default() -> Self {
        Settings {
            submit_report: true,
            include_url: true,
        }
    }
}

impl Settings {
    /// Write the settings to the given writer.
    pub fn to_writer<W: std::io::Write>(&self, writer: W) -> anyhow::Result<()> {
        Ok(serde_json::to_writer_pretty(writer, self)?)
    }

    /// Read the settings from the given reader.
    pub fn from_reader<R: std::io::Read>(reader: R) -> anyhow::Result<Self> {
        Ok(serde_json::from_reader(reader)?)
    }

    #[cfg(test)]
    pub fn to_string(&self) -> String {
        serde_json::to_string_pretty(self).unwrap()
    }
}
