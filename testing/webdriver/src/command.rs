use regex::Captures;
use rustc_serialize::json::{ToJson, Json};
use std::collections::BTreeMap;

use common::{Date, Nullable, WebElement, FrameId, LocatorStrategy};
use error::{WebDriverResult, WebDriverError, ErrorStatus};
use httpapi::{Route, WebDriverExtensionRoute, VoidWebDriverExtensionRoute};


#[derive(PartialEq)]
pub enum WebDriverCommand<T: WebDriverExtensionCommand> {
    NewSession,
    DeleteSession,
    Get(GetParameters),
    GetCurrentUrl,
    GoBack,
    GoForward,
    Refresh,
    GetTitle,
    GetPageSource,
    GetWindowHandle,
    GetWindowHandles,
    Close,
    SetWindowSize(WindowSizeParameters),
    GetWindowSize,
    MaximizeWindow,
//    FullscreenWindow // Not supported in marionette
    SwitchToWindow(SwitchToWindowParameters),
    SwitchToFrame(SwitchToFrameParameters),
    SwitchToParentFrame,
    FindElement(LocatorParameters),
    FindElements(LocatorParameters),
    FindElementElement(WebElement, LocatorParameters),
    FindElementElements(WebElement, LocatorParameters),
    GetActiveElement,
    IsDisplayed(WebElement),
    IsSelected(WebElement),
    GetElementAttribute(WebElement, String),
    GetCSSValue(WebElement, String),
    GetElementText(WebElement),
    GetElementTagName(WebElement),
    GetElementRect(WebElement),
    IsEnabled(WebElement),
    ExecuteScript(JavascriptCommandParameters),
    ExecuteAsyncScript(JavascriptCommandParameters),
    GetCookies,
    GetCookie(String),
    AddCookie(AddCookieParameters),
    DeleteCookies,
    DeleteCookie(String),
    SetTimeouts(TimeoutsParameters),
    //Actions(ActionsParameters),
    ElementClick(WebElement),
    ElementTap(WebElement),
    ElementClear(WebElement),
    ElementSendKeys(WebElement, SendKeysParameters),
    DismissAlert,
    AcceptAlert,
    GetAlertText,
    SendAlertText(SendAlertTextParameters),
    TakeScreenshot,
    Extension(T)
}

pub trait WebDriverExtensionCommand : Clone + Send + PartialEq {
    fn parameters_json(&self) -> Option<Json>;
}

#[derive(Clone, PartialEq)]
pub struct VoidWebDriverExtensionCommand;

impl WebDriverExtensionCommand for VoidWebDriverExtensionCommand {
    fn parameters_json(&self) -> Option<Json> {
        panic!("No extensions implemented");
    }
}

#[derive(PartialEq)]
pub struct WebDriverMessage <U: WebDriverExtensionRoute=VoidWebDriverExtensionRoute> {
    pub session_id: Option<String>,
    pub command: WebDriverCommand<U::Command>,
}

impl <U: WebDriverExtensionRoute> WebDriverMessage<U> {
    pub fn new(session_id: Option<String>, command: WebDriverCommand<U::Command>) -> WebDriverMessage<U> {
        WebDriverMessage {
            session_id: session_id,
            command: command,
        }
    }

