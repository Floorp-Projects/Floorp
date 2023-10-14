/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde_derive::*;

use super::Command;

/// The serialized form of a client record.
#[derive(Clone, Debug, Eq, Deserialize, Hash, PartialEq, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ClientRecord {
    #[serde(rename = "id")]
    pub id: String,

    pub name: String,

    #[serde(rename = "type")]
    pub typ: crate::DeviceType,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub commands: Vec<CommandRecord>,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub fxa_device_id: Option<String>,

    /// `version`, `protocols`, `formfactor`, `os`, `appPackage`, `application`,
    /// and `device` are unused and optional in all implementations (Desktop,
    /// iOS, and Fennec), but we round-trip them.

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub version: Option<String>,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub protocols: Vec<String>,

    #[serde(
        default,
        rename = "formfactor",
        skip_serializing_if = "Option::is_none"
    )]
    pub form_factor: Option<String>,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub os: Option<String>,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub app_package: Option<String>,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub application: Option<String>,

    /// The model of the device, like "iPhone" or "iPod touch" on iOS. Note
    /// that this is _not_ the client ID (`id`) or the FxA device ID
    /// (`fxa_device_id`).
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub device: Option<String>,
}

impl From<&ClientRecord> for crate::RemoteClient {
    fn from(record: &ClientRecord) -> crate::RemoteClient {
        crate::RemoteClient {
            fxa_device_id: record.fxa_device_id.clone(),
            device_name: record.name.clone(),
            device_type: record.typ,
        }
    }
}

/// The serialized form of a client command.
#[derive(Clone, Debug, Eq, Deserialize, Hash, PartialEq, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CommandRecord {
    /// The command name. This is a string, not an enum, because we want to
    /// round-trip commands that we don't support yet.
    #[serde(rename = "command")]
    pub name: String,

    /// Extra, command-specific arguments. Note that we must send an empty
    /// array if the command expects no arguments.
    #[serde(default)]
    pub args: Vec<Option<String>>,

    /// Some commands, like repair, send a "flow ID" that other cliennts can
    /// record in their telemetry. We don't currently send commands with
    /// flow IDs, but we round-trip them.
    #[serde(default, rename = "flowID", skip_serializing_if = "Option::is_none")]
    pub flow_id: Option<String>,
}

impl CommandRecord {
    // In an ideal future we'd treat non-string args as "soft errors" rather than a hard
    // serde failure, but there's no evidence we actually see these. There *is* evidence of
    // seeing nulls instead of strings though (presumably due to old sendTab commands), so we
    // do handle that.
    fn get_single_string_arg(&self) -> Option<String> {
        let cmd_name = &self.name;
        if self.args.len() == 1 {
            match &self.args[0] {
                Some(name) => Some(name.into()),
                None => {
                    log::error!("Incoming '{cmd_name}' command has null argument");
                    None
                }
            }
        } else {
            log::error!(
                "Incoming '{cmd_name}' command has wrong number of arguments ({})",
                self.args.len()
            );
            None
        }
    }

    /// Converts a serialized command into one that we can apply. Returns `None`
    /// if we don't support the command.
    pub fn as_command(&self) -> Option<Command> {
        match self.name.as_str() {
            "wipeEngine" => self.get_single_string_arg().map(Command::Wipe),
            "resetEngine" => self.get_single_string_arg().map(Command::Reset),
            "resetAll" => {
                if self.args.is_empty() {
                    Some(Command::ResetAll)
                } else {
                    log::error!("Invalid arguments for 'resetAll' command");
                    None
                }
            }
            // Note callers are expected to log on an unknown command.
            _ => None,
        }
    }
}

impl From<Command> for CommandRecord {
    fn from(command: Command) -> CommandRecord {
        match command {
            Command::Wipe(engine) => CommandRecord {
                name: "wipeEngine".into(),
                args: vec![Some(engine)],
                flow_id: None,
            },
            Command::Reset(engine) => CommandRecord {
                name: "resetEngine".into(),
                args: vec![Some(engine)],
                flow_id: None,
            },
            Command::ResetAll => CommandRecord {
                name: "resetAll".into(),
                args: Vec::new(),
                flow_id: None,
            },
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_valid_commands() {
        let ser = serde_json::json!({"command": "wipeEngine", "args": ["foo"]});
        let record: CommandRecord = serde_json::from_value(ser).unwrap();
        assert_eq!(record.as_command(), Some(Command::Wipe("foo".to_string())));

        let ser = serde_json::json!({"command": "resetEngine", "args": ["foo"]});
        let record: CommandRecord = serde_json::from_value(ser).unwrap();
        assert_eq!(record.as_command(), Some(Command::Reset("foo".to_string())));

        let ser = serde_json::json!({"command": "resetAll"});
        let record: CommandRecord = serde_json::from_value(ser).unwrap();
        assert_eq!(record.as_command(), Some(Command::ResetAll));
    }

    #[test]
    fn test_unknown_command() {
        let ser = serde_json::json!({"command": "unknown", "args": ["foo", "bar"]});
        let record: CommandRecord = serde_json::from_value(ser).unwrap();
        assert_eq!(record.as_command(), None);
    }

    #[test]
    fn test_bad_args() {
        let ser = serde_json::json!({"command": "wipeEngine", "args": ["foo", "bar"]});
        let record: CommandRecord = serde_json::from_value(ser).unwrap();
        assert_eq!(record.as_command(), None);

        let ser = serde_json::json!({"command": "wipeEngine"});
        let record: CommandRecord = serde_json::from_value(ser).unwrap();
        assert_eq!(record.as_command(), None);

        let ser = serde_json::json!({"command": "resetAll", "args": ["foo"]});
        let record: CommandRecord = serde_json::from_value(ser).unwrap();
        assert_eq!(record.as_command(), None);
    }

    #[test]
    fn test_null_args() {
        let ser = serde_json::json!({"command": "unknown", "args": ["foo", null]});
        let record: CommandRecord = serde_json::from_value(ser).unwrap();
        assert_eq!(record.as_command(), None);
    }
}
