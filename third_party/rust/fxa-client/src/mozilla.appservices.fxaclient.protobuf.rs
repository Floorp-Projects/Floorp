#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Profile {
    #[prost(string, optional, tag="1")]
    pub uid: ::std::option::Option<std::string::String>,
    #[prost(string, optional, tag="2")]
    pub email: ::std::option::Option<std::string::String>,
    #[prost(string, optional, tag="3")]
    pub avatar: ::std::option::Option<std::string::String>,
    #[prost(bool, optional, tag="4")]
    pub avatar_default: ::std::option::Option<bool>,
    #[prost(string, optional, tag="5")]
    pub display_name: ::std::option::Option<std::string::String>,
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct AccessTokenInfo {
    #[prost(string, required, tag="1")]
    pub scope: std::string::String,
    #[prost(string, required, tag="2")]
    pub token: std::string::String,
    #[prost(message, optional, tag="3")]
    pub key: ::std::option::Option<ScopedKey>,
    #[prost(uint64, required, tag="4")]
    pub expires_at: u64,
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct IntrospectInfo {
    #[prost(bool, required, tag="1")]
    pub active: bool,
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct ScopedKey {
    #[prost(string, required, tag="1")]
    pub kty: std::string::String,
    #[prost(string, required, tag="2")]
    pub scope: std::string::String,
    #[prost(string, required, tag="3")]
    pub k: std::string::String,
    #[prost(string, required, tag="4")]
    pub kid: std::string::String,
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Device {
    #[prost(string, required, tag="1")]
    pub id: std::string::String,
    #[prost(string, required, tag="2")]
    pub display_name: std::string::String,
    #[prost(enumeration="device::Type", required, tag="3")]
    pub r#type: i32,
    #[prost(message, optional, tag="4")]
    pub push_subscription: ::std::option::Option<device::PushSubscription>,
    #[prost(bool, required, tag="5")]
    pub push_endpoint_expired: bool,
    #[prost(bool, required, tag="6")]
    pub is_current_device: bool,
    #[prost(uint64, optional, tag="7")]
    pub last_access_time: ::std::option::Option<u64>,
    #[prost(enumeration="device::Capability", repeated, packed="false", tag="8")]
    pub capabilities: ::std::vec::Vec<i32>,
}
pub mod device {
    #[derive(Clone, PartialEq, ::prost::Message)]
    pub struct PushSubscription {
        #[prost(string, required, tag="1")]
        pub endpoint: std::string::String,
        #[prost(string, required, tag="2")]
        pub public_key: std::string::String,
        #[prost(string, required, tag="3")]
        pub auth_key: std::string::String,
    }
    #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, ::prost::Enumeration)]
    #[repr(i32)]
    pub enum Capability {
        SendTab = 1,
    }
    #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, ::prost::Enumeration)]
    #[repr(i32)]
    pub enum Type {
        Desktop = 1,
        Mobile = 2,
        Tablet = 3,
        Vr = 4,
        Tv = 5,
        Unknown = 6,
    }
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Devices {
    #[prost(message, repeated, tag="1")]
    pub devices: ::std::vec::Vec<Device>,
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Capabilities {
    #[prost(enumeration="device::Capability", repeated, packed="false", tag="1")]
    pub capability: ::std::vec::Vec<i32>,
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct IncomingDeviceCommand {
    #[prost(enumeration="incoming_device_command::IncomingDeviceCommandType", required, tag="1")]
    pub r#type: i32,
    #[prost(oneof="incoming_device_command::Data", tags="2")]
    pub data: ::std::option::Option<incoming_device_command::Data>,
}
pub mod incoming_device_command {
    #[derive(Clone, PartialEq, ::prost::Message)]
    pub struct SendTabData {
        #[prost(message, optional, tag="1")]
        pub from: ::std::option::Option<super::Device>,
        #[prost(message, repeated, tag="2")]
        pub entries: ::std::vec::Vec<send_tab_data::TabHistoryEntry>,
    }
    pub mod send_tab_data {
        #[derive(Clone, PartialEq, ::prost::Message)]
        pub struct TabHistoryEntry {
            #[prost(string, required, tag="1")]
            pub title: std::string::String,
            #[prost(string, required, tag="2")]
            pub url: std::string::String,
        }
    }
    #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, ::prost::Enumeration)]
    #[repr(i32)]
    pub enum IncomingDeviceCommandType {
        /// `data` set to `tab_received_data`.
        TabReceived = 1,
    }
    #[derive(Clone, PartialEq, ::prost::Oneof)]
    pub enum Data {
        #[prost(message, tag="2")]
        TabReceivedData(SendTabData),
    }
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct IncomingDeviceCommands {
    #[prost(message, repeated, tag="1")]
    pub commands: ::std::vec::Vec<IncomingDeviceCommand>,
}
/// This is basically an enum with associated values,
/// but it's a bit harder to model in proto2.
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct AccountEvent {
    #[prost(enumeration="account_event::AccountEventType", required, tag="1")]
    pub r#type: i32,
    #[prost(oneof="account_event::Data", tags="2, 3, 4")]
    pub data: ::std::option::Option<account_event::Data>,
}
pub mod account_event {
    #[derive(Clone, PartialEq, ::prost::Message)]
    pub struct DeviceDisconnectedData {
        #[prost(string, required, tag="1")]
        pub device_id: std::string::String,
        #[prost(bool, required, tag="2")]
        pub is_local_device: bool,
    }
    #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, ::prost::Enumeration)]
    #[repr(i32)]
    pub enum AccountEventType {
        /// `data` set to `device_command`.
        IncomingDeviceCommand = 1,
        ProfileUpdated = 2,
        /// `data` set to `device_connected_name`.
        DeviceConnected = 3,
        AccountAuthStateChanged = 4,
        /// `data` set to `device_disconnected_data`.
        DeviceDisconnected = 5,
        AccountDestroyed = 6,
    }
    #[derive(Clone, PartialEq, ::prost::Oneof)]
    pub enum Data {
        #[prost(message, tag="2")]
        DeviceCommand(super::IncomingDeviceCommand),
        #[prost(string, tag="3")]
        DeviceConnectedName(std::string::String),
        #[prost(message, tag="4")]
        DeviceDisconnectedData(DeviceDisconnectedData),
    }
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct AccountEvents {
    #[prost(message, repeated, tag="1")]
    pub events: ::std::vec::Vec<AccountEvent>,
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct AuthorizationPkceParams {
    #[prost(string, required, tag="1")]
    pub code_challenge: std::string::String,
    #[prost(string, required, tag="2")]
    pub code_challenge_method: std::string::String,
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct AuthorizationParams {
    #[prost(string, required, tag="1")]
    pub client_id: std::string::String,
    #[prost(string, required, tag="2")]
    pub scope: std::string::String,
    #[prost(string, required, tag="3")]
    pub state: std::string::String,
    #[prost(string, required, tag="4")]
    pub access_type: std::string::String,
    #[prost(message, optional, tag="5")]
    pub pkce_params: ::std::option::Option<AuthorizationPkceParams>,
    #[prost(string, optional, tag="6")]
    pub keys_jwk: ::std::option::Option<std::string::String>,
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct MetricsParams {
    #[prost(map="string, string", tag="1")]
    pub parameters: ::std::collections::HashMap<std::string::String, std::string::String>,
}
