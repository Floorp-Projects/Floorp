use regex::{Regex, Captures};
use rustc_serialize::json::Json;

use hyper::method::Method;
use hyper::method::Method::{Get, Post, Delete};

use command::{WebDriverCommand, WebDriverMessage, WebDriverExtensionCommand,
              VoidWebDriverExtensionCommand};
use error::{WebDriverResult, WebDriverError, ErrorStatus};

fn standard_routes<U:WebDriverExtensionRoute>() -> Vec<(Method, &'static str, Route<U>)> {
    return vec![(Post, "/session", Route::NewSession),
                (Delete, "/session/{sessionId}", Route::DeleteSession),
                (Post, "/session/{sessionId}/url", Route::Get),
                (Get, "/session/{sessionId}/url", Route::GetCurrentUrl),
                (Post, "/session/{sessionId}/back", Route::GoBack),
                (Post, "/session/{sessionId}/forward", Route::GoForward),
                (Post, "/session/{sessionId}/refresh", Route::Refresh),
                (Get, "/session/{sessionId}/title", Route::GetTitle),
                (Get, "/session/{sessionId}/source", Route::GetPageSource),
                (Get, "/session/{sessionId}/window", Route::GetWindowHandle),
                (Get, "/session/{sessionId}/window/handles", Route::GetWindowHandles),
                (Delete, "/session/{sessionId}/window", Route::Close),
                (Post, "/session/{sessionId}/window/size", Route::SetWindowSize),
                (Get, "/session/{sessionId}/window/size", Route::GetWindowSize),
                (Post, "/session/{sessionId}/window/maximize", Route::MaximizeWindow),
                (Post, "/session/{sessionId}/window", Route::SwitchToWindow),
                (Post, "/session/{sessionId}/frame", Route::SwitchToFrame),
                (Post, "/session/{sessionId}/frame/parent", Route::SwitchToParentFrame),
                (Post, "/session/{sessionId}/element", Route::FindElement),
                (Post, "/session/{sessionId}/elements", Route::FindElements),
                (Post, "/session/{sessionId}/element/{elementId}/element", Route::FindElementElement),
                (Post, "/session/{sessionId}/element/{elementId}/elements", Route::FindElementElements),
                (Get, "/session/{sessionId}/element/active", Route::GetActiveElement),
                (Get, "/session/{sessionId}/element/{elementId}/displayed", Route::IsDisplayed),
                (Get, "/session/{sessionId}/element/{elementId}/selected", Route::IsSelected),
                (Get, "/session/{sessionId}/element/{elementId}/attribute/{name}", Route::GetElementAttribute),
                (Get, "/session/{sessionId}/element/{elementId}/css/{propertyName}", Route::GetCSSValue),
                (Get, "/session/{sessionId}/element/{elementId}/text", Route::GetElementText),
                (Get, "/session/{sessionId}/element/{elementId}/name", Route::GetElementTagName),
                (Get, "/session/{sessionId}/element/{elementId}/rect", Route::GetElementRect),
                (Get, "/session/{sessionId}/element/{elementId}/enabled", Route::IsEnabled),
                (Post, "/session/{sessionId}/execute/sync", Route::ExecuteScript),
                (Post, "/session/{sessionId}/execute/async", Route::ExecuteAsyncScript),
                (Get, "/session/{sessionId}/cookie", Route::GetCookies),
                (Get, "/session/{sessionId}/cookie/{name}", Route::GetCookie),
                (Post, "/session/{sessionId}/cookie", Route::AddCookie),
                (Delete, "/session/{sessionId}/cookie", Route::DeleteCookies),
                (Delete, "/session/{sessionId}/cookie/{name}", Route::DeleteCookie),
                (Post, "/session/{sessionId}/timeouts", Route::SetTimeouts),
                //(Post, "/session/{sessionId}/actions", Route::Actions),
                (Post, "/session/{sessionId}/element/{elementId}/click", Route::ElementClick),
                (Post, "/session/{sessionId}/element/{elementId}/tap", Route::ElementTap),
                (Post, "/session/{sessionId}/element/{elementId}/clear", Route::ElementClear),
                (Post, "/session/{sessionId}/element/{elementId}/value", Route::ElementSendKeys),
                (Post, "/session/{sessionId}/alert/dismiss", Route::DismissAlert),
                (Post, "/session/{sessionId}/alert/accept", Route::AcceptAlert),
                (Get, "/session/{sessionId}/alert/text", Route::GetAlertText),
                (Post, "/session/{sessionId}/alert/text", Route::SendAlertText),
                (Get, "/session/{sessionId}/screenshot", Route::TakeScreenshot),
                // TODO Remove this when > v0.5 is released. There for compatibility reasons with existing
                //      Webdriver implementations.
                (Get, "/session/{sessionId}/alert_text", Route::GetAlertText),
                (Post, "/session/{sessionId}/alert_text", Route::SendAlertText),
                (Post, "/session/{sessionId}/accept_alert", Route::AcceptAlert),
                (Post, "/session/{sessionId}/dismiss_alert", Route::DismissAlert),
                (Get, "/session/{sessionId}/window_handle", Route::GetWindowHandle),
                (Get, "/session/{sessionId}/window_handles", Route::GetWindowHandles),
                (Delete, "/session/{sessionId}/window_handle", Route::Close),
                (Post, "/session/{sessionId}/execute_async", Route::ExecuteAsyncScript),
                (Post, "/session/{sessionId}/execute", Route::ExecuteScript),]
}