    pub fn from_http(match_type: Route<U>, params: &Captures, body: &str, requires_body: bool) -> WebDriverResult<WebDriverMessage<U>> {
        let session_id = WebDriverMessage::<U>::get_session_id(params);
        let body_data = if requires_body {
            debug!("Got request body {}", body);
            match Json::from_str(body) {
                Ok(x @ Json::Object(_)) => x,
                Ok(_) => return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                        "Body was not a json object")),
                Err(_) => return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                         format!("Failed to decode request body as json: {}", body)))
            }
        } else {
            Json::Null
        };
        let command = match match_type {
            Route::NewSession => WebDriverCommand::NewSession,
            Route::DeleteSession => WebDriverCommand::DeleteSession,
            Route::Get => {
                let parameters: GetParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::Get(parameters)
            },
            Route::GetCurrentUrl => WebDriverCommand::GetCurrentUrl,
            Route::GoBack => WebDriverCommand::GoBack,
            Route::GoForward => WebDriverCommand::GoForward,
            Route::Refresh => WebDriverCommand::Refresh,
            Route::GetTitle => WebDriverCommand::GetTitle,
            Route::GetPageSource => WebDriverCommand::GetPageSource,
            Route::GetWindowHandle => WebDriverCommand::GetWindowHandle,
            Route::GetWindowHandles => WebDriverCommand::GetWindowHandles,
            Route::Close => WebDriverCommand::Close,
            Route::SetTimeouts => {
                let parameters: TimeoutsParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::SetTimeouts(parameters)
            },
            Route::SetWindowSize => {
                let parameters: WindowSizeParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::SetWindowSize(parameters)
            },
            Route::GetWindowSize => WebDriverCommand::GetWindowSize,
            Route::MaximizeWindow => WebDriverCommand::MaximizeWindow,
            Route::SwitchToWindow => {
                let parameters: SwitchToWindowParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::SwitchToWindow(parameters)
            }
            Route::SwitchToFrame => {
                let parameters: SwitchToFrameParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::SwitchToFrame(parameters)
            },
            Route::SwitchToParentFrame => WebDriverCommand::SwitchToParentFrame,
            Route::FindElement => {
                let parameters: LocatorParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::FindElement(parameters)
            },
            Route::FindElements => {
                let parameters: LocatorParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::FindElements(parameters)
            },
            Route::FindElementElement => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                let parameters: LocatorParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::FindElementElement(element, parameters)
            },
            Route::FindElementElements => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                let parameters: LocatorParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::FindElementElements(element, parameters)
            },
            Route::GetActiveElement => WebDriverCommand::GetActiveElement,
            Route::IsDisplayed => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                WebDriverCommand::IsDisplayed(element)
            },
            Route::IsSelected => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                WebDriverCommand::IsSelected(element)
            },
            Route::GetElementAttribute => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                let attr = try_opt!(params.name("name"),
                                    ErrorStatus::InvalidArgument,
                                    "Missing name parameter").to_string();
                WebDriverCommand::GetElementAttribute(element, attr)
            },
            Route::GetCSSValue => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                let property = try_opt!(params.name("propertyName"),
                                        ErrorStatus::InvalidArgument,
                                        "Missing propertyName parameter").to_string();
                WebDriverCommand::GetCSSValue(element, property)
            },
            Route::GetElementText => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                WebDriverCommand::GetElementText(element)
            },
            Route::GetElementTagName => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                WebDriverCommand::GetElementTagName(element)
            },
            Route::GetElementRect => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                WebDriverCommand::GetElementRect(element)
            },
            Route::IsEnabled => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                WebDriverCommand::IsEnabled(element)
            },
            Route::ElementClick => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                WebDriverCommand::ElementClick(element)
            },
            Route::ElementTap => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                WebDriverCommand::ElementTap(element)
            },
            Route::ElementClear => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                WebDriverCommand::ElementClear(element)
            },
            Route::ElementSendKeys => {
                let element_id = try_opt!(params.name("elementId"),
                                          ErrorStatus::InvalidArgument,
                                          "Missing elementId parameter");
                let element = WebElement::new(element_id.to_string());
                let parameters: SendKeysParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::ElementSendKeys(element, parameters)
            },
            Route::ExecuteScript => {
                let parameters: JavascriptCommandParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::ExecuteScript(parameters)
            },
            Route::ExecuteAsyncScript => {
                let parameters: JavascriptCommandParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::ExecuteAsyncScript(parameters)
            },
            Route::GetCookies => {
                WebDriverCommand::GetCookies
            },
            Route::GetCookie => {
                let name = try_opt!(params.name("name"),
                                    ErrorStatus::InvalidArgument,
                                    "Missing name parameter").to_string();
                WebDriverCommand::GetCookie(name)
            },
            Route::AddCookie => {
                let parameters: AddCookieParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::AddCookie(parameters)
            },
            Route::DeleteCookies => {
                WebDriverCommand::DeleteCookies
            },
            Route::DeleteCookie => {
                let name = try_opt!(params.name("name"),
                                    ErrorStatus::InvalidArgument,
                                    "Missing name parameter").to_string();
                WebDriverCommand::DeleteCookie(name)
            },
            Route::DismissAlert => {
                WebDriverCommand::DismissAlert
            },
            Route::AcceptAlert => {
                WebDriverCommand::AcceptAlert
            },
            Route::GetAlertText => {
                WebDriverCommand::GetAlertText
            },
            Route::SendAlertText => {
                let parameters: SendAlertTextParameters = try!(Parameters::from_json(&body_data));
                WebDriverCommand::SendAlertText(parameters)
            },
            Route::TakeScreenshot => WebDriverCommand::TakeScreenshot,
            Route::Extension(ref extension) => {
                try!(extension.command(params, &body_data))
            }
        };
        Ok(WebDriverMessage::new(session_id, command))
    }

    fn get_session_id(params: &Captures) -> Option<String> {
        params.name("sessionId").map(|x| x.to_string())
    }
}

