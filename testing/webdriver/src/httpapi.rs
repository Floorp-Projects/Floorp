use regex::{Regex, Captures};

use hyper::method::Method;
use hyper::method::Method::{Get, Post, Delete};

use command::{WebDriverMessage};
use error::{WebDriverResult, WebDriverError, ErrorStatus};

static ROUTES: [(Method, &'static str, Route); 41] = [
    (Post, "/session", Route::NewSession),
    (Delete, "/session/{sessionId}", Route::DeleteSession),
    (Post, "/session/{sessionId}/url", Route::Get),
    (Get, "/session/{sessionId}/url", Route::GetCurrentUrl),
    (Post, "/session/{sessionId}/back", Route::GoBack),
    (Post, "/session/{sessionId}/forward", Route::GoForward),
    (Post, "/session/{sessionId}/refresh", Route::Refresh),
    (Get, "/session/{sessionId}/title", Route::GetTitle),
    (Get, "/session/{sessionId}/window_handle", Route::GetWindowHandle),
    (Get, "/session/{sessionId}/window_handles", Route::GetWindowHandles),
    (Delete, "/session/{sessionId}/window_handle", Route::Close),
    (Post, "/session/{sessionId}/window/size", Route::SetWindowSize),
    (Get, "/session/{sessionId}/window/size", Route::GetWindowSize),
    (Post, "/session/{sessionId}/window/maximize", Route::MaximizeWindow),
    (Post, "/session/{sessionId}/window", Route::SwitchToWindow),
    (Post, "/session/{sessionId}/frame", Route::SwitchToFrame),
    (Post, "/session/{sessionId}/frame/parent", Route::SwitchToParentFrame),
    (Post, "/session/{sessionId}/element", Route::FindElement),
    (Post, "/session/{sessionId}/elements", Route::FindElements),
    (Get, "/session/{sessionId}/element/{elementId}/displayed", Route::IsDisplayed),
    (Get, "/session/{sessionId}/element/{elementId}/selected", Route::IsSelected),
    (Get, "/session/{sessionId}/element/{elementId}/attribute/{name}", Route::GetElementAttribute),
    (Get, "/session/{sessionId}/element/{elementId}/css/{propertyName}", Route::GetCSSValue),
    (Get, "/session/{sessionId}/element/{elementId}/text", Route::GetElementText),
    (Get, "/session/{sessionId}/element/{elementId}/name", Route::GetElementTagName),
    (Get, "/session/{sessionId}/element/{elementId}/rect", Route::GetElementRect),
    (Get, "/session/{sessionId}/element/{elementId}/enabled", Route::IsEnabled),
    (Post, "/session/{sessionId}/execute", Route::ExecuteScript),
    (Post, "/session/{sessionId}/execute_async", Route::ExecuteAsyncScript),
    (Get, "/session/{sessionId}/cookie", Route::GetCookie),
    (Post, "/session/{sessionId}/cookie", Route::AddCookie),
    (Post, "/session/{sessionId}/timeouts", Route::SetTimeouts),
    //(Post, "/session/{sessionId}/actions", Route::Actions),
    (Post, "/session/{sessionId}/element/{elementId}/click", Route::ElementClick),
    (Post, "/session/{sessionId}/element/{elementId}/tap", Route::ElementTap),
    (Post, "/session/{sessionId}/element/{elementId}/clear", Route::ElementClear),
    (Post, "/session/{sessionId}/element/{elementId}/value", Route::ElementSendKeys),
    (Post, "/session/{sessionId}/dismiss_alert", Route::DismissAlert),
    (Post, "/session/{sessionId}/accept_alert", Route::AcceptAlert),
    (Get, "/session/{sessionId}/alert_text", Route::GetAlertText),
    (Post, "/session/{sessionId}/alert_text", Route::SendAlertText),
    (Get, "/session/{sessionId}/screenshot", Route::TakeScreenshot)
];

#[derive(Clone, Copy)]
pub enum Route {
    NewSession,
    DeleteSession,
    Get,
    GetCurrentUrl,
    GoBack,
    GoForward,
    Refresh,
    GetTitle,
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
    GetCookie,
    AddCookie,
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
    TakeScreenshot
}

#[derive(Clone)]
struct RequestMatcher {
    method: Method,
    path_regexp: Regex,
    match_type: Route
}

impl RequestMatcher {
    pub fn new(method: Method, path: &str, match_type: Route) -> RequestMatcher {
        let path_regexp = RequestMatcher::compile_path(path);
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

pub struct WebDriverHttpApi {
    routes: Vec<(Method, RequestMatcher)>
}

impl WebDriverHttpApi {
    pub fn new() -> WebDriverHttpApi {
        let mut rv = WebDriverHttpApi {
            routes: vec![]
        };
        debug!("Creating routes");
        for &(ref method, ref url, ref match_type) in ROUTES.iter() {
            rv.add(method.clone(), *url, *match_type);
        };
        rv
    }

    fn add(&mut self, method: Method, path: &str, match_type: Route) {
        let http_matcher = RequestMatcher::new(method.clone(), path, match_type);
        self.routes.push((method, http_matcher));
    }

    pub fn decode_request(&self, method: Method, path: &str, body: &str) -> WebDriverResult<WebDriverMessage> {
        let mut error = ErrorStatus::UnknownPath;
        for &(ref match_method, ref matcher) in self.routes.iter() {
            if method == *match_method {
                let (method_match, captures) = matcher.get_match(method.clone(), path);
                if captures.is_some() {
                    if method_match {
                        return WebDriverMessage::from_http(matcher.match_type,
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
                                &format!("{} {} did not match a known command", method, path)[..]))
    }
}
