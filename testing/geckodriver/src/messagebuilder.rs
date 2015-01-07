use regex::{Regex, Captures};

use hyper::method::{Method, Get, Post, Delete};

use command::{WebDriverMessage};
use common::{WebDriverResult, WebDriverError, ErrorStatus};

#[deriving(Clone)]
pub enum MatchType {
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

#[deriving(Clone)]
pub struct RequestMatcher {
    method: Method,
    path_regexp: Regex,
    match_type: MatchType
}

impl RequestMatcher {
    pub fn new(method: Method, path: &str, match_type: MatchType) -> RequestMatcher {
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
        let mut components = path.split('/');
        for component in components {
            if component.starts_with("{") {
                if !component.ends_with("}") {
                    panic!("Invalid url pattern")
                }
                rv.push_str(format!("(?P<{}>[^/]+)/", component[1..component.len()-1])[]);
            } else {
                rv.push_str(format!("{}/", component)[]);
            }
        }
        //Remove the trailing /
        rv.pop();
        rv.push_str("$");
        //This will fail at runtime if the regexp is invalid
        Regex::new(rv[]).unwrap()
    }
}

pub struct MessageBuilder {
    http_matchers: Vec<(Method, RequestMatcher)>
}

impl MessageBuilder {
    pub fn new() -> MessageBuilder {
        MessageBuilder {
            http_matchers: vec![]
        }
    }

    pub fn from_http(&self, method: Method, path: &str, body: &str) -> WebDriverResult<WebDriverMessage> {
        let mut error = ErrorStatus::UnknownPath;
        for &(ref match_method, ref matcher) in self.http_matchers.iter() {
            if method == *match_method {
                let (method_match, captures) = matcher.get_match(method.clone(), path);
                if captures.is_some() {
                    if method_match {
                        return WebDriverMessage::from_http(matcher.match_type,
                                                           &captures.unwrap(),
                                                           body)
                    } else {
                        error = ErrorStatus::UnknownMethod;
                    }
                }
            }
        }
        Err(WebDriverError::new(error,
                                format!("{} {} did not match a known command", method, path)[]))
    }

    pub fn add(&mut self, method: Method, path: &str, match_type: MatchType) {
        let http_matcher = RequestMatcher::new(method.clone(), path, match_type);
        self.http_matchers.push((method, http_matcher));
    }
}

pub fn get_builder() -> MessageBuilder {
    let mut builder = MessageBuilder::new();
    let matchers = vec![(Post, "/session", MatchType::NewSession),
                        (Delete, "/session/{sessionId}", MatchType::DeleteSession),
                        (Post, "/session/{sessionId}/url", MatchType::Get),
                        (Get, "/session/{sessionId}/url", MatchType::GetCurrentUrl),
                        (Post, "/session/{sessionId}/back", MatchType::GoBack),
                        (Post, "/session/{sessionId}/forward", MatchType::GoForward),
                        (Post, "/session/{sessionId}/refresh", MatchType::Refresh),
                        (Get, "/session/{sessionId}/title", MatchType::GetTitle),
                        (Get, "/session/{sessionId}/window_handle", MatchType::GetWindowHandle),
                        (Get, "/session/{sessionId}/window_handles", MatchType::GetWindowHandles),
                        (Delete, "/session/{sessionId}/window_handle", MatchType::Close),
                        (Post, "/session/{sessionId}/window/size", MatchType::SetWindowSize),
                        (Get, "/session/{sessionId}/window/size", MatchType::GetWindowSize),
                        (Post, "/session/{sessionId}/window/maximize", MatchType::MaximizeWindow),
                        (Post, "/session/{sessionId}/window", MatchType::SwitchToWindow),
                        (Post, "/session/{sessionId}/frame", MatchType::SwitchToFrame),
                        (Post, "/session/{sessionId}/frame/parent", MatchType::SwitchToParentFrame),
                        (Post, "/session/{sessionId}/element", MatchType::FindElement),
                        (Post, "/session/{sessionId}/elements", MatchType::FindElements),
                        (Get, "/session/{sessionId}/element/{elementId}/displayed", MatchType::IsDisplayed),
                        (Get, "/session/{sessionId}/element/{elementId}/selected", MatchType::IsSelected),
                        (Get, "/session/{sessionId}/element/{elementId}/attribute/{name}", MatchType::GetElementAttribute),
                        (Get, "/session/{sessionId}/element/{elementId}/css/{propertyName}", MatchType::GetCSSValue),
                        (Get, "/session/{sessionId}/element/{elementId}/text", MatchType::GetElementText),
                        (Get, "/session/{sessionId}/element/{elementId}/name", MatchType::GetElementTagName),
                        (Get, "/session/{sessionId}/element/{elementId}/rect", MatchType::GetElementRect),
                        (Get, "/session/{sessionId}/element/{elementId}/enabled", MatchType::IsEnabled),
                        (Post, "/session/{sessionId}/execute", MatchType::ExecuteScript),
                        (Post, "/session/{sessionId}/execute_async", MatchType::ExecuteAsyncScript),
                        (Get, "/session/{sessionId}/cookie", MatchType::GetCookie),
                        (Post, "/session/{sessionId}/cookie", MatchType::AddCookie),
                        (Post, "/session/{sessionId}/timeouts", MatchType::SetTimeouts),
                        //(Post, "/session/{sessionId}/actions", MatchType::Actions),
                        (Post, "/session/{sessionId}/element/{elementId}/click", MatchType::ElementClick),
                        (Post, "/session/{sessionId}/element/{elementId}/tap", MatchType::ElementTap),
                        (Post, "/session/{sessionId}/element/{elementId}/clear", MatchType::ElementClear),
                        (Post, "/session/{sessionId}/element/{elementId}/sendKeys", MatchType::ElementSendKeys),
                        (Post, "/session/{sessionId}/dismiss_alert", MatchType::DismissAlert),
                        (Post, "/session/{sessionId}/accept_alert", MatchType::AcceptAlert),
                        (Get, "/session/{sessionId}/alert_text", MatchType::GetAlertText),
                        (Post, "/session/{sessionId}/alert_text", MatchType::SendAlertText),
                        (Get, "/session/{sessionId}/screenshot", MatchType::TakeScreenshot)
                        ];
    debug!("Creating routes");
    for &(ref method, ref url, ref match_type) in matchers.iter() {
        builder.add(method.clone(), *url, *match_type);
    }
    builder
}