impl <U:WebDriverExtensionRoute> ToJson for WebDriverMessage<U> {
    fn to_json(&self) -> Json {
        let parameters = match self.command {
            WebDriverCommand::NewSession |
            WebDriverCommand::DeleteSession | WebDriverCommand::GetCurrentUrl |
            WebDriverCommand::GoBack | WebDriverCommand::GoForward | WebDriverCommand::Refresh |
            WebDriverCommand::GetTitle | WebDriverCommand::GetPageSource |
            WebDriverCommand::GetWindowHandle | WebDriverCommand::GetWindowHandles |
            WebDriverCommand::Close | WebDriverCommand::GetWindowSize | WebDriverCommand::MaximizeWindow |
            WebDriverCommand::SwitchToParentFrame | WebDriverCommand::GetActiveElement |
            WebDriverCommand::IsDisplayed(_) | WebDriverCommand::IsSelected(_) |
            WebDriverCommand::GetElementAttribute(_, _) | WebDriverCommand::GetCSSValue(_, _) |
            WebDriverCommand::GetElementText(_) | WebDriverCommand::GetElementTagName(_) |
            WebDriverCommand::GetElementRect(_) | WebDriverCommand::IsEnabled(_) |
            WebDriverCommand::GetCookies | WebDriverCommand::GetCookie(_) |
            WebDriverCommand::DeleteCookies | WebDriverCommand::DeleteCookie(_) |
            WebDriverCommand::DismissAlert | WebDriverCommand::AcceptAlert |
            WebDriverCommand::GetAlertText | WebDriverCommand::ElementClick(_) |
            WebDriverCommand::ElementTap(_) | WebDriverCommand::ElementClear(_) |
            WebDriverCommand::TakeScreenshot => {
                None
            },
            WebDriverCommand::Get(ref x) => Some(x.to_json()),
            WebDriverCommand::SetTimeouts(ref x) => Some(x.to_json()),
            WebDriverCommand::SetWindowSize(ref x) => Some(x.to_json()),
            WebDriverCommand::SwitchToWindow(ref x) => Some(x.to_json()),
            WebDriverCommand::SwitchToFrame(ref x) => Some(x.to_json()),
            WebDriverCommand::FindElement(ref x) => Some(x.to_json()),
            WebDriverCommand::FindElements(ref x) => Some(x.to_json()),
            WebDriverCommand::FindElementElement(_, ref x) => Some(x.to_json()),
            WebDriverCommand::FindElementElements(_, ref x) => Some(x.to_json()),
            WebDriverCommand::ElementSendKeys(_, ref x) => Some(x.to_json()),
            WebDriverCommand::ExecuteScript(ref x) |
            WebDriverCommand::ExecuteAsyncScript(ref x) => Some(x.to_json()),
            WebDriverCommand::AddCookie(ref x) => Some(x.to_json()),
            WebDriverCommand::SendAlertText(ref x) => Some(x.to_json()),
            WebDriverCommand::Extension(ref x) => x.parameters_json(),
        };

        let mut data = BTreeMap::new();
        if let Some(parameters) = parameters {
            data.insert("parameters".to_string(), parameters);
        }
        Json::Object(data)
    }
}

pub trait Parameters: Sized {
    fn from_json(body: &Json) -> WebDriverResult<Self>;
}

#[derive(PartialEq)]
pub struct GetParameters {
    pub url: String
}

impl Parameters for GetParameters {
    fn from_json(body: &Json) -> WebDriverResult<GetParameters> {
        let data = try_opt!(body.as_object(), ErrorStatus::UnknownError,
                            "Message body was not an object");
        let url = try_opt!(
            try_opt!(data.get("url"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'url' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "'url' not a string");
        return Ok(GetParameters {
            url: url.to_string()
        })
    }
}

impl ToJson for GetParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("url".to_string(), self.url.to_json());
        Json::Object(data)
    }
}

