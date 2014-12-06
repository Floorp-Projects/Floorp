use std::collections::TreeMap;
use serialize::json;
use serialize::{Encodable};
use serialize::json::{ToJson};
use regex::Captures;

use common::{WebDriverResult, WebDriverError, ErrorStatus};
use messagebuilder::MatchType;


#[deriving(PartialEq)]
pub enum WebDriverCommand {
    GetMarionetteId, //TODO: move this
    NewSession,
    DeleteSession,
    Get(GetParameters),
    GetCurrentUrl,
    GoBack,
    GoForward,
    Refresh,
    GetTitle,
    GetWindowHandle,
    GetWindowHandles,
    Close,
    Timeouts(TimeoutsParameters)
}

#[deriving(PartialEq)]
pub struct WebDriverMessage {
    pub session_id: Option<String>,
    pub command: WebDriverCommand
}

impl WebDriverMessage {
    pub fn new(session_id: Option<String>, command: WebDriverCommand) -> WebDriverMessage {
        WebDriverMessage {
            session_id: session_id,
            command: command
        }
    }

    pub fn from_http(match_type: MatchType, params: &Captures, body: &str) -> WebDriverResult<WebDriverMessage> {
        let session_id = WebDriverMessage::get_session_id(params);
        let body_data = match json::from_str(body) {
            Ok(x) => x,
            Err(_) => return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                     "Failed to decode request body"))
        };
        let command = match match_type {
            MatchType::NewSession => WebDriverCommand::NewSession,
            MatchType::DeleteSession => WebDriverCommand::DeleteSession,
            MatchType::Get => {
                match GetParameters::from_json(&body_data) {
                    Ok(parameters) => {
                        WebDriverCommand::Get(parameters)
                    },
                    Err(_) => return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                             "Failed to decode request body"))
                }
            },
            MatchType::GetCurrentUrl => WebDriverCommand::GetCurrentUrl,
            MatchType::GoBack => WebDriverCommand::GoBack,
            MatchType::GoForward => WebDriverCommand::GoForward,
            MatchType::Refresh => WebDriverCommand::Refresh,
            MatchType::GetTitle => WebDriverCommand::GetTitle,
            MatchType::GetWindowHandle => WebDriverCommand::GetWindowHandle,
            MatchType::GetWindowHandles => WebDriverCommand::GetWindowHandles,
            MatchType::Close => WebDriverCommand::Close,
            MatchType::Timeouts => {
                let parameters_result = TimeoutsParameters::from_json(&body_data);
                match parameters_result {
                    Ok(parameters) => WebDriverCommand::Timeouts(parameters),
                    Err(_) => return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                             "Failed to decode request body"))
                }
            }
        };
        Ok(WebDriverMessage::new(session_id, command))
    }

    fn get_session_id(params: &Captures) -> Option<String> {
        let session_id_str = params.name("sessionId");
        if session_id_str == "" {
            None
        } else {
            Some(session_id_str.to_string())
        }
    }
}

impl ToJson for WebDriverMessage {
    fn to_json(&self) -> json::Json {
        match self.command {
            WebDriverCommand::Get(ref x) => {
                x.to_json()
            },
            WebDriverCommand::Timeouts(ref x) => {
                x.to_json()
            }
            _ => {
                json::Object(TreeMap::new())
            }
        }
    }
}

#[deriving(PartialEq)]
struct GetParameters {
    url: String
}

impl GetParameters {
    pub fn from_json(body: &json::Json) -> Result<GetParameters, String> {
        return Ok(GetParameters {
            url: body.find("url").unwrap().as_string().unwrap().to_string()
        })
    }
}

impl ToJson for GetParameters {
    fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        data.insert("url".to_string(), self.url.to_json());
        json::Object(data)
    }
}

#[deriving(PartialEq)]
struct TimeoutsParameters {
    type_: String,
    ms: u32
}

impl TimeoutsParameters {
    pub fn from_json(body: &json::Json) -> Result<TimeoutsParameters, String> {
        return Ok(TimeoutsParameters {
            type_: body.find("type").unwrap().as_string().unwrap().to_string(),
            ms: body.find("ms").unwrap().as_i64().unwrap() as u32
        })
    }
}

impl ToJson for TimeoutsParameters {
    fn to_json(&self) -> json::Json {
        let mut data = TreeMap::new();
        data.insert("type".to_string(), self.type_.to_json());
        data.insert("ms".to_string(), self.ms.to_json());
        json::Object(data)
    }
}
