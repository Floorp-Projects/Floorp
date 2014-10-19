use serialize::json::ToJson;
use serialize::json;
use std::collections::TreeMap;
use std::io::{IoResult, TcpStream, IoError};

use uuid::Uuid;

use command::{WebDriverMessage, GetMarionetteId, NewSession};
use response::WebDriverResponse;
use common::{WebDriverResult, WebDriverError, UnknownError};

pub struct MarionetteSession {
    pub session_id: String,
    pub to: String,
    pub marionette_session_id: Option<json::Json>
}

impl MarionetteSession {
    pub fn new() -> MarionetteSession {
        MarionetteSession {
            session_id: Uuid::new_v4().to_string(),
            to: String::from_str("root"),
            marionette_session_id: None
        }
    }

    pub fn id(&self) -> String {
        self.session_id.clone()
    }

    pub fn update(&mut self, msg: &WebDriverMessage, from: &json::Json, session_id: &json::Json) {
        match msg.command {
            GetMarionetteId => {
                self.to = from.to_string().clone();
            },
            NewSession =>  {
                self.marionette_session_id = Some(session_id.clone());
            }
            _ => {}
        }
    }

    fn id_to_marionette(&self, msg: &WebDriverMessage) -> Option<json::Json> {
        match msg.command {
            // Clean up these fails! to return the right error instead
            GetMarionetteId | NewSession => {
                match msg.session_id {
                    Some(_) => fail!("Tried to start session but session was already started"),
                    None => {}
                }
            },
            _ => {
                match msg.session_id {
                    Some(ref x) if *x != self.session_id => {
                        fail!("Invalid session id");
                    },
                    None => {
                        fail!("Session id not supplied");
                    }
                    _ => {}
                }
            }
        }
        match msg.command {
            GetMarionetteId => None,
            _ => Some(match self.marionette_session_id {
                Some(ref x) => x.clone(),
                None => json::Null
            })
        }

    }

    pub fn msg_to_json(&self, msg: &WebDriverMessage) -> json::Json {
        let mut data = msg.to_json().as_object().unwrap().clone();
        let session_id = self.id_to_marionette(msg);
        if session_id.is_some() {
            data.insert("sessionId".to_string(), session_id.unwrap());
        }
        data.insert("to".to_string(), self.to.to_json());
        json::Object(data)
    }
}

pub struct MarionetteConnection {
    stream: IoResult<TcpStream>,
    session: MarionetteSession
}

impl MarionetteConnection {
    pub fn new() -> MarionetteConnection {
        let stream = TcpStream::connect("127.0.0.1", 2828);
        MarionetteConnection {
            stream: stream,
            session: MarionetteSession::new()
        }
    }

    pub fn connect(&mut self) -> Result<(), IoError> {
        try!(self.read_resp());
        //Would get traits and application type here
        self.send_message(&WebDriverMessage::new(GetMarionetteId, None));
        Ok(())
    }

    fn encode_msg(&self, msg: &WebDriverMessage) -> String {
        let data = format!("{}", self.session.msg_to_json(msg));
        let len = data.len().to_string();
        let mut message = len;
        message.push_str(":");
        message.push_str(data.as_slice());
        message
    }

    pub fn send_message(&mut self, msg: &WebDriverMessage) -> WebDriverResult<Option<WebDriverResponse>> {
        let data = self.encode_msg(msg);
        println!("{}", data);
        match self.stream.write_str(data.as_slice()) {
            Ok(_) => {},
            Err(_) => {
                return Err(WebDriverError::new(Some(self.session.session_id.clone()),
                                               UnknownError,
                                               "Failed to write response to stream"))
            }
        }
        match self.read_resp() {
            Ok(resp) => {
                println!("{}", resp);
                Ok(WebDriverResponse::from_json(&mut self.session, msg, resp.as_slice()))
            },
            Err(_) => {
                Err(WebDriverError::new(Some(self.session.id()),
                                        UnknownError,
                                        "Failed to decode response from marionette"))
            }
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