#[derive(PartialEq)]
pub struct TimeoutsParameters {
    pub type_: String,
    pub ms: f64
}

impl Parameters for TimeoutsParameters {
    fn from_json(body: &Json) -> WebDriverResult<TimeoutsParameters> {
        let data = try_opt!(body.as_object(), ErrorStatus::UnknownError,
                            "Message body was not an object");
        let type_ = try_opt!(
            try_opt!(data.get("type"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'type' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "'type' not a string");

        let ms = try_opt!(
            try_opt!(data.get("ms"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'ms' parameter").as_f64(),
            ErrorStatus::InvalidArgument,
            "'ms' not a float");
        return Ok(TimeoutsParameters {
            type_: type_.to_string(),
            ms: ms
        })
    }
}

impl ToJson for TimeoutsParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("type".to_string(), self.type_.to_json());
        data.insert("ms".to_string(), self.ms.to_json());
        Json::Object(data)
    }
}

#[derive(PartialEq)]
pub struct WindowSizeParameters {
    pub width: u64,
    pub height: u64
}

impl Parameters for WindowSizeParameters {
    fn from_json(body: &Json) -> WebDriverResult<WindowSizeParameters> {
        let data = try_opt!(body.as_object(), ErrorStatus::UnknownError,
                            "Message body was not an object");
        let height = try_opt!(
            try_opt!(data.get("height"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'height' parameter").as_u64(),
            ErrorStatus::InvalidArgument,
            "'height' is not a positive integer");
        let width = try_opt!(
            try_opt!(data.get("width"),
                     ErrorStatus::InvalidArgument,
                     "Missing width parameter").as_u64(),
            ErrorStatus::InvalidArgument,
            "'width' is not a positive integer");
        return Ok(WindowSizeParameters {
            height: height,
            width: width
        })
    }
}

impl ToJson for WindowSizeParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("width".to_string(), self.width.to_json());
        data.insert("height".to_string(), self.height.to_json());
        Json::Object(data)
    }
}

#[derive(PartialEq)]
pub struct SwitchToWindowParameters {
    pub handle: String
}

impl Parameters for SwitchToWindowParameters {
    fn from_json(body: &Json) -> WebDriverResult<SwitchToWindowParameters> {
        let data = try_opt!(body.as_object(), ErrorStatus::UnknownError,
                            "Message body was not an object");
        let handle = try_opt!(
            try_opt!(data.get("handle"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'handle' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "'handle' not a string");
        return Ok(SwitchToWindowParameters {
            handle: handle.to_string()
        })
    }
}

impl ToJson for SwitchToWindowParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("handle".to_string(), self.handle.to_json());
        Json::Object(data)
    }
}

#[derive(PartialEq)]
pub struct LocatorParameters {
    pub using: LocatorStrategy,
    pub value: String
}

impl Parameters for LocatorParameters {
    fn from_json(body: &Json) -> WebDriverResult<LocatorParameters> {
        let data = try_opt!(body.as_object(), ErrorStatus::UnknownError,
                            "Message body was not an object");

        let using = try!(LocatorStrategy::from_json(
            try_opt!(data.get("using"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'using' parameter")));

        let value = try_opt!(
            try_opt!(data.get("value"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'value' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "Could not convert using to string").to_string();

        return Ok(LocatorParameters {
            using: using,
            value: value
        })
    }
}

impl ToJson for LocatorParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("using".to_string(), self.using.to_json());
        data.insert("value".to_string(), self.value.to_json());
        Json::Object(data)
    }
}

#[derive(PartialEq)]
pub struct SwitchToFrameParameters {
    pub id: FrameId
}

impl Parameters for SwitchToFrameParameters {
    fn from_json(body: &Json) -> WebDriverResult<SwitchToFrameParameters> {
        let data = try_opt!(body.as_object(),
                            ErrorStatus::UnknownError,
                            "Message body was not an object");
        let id = try!(FrameId::from_json(try_opt!(data.get("id"),
                                                  ErrorStatus::UnknownError,
                                                  "Missing 'id' parameter")));

        Ok(SwitchToFrameParameters {
            id: id
        })
    }
}

impl ToJson for SwitchToFrameParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("id".to_string(), self.id.to_json());
        Json::Object(data)
    }
}

#[derive(PartialEq)]
pub struct SendKeysParameters {
    pub value: Vec<char>
}

impl Parameters for SendKeysParameters {
    fn from_json(body: &Json) -> WebDriverResult<SendKeysParameters> {
        let data = try_opt!(body.as_object(),
                            ErrorStatus::InvalidArgument,
                            "Message body was not an object");
        let value_json = try_opt!(try_opt!(data.get("value"),
                                           ErrorStatus::InvalidArgument,
                                           "Missing 'value' parameter").as_array(),
                                  ErrorStatus::InvalidArgument,
                                  "Could not convert 'value' to array");

        let value = try!(value_json.iter().map(|x| {
           let str_value = try_opt!(x.as_string(),
                                    ErrorStatus::InvalidArgument,
                                    "Value was not a string");
            let chars = str_value.chars().collect::<Vec<char>>();
            if chars.len() != 1 {
                return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                               "Value was not a string"));
            }
            Ok(chars[0])
        }).collect::<Result<Vec<_>, _>>());

        Ok(SendKeysParameters {
            value: value
        })
    }
}

impl ToJson for SendKeysParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        let value_string: Vec<String> = self.value.iter().map(|x| {
            x.to_string()
        }).collect();
        data.insert("value".to_string(), value_string.to_json());
        Json::Object(data)
    }
}

