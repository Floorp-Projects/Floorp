use serialize::json::ToJson;
use serialize::json;
use std::io::{IoResult, TcpStream, IoError};
use std::collections::TreeMap;

use command::{WebDriverMessage, WebDriverCommand};
use command::WebDriverCommand::{GetMarionetteId, NewSession, DeleteSession, Get, GetCurrentUrl,
                                GoBack, GoForward, Refresh, GetTitle, GetWindowHandle,
                                GetWindowHandles, Close, Timeouts};
use response::WebDriverResponse;
use common::{WebDriverResult, WebDriverError, ErrorStatus};

pub struct MarionetteSession {
    pub session_id: String,
    pub to: String
}

impl MarionetteSession {
    pub fn new() -> MarionetteSession {
        MarionetteSession {
            session_id: "".to_string(),
            to: String::from_str("root")
        }
    }

    pub fn update(&mut self, msg: &WebDriverMessage, resp: &TreeMap<String, json::Json>) -> WebDriverResult<()> {
        match msg.command {
            GetMarionetteId => {
                let to = match resp.get(&"to".to_string()) {
                    Some(x) => x,
                    None => return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                           "Unable to get to value"))
                };
                self.to = to.to_string().clone();
            },
            NewSession => {
                let session_id = match resp.get(&"value".to_string()) {
                    Some(x) => x,
                    None => return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                           "Unable to get session id"))
                };
                self.session_id = session_id.to_string().clone();
            }
            _ => {}
        }
        Ok(())
    }

    fn command_name(msg:&WebDriverMessage) -> String {
        match msg.command {
            GetMarionetteId => "getMarionetteID",
            NewSession => "newSession",
            DeleteSession => "deleteSession",
            Get(_) => "get",
            GetCurrentUrl => "getCurrentUrl",
            GoBack => "goBack",
            GoForward => "goForward",
            Refresh => "refresh",
            GetTitle => "getTitle",
            GetWindowHandle => "getWindowHandle",
            GetWindowHandles => "getWindowHandles",
            Close => "close",
            Timeouts(_) => "timeouts"
        }.to_string()
    }

    pub fn msg_to_marionette(&self, msg: &WebDriverMessage) -> json::Json {
        let mut data = msg.to_json().as_object().unwrap().clone();
        match msg.session_id {
            Some(ref x) => data.insert("sessionId".to_string(), x.to_json()),
            None => None
        };
        data.insert("to".to_string(), self.to.to_json());
        data.insert("command".to_string(), MarionetteSession::command_name(msg).to_json());
        json::Object(data)
    }

    pub fn response_from_json(&mut self, message: &WebDriverMessage,
                              data: &str) -> Option<WebDriverResult<WebDriverResponse>> {
        let decoded = match json::from_str(data) {
            Ok(data) => data,
            Err(_) => {
                return Some(Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                    "Failed to decode marionette data as json")));
            }
        };
        let json_data = match decoded {
            json::Object(x) => x,
            _ => {
                return Some(Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                    "Expected a json object")));
            }
        };
        if json_data.contains_key(&"error".to_string()) {
            //TODO: convert the marionette error into the right webdriver error
            let err_msg = match json_data.get(&"error".to_string()).unwrap().as_string() {
                Some(x) => x,
                None => "Unexpected error"
            };
            return Some(Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                err_msg)));
        }

        self.update(message, &json_data);

        match message.command {
            //Everything that doesn't have a response value
            GetMarionetteId => None,
            Get(_) | GoBack | GoForward | Refresh | Close | Timeouts(_) => {
                Some(Ok(WebDriverResponse::new(json::Null)))
            },
            //Things that simply return the contents of the marionette "value" property
            GetCurrentUrl | GetTitle | GetWindowHandle | GetWindowHandles => {
                let value = match json_data.get(&"value".to_string()) {
                    Some(data) => data,
                    None => {
                        return Some(Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                            "Failed to find value field")));
                    }
                };
                Some(Ok(WebDriverResponse::new(value.clone())))
            },
            NewSession => {
                let value = match json_data.get(&"value".to_string()) {
                    Some(data) => data,
                    None => {
                        return Some(Err(WebDriverError::new(ErrorStatus::UnknownError,
                                                            "Failed to find value field")));
                    }
                };
                Some(Ok(WebDriverResponse::new(value.clone())))
            }
            DeleteSession => {
                Some(Ok(WebDriverResponse::new(json::Null)))
            }
        }
    }
}

pub struct MarionetteConnection {
    stream: IoResult<TcpStream>,
    pub session: MarionetteSession
}

impl MarionetteConnection {
    pub fn new() -> MarionetteConnection {
        let stream = TcpStream::connect("127.0.0.1:2828");
        MarionetteConnection {
            stream: stream,
            session: MarionetteSession::new()
        }
    }

    pub fn connect(&mut self) -> Result<(), IoError> {
        try!(self.read_resp());
        //Would get traits and application type here
        let mut msg = TreeMap::new();
        msg.insert("name".to_string(), "getMarionetteId".to_json());
        msg.insert("to".to_string(), "root".to_json());
        match self.send(&msg.to_json()) {
            Ok(_) => Ok(()),
            Err(_) => panic!("Failed to connect to marionette")
        }
    }

    fn encode_msg(&self, msg:&json::Json) -> String {
        let data = json::encode(msg);
        let len = data.len().to_string();
        let mut message = len;
        message.push_str(":");
        message.push_str(data.as_slice());
        message
    }

    pub fn send_message(&mut self, msg: &WebDriverMessage) -> Option<WebDriverResult<WebDriverResponse>> {
        let resp = {
            self.session.msg_to_marionette(msg)
        };
        let resp = match self.send(&resp) {
            Ok(resp_data) => self.session.response_from_json(msg, resp_data[]),
            Err(x) => Some(Err(x))
        };
        resp
    }

    fn send(&mut self, msg: &json::Json) -> WebDriverResult<String> {
        let data = self.encode_msg(msg);
        println!("{}", data);
        match self.stream.write_str(data.as_slice()) {
            Ok(_) => {},
            Err(_) => {
                return Err(WebDriverError::new(ErrorStatus::UnknownError,
                                               "Failed to write response to stream"))
            }
        }
        match self.read_resp() {
            Ok(resp) => {
                Ok(resp)
            },
            Err(_) => Err(WebDriverError::new(ErrorStatus::UnknownError,
                                              "Failed to decode response from marionette"))
        }
    }

    fn read_resp(&mut self) -> Result<String, IoError> {
        let mut bytes = 0 as uint;
        loop {
            let byte = try!(self.stream.read_byte()) as char;
            match byte {
                '0'...'9' => {
                    bytes = bytes * 10;
                    bytes += byte as uint - '0' as uint;
                },
                ':' => {
                    break
                }
                _ => {}
            }
        }
        let data = try!(self.stream.read_exact(bytes));
        //Need to handle the error here
        Ok(String::from_utf8(data).unwrap())
    }

}