#[derive(Clone, Copy)]
pub enum Route<U:WebDriverExtensionRoute> {
    NewSession,
    DeleteSession,
    Get,
    GetCurrentUrl,
    GoBack,
    GoForward,
    Refresh,
    GetTitle,
    GetPageSource,
    GetWindowHandle,
    GetWindowHandles,
    Close,
    SetWindowSize,
    GetWindowSize,
    MaximizeWindow,
    SwitchToWindow,
    SwitchToFrame,
    SwitchToParentFrame,
    FindElement,
    FindElements,
    FindElementElement,
    FindElementElements,
    GetActiveElement,
    IsDisplayed,
    IsSelected,
    GetElementAttribute,
    GetCSSValue,
    GetElementText,
    GetElementTagName,
    GetElementRect,
    IsEnabled,
    ExecuteScript,
    ExecuteAsyncScript,
    GetCookies,
    GetCookie,
    AddCookie,
    DeleteCookies,
    DeleteCookie,
    SetTimeouts,
    //Actions XXX - once I understand the spec, perhaps
    ElementClick,
    ElementTap,
    ElementClear,
    ElementSendKeys,
    DismissAlert,
    AcceptAlert,
    GetAlertText,
    SendAlertText,
    TakeScreenshot,
    Extension(U)
}

pub trait WebDriverExtensionRoute : Clone + Send + PartialEq {
    type Command: WebDriverExtensionCommand + 'static;

    fn command(&self, &Captures, &Json) -> WebDriverResult<WebDriverCommand<Self::Command>>;
}

#[derive(Clone, PartialEq)]
pub struct VoidWebDriverExtensionRoute;

impl WebDriverExtensionRoute for VoidWebDriverExtensionRoute {
    type Command = VoidWebDriverExtensionCommand;

    fn command(&self, _:&Captures, _:&Json) -> WebDriverResult<WebDriverCommand<VoidWebDriverExtensionCommand>> {
        panic!("No extensions implemented");
    }
}

#[derive(Clone)]
struct RequestMatcher<U: WebDriverExtensionRoute> {
    method: Method,
    path_regexp: Regex,
    match_type: Route<U>
}

impl <U: WebDriverExtensionRoute> RequestMatcher<U> {
    pub fn new(method: Method, path: &str, match_type: Route<U>) -> RequestMatcher<U> {
        let path_regexp = RequestMatcher::<U>::compile_path(path);
        RequestMatcher {
            method: method,
            path_regexp: path_regexp,
            match_type: match_type
        }
    }

    pub fn get_match<'t>(&'t self, method: Method, path: &'t str) -> (bool, Option<Captures>) {
        let captures = self.path_regexp.captures(path);
        (method == self.method, captures)
    }

    fn compile_path(path: &str) -> Regex {
        let mut rv = String::new();
        rv.push_str("^");
        let components = path.split('/');
        for component in components {
            if component.starts_with("{") {
                if !component.ends_with("}") {
                    panic!("Invalid url pattern")
                }
                rv.push_str(&format!("(?P<{}>[^/]+)/", &component[1..component.len()-1])[..]);
            } else {
                rv.push_str(&format!("{}/", component)[..]);
            }
        }
        //Remove the trailing /
        rv.pop();
        rv.push_str("$");
        //This will fail at runtime if the regexp is invalid
        Regex::new(&rv[..]).unwrap()
    }
}

pub struct WebDriverHttpApi<U: WebDriverExtensionRoute> {
    routes: Vec<(Method, RequestMatcher<U>)>,
}

impl <U: WebDriverExtensionRoute> WebDriverHttpApi<U> {
    pub fn new(extension_routes:Vec<(Method, &str, U)>) -> WebDriverHttpApi<U> {
        let mut rv = WebDriverHttpApi::<U> {
            routes: vec![],
        };
        debug!("Creating routes");
        for &(ref method, ref url, ref match_type) in standard_routes::<U>().iter() {
            rv.add(method.clone(), *url, (*match_type).clone());
        };
        for &(ref method, ref url, ref extension_route) in extension_routes.iter() {
            rv.add(method.clone(), *url, Route::Extension(extension_route.clone()));
        };
        rv
    }

    fn add(&mut self, method: Method, path: &str, match_type: Route<U>) {
        let http_matcher = RequestMatcher::new(method.clone(), path, match_type);
        self.routes.push((method, http_matcher));
    }

    pub fn decode_request(&self, method: Method, path: &str, body: &str) -> WebDriverResult<WebDriverMessage<U>> {
        let mut error = ErrorStatus::UnknownPath;
        for &(ref match_method, ref matcher) in self.routes.iter() {
            if method == *match_method {
                let (method_match, captures) = matcher.get_match(method.clone(), path);
                if captures.is_some() {
                    if method_match {
                        return WebDriverMessage::from_http(matcher.match_type.clone(),
                                                           &captures.unwrap(),
                                                           body,
                                                           method == Post)
                    } else {
                        error = ErrorStatus::UnknownMethod;
                    }
                }
            }
        }
        Err(WebDriverError::new(error,
                                format!("{} {} did not match a known command", method, path)))
    }
}