#[derive(PartialEq)]
pub struct JavascriptCommandParameters {
    pub script: String,
    pub args: Nullable<Vec<Json>>
}

impl Parameters for JavascriptCommandParameters {
    fn from_json(body: &Json) -> WebDriverResult<JavascriptCommandParameters> {
        let data = try_opt!(body.as_object(),
                            ErrorStatus::InvalidArgument,
                            "Message body was not an object");

        let args_json = try_opt!(data.get("args"),
                                 ErrorStatus::InvalidArgument,
                                 "Missing args parameter");

        let args = try!(Nullable::from_json(
            args_json,
            |x| {
                Ok((try_opt!(x.as_array(),
                             ErrorStatus::InvalidArgument,
                             "Failed to convert args to Array")).clone())
            }));

         //TODO: Look for WebElements in args?
        let script = try_opt!(
            try_opt!(data.get("script"),
                     ErrorStatus::InvalidArgument,
                     "Missing script parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "Failed to convert script to String");
        Ok(JavascriptCommandParameters {
            script: script.to_string(),
            args: args.clone()
        })
    }
}

impl ToJson for JavascriptCommandParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        //TODO: Wrap script so that it becomes marionette-compatible
        data.insert("script".to_string(), self.script.to_json());
        data.insert("args".to_string(), self.args.to_json());
        Json::Object(data)
    }
}

#[derive(PartialEq)]
pub struct GetCookieParameters {
    pub name: Nullable<String>
}

impl Parameters for GetCookieParameters {
    fn from_json(body: &Json) -> WebDriverResult<GetCookieParameters> {
        let data = try_opt!(body.as_object(), ErrorStatus::InvalidArgument,
                            "Message body was not an object");
        let name_json = try_opt!(data.get("name"),
                                 ErrorStatus::InvalidArgument,
                                 "Missing 'name' parameter");
        let name = try!(Nullable::from_json(
            name_json,
            |x| {
                Ok(try_opt!(x.as_string(),
                            ErrorStatus::InvalidArgument,
                            "Failed to convert name to String").to_string())
            }));
        return Ok(GetCookieParameters {
            name: name
        })
    }
}

impl ToJson for GetCookieParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("name".to_string(), self.name.to_json());
        Json::Object(data)
    }
}

#[derive(PartialEq)]
pub struct AddCookieParameters {
    pub name: String,
    pub value: String,
    pub path: Nullable<String>,
    pub domain: Nullable<String>,
    pub expiry: Nullable<Date>,
    pub maxAge: Nullable<Date>,
    pub secure: bool,
    pub httpOnly: bool
}

impl Parameters for AddCookieParameters {
    fn from_json(body: &Json) -> WebDriverResult<AddCookieParameters> {
        if !body.is_object() {
            return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                           "Message body was not an object"));
        }

        let data = try_opt!(body.find("cookie").and_then(|x| x.as_object()),
                            ErrorStatus::UnableToSetCookie,
                            "Cookie parameter not found or not an object");

        let name = try_opt!(
            try_opt!(data.get("name"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'name' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "'name' is not a string").to_string();

        let value = try_opt!(
            try_opt!(data.get("value"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'value' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "'value' is not a string").to_string();

        let path = match data.get("path") {
            Some(path_json) => {
                try!(Nullable::from_json(
                    path_json,
                    |x| {
                        Ok(try_opt!(x.as_string(),
                                    ErrorStatus::InvalidArgument,
                                    "Failed to convert path to String").to_string())
                    }))
            },
            None => Nullable::Null
        };

        let domain = match data.get("domain") {
            Some(domain_json) => {
                try!(Nullable::from_json(
                    domain_json,
                    |x| {
                        Ok(try_opt!(x.as_string(),
                                    ErrorStatus::InvalidArgument,
                                    "Failed to convert domain to String").to_string())
                    }))
            },
            None => Nullable::Null
        };

        //TODO: This is supposed to support some text format
        let expiry = match data.get("expiry") {
            Some(expiry_json) => {
                try!(Nullable::from_json(
                    expiry_json,
                    |x| {
                        Ok(Date::new(try_opt!(x.as_u64(),
                                              ErrorStatus::InvalidArgument,
                                              "Failed to convert expiry to Date")))
                    }))
            },
            None => Nullable::Null
        };

        let max_age = match data.get("maxAge") {
            Some(max_age_json) => {
                try!(Nullable::from_json(
                    max_age_json,
                    |x| {
                        Ok(Date::new(try_opt!(x.as_u64(),
                                              ErrorStatus::InvalidArgument,
                                              "Failed to convert expiry to Date")))
                    }))
            },
            None => Nullable::Null
        };

        let secure = match data.get("secure") {
            Some(x) => try_opt!(x.as_boolean(),
                                ErrorStatus::InvalidArgument,
                                "Failed to convert secure to boolean"),
            None => false
        };

        let http_only = match data.get("httpOnly") {
            Some(x) => try_opt!(x.as_boolean(),
                                ErrorStatus::InvalidArgument,
                                "Failed to convert httpOnly to boolean"),
            None => false
        };

        return Ok(AddCookieParameters {
            name: name,
            value: value,
            path: path,
            domain: domain,
            expiry: expiry,
            maxAge: max_age,
            secure: secure,
            httpOnly: http_only
        })
    }
}

impl ToJson for AddCookieParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("name".to_string(), self.name.to_json());
        data.insert("value".to_string(), self.value.to_json());
        data.insert("path".to_string(), self.path.to_json());
        data.insert("domain".to_string(), self.domain.to_json());
        data.insert("expiry".to_string(), self.expiry.to_json());
        data.insert("maxAge".to_string(), self.maxAge.to_json());
        data.insert("secure".to_string(), self.secure.to_json());
        data.insert("httpOnly".to_string(), self.httpOnly.to_json());
        Json::Object(data)
    }
}

#[derive(PartialEq)]
pub struct SendAlertTextParameters {
    message: String
}

impl Parameters for SendAlertTextParameters {
    fn from_json(body: &Json) -> WebDriverResult<SendAlertTextParameters> {
        let data = try_opt!(body.as_object(), ErrorStatus::InvalidArgument,
                            "Message body was not an object");
        let keys = try_opt!(
            try_opt!(data.get("text"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'text' parameter").as_string(),
            ErrorStatus::InvalidArgument,
            "'text' not a string").to_string();
        return Ok(SendAlertTextParameters {
            message: keys
        })
    }
}

impl ToJson for SendAlertTextParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("message".to_string(), self.message.to_json());
        Json::Object(data)
    }
}

#[derive(PartialEq)]
pub struct TakeScreenshotParameters {
    pub element: Nullable<WebElement>
}

impl Parameters for TakeScreenshotParameters {
    fn from_json(body: &Json) -> WebDriverResult<TakeScreenshotParameters> {
        let data = try_opt!(body.as_object(),
                            ErrorStatus::InvalidArgument,
                            "Message body was not an object");
        let element = match data.get("element") {
            Some(element_json) => try!(Nullable::from_json(
                element_json,
                |x| {
                    Ok(try!(WebElement::from_json(x)))
                })),
            None => Nullable::Null
        };

        return Ok(TakeScreenshotParameters {
            element: element
        })
    }
}

impl ToJson for TakeScreenshotParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("element".to_string(), self.element.to_json());
        Json::Object(data)
    }
}
