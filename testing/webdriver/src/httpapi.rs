use regex::{Captures, Regex};

use hyper::Method;
use serde_json::Value;

use command::{VoidWebDriverExtensionCommand, WebDriverCommand, WebDriverExtensionCommand,
              WebDriverMessage};
use error::{ErrorStatus, WebDriverError, WebDriverResult};

fn standard_routes<U: WebDriverExtensionRoute>() -> Vec<(Method, &'static str, Route<U>)> {
    return vec![
        (Method::POST, "/session", Route::NewSession),
        (Method::DELETE, "/session/{sessionId}", Route::DeleteSession),
        (Method::POST, "/session/{sessionId}/url", Route::Get),
        (Method::GET, "/session/{sessionId}/url", Route::GetCurrentUrl),
        (Method::POST, "/session/{sessionId}/back", Route::GoBack),
        (Method::POST, "/session/{sessionId}/forward", Route::GoForward),
        (Method::POST, "/session/{sessionId}/refresh", Route::Refresh),
        (Method::GET, "/session/{sessionId}/title", Route::GetTitle),
        (Method::GET, "/session/{sessionId}/source", Route::GetPageSource),
        (Method::GET, "/session/{sessionId}/window", Route::GetWindowHandle),
        (
            Method::GET,
            "/session/{sessionId}/window/handles",
            Route::GetWindowHandles,
        ),
        (Method::DELETE, "/session/{sessionId}/window", Route::CloseWindow),
        (
            Method::GET,
            "/session/{sessionId}/window/size",
            Route::GetWindowSize,
        ),
        (
            Method::POST,
            "/session/{sessionId}/window/size",
            Route::SetWindowSize,
        ),
        (
            Method::GET,
            "/session/{sessionId}/window/position",
            Route::GetWindowPosition,
        ),
        (
            Method::POST,
            "/session/{sessionId}/window/position",
            Route::SetWindowPosition,
        ),
        (
            Method::GET,
            "/session/{sessionId}/window/rect",
            Route::GetWindowRect,
        ),
        (
            Method::POST,
            "/session/{sessionId}/window/rect",
            Route::SetWindowRect,
        ),
        (
            Method::POST,
            "/session/{sessionId}/window/minimize",
            Route::MinimizeWindow,
        ),
        (
            Method::POST,
            "/session/{sessionId}/window/maximize",
            Route::MaximizeWindow,
        ),
        (
            Method::POST,
            "/session/{sessionId}/window/fullscreen",
            Route::FullscreenWindow,
        ),
        (Method::POST, "/session/{sessionId}/window", Route::SwitchToWindow),
        (Method::POST, "/session/{sessionId}/frame", Route::SwitchToFrame),
        (
            Method::POST,
            "/session/{sessionId}/frame/parent",
            Route::SwitchToParentFrame,
        ),
        (Method::POST, "/session/{sessionId}/element", Route::FindElement),
        (Method::POST, "/session/{sessionId}/elements", Route::FindElements),
        (
            Method::POST,
            "/session/{sessionId}/element/{elementId}/element",
            Route::FindElementElement,
        ),
        (
            Method::POST,
            "/session/{sessionId}/element/{elementId}/elements",
            Route::FindElementElements,
        ),
        (
            Method::GET,
            "/session/{sessionId}/element/active",
            Route::GetActiveElement,
        ),
        (
            Method::GET,
            "/session/{sessionId}/element/{elementId}/displayed",
            Route::IsDisplayed,
        ),
        (
            Method::GET,
            "/session/{sessionId}/element/{elementId}/selected",
            Route::IsSelected,
        ),
        (
            Method::GET,
            "/session/{sessionId}/element/{elementId}/attribute/{name}",
            Route::GetElementAttribute,
        ),
        (
            Method::GET,
            "/session/{sessionId}/element/{elementId}/property/{name}",
            Route::GetElementProperty,
        ),
        (
            Method::GET,
            "/session/{sessionId}/element/{elementId}/css/{propertyName}",
            Route::GetCSSValue,
        ),
        (
            Method::GET,
            "/session/{sessionId}/element/{elementId}/text",
            Route::GetElementText,
        ),
        (
            Method::GET,
            "/session/{sessionId}/element/{elementId}/name",
            Route::GetElementTagName,
        ),
        (
            Method::GET,
            "/session/{sessionId}/element/{elementId}/rect",
            Route::GetElementRect,
        ),
        (
            Method::GET,
            "/session/{sessionId}/element/{elementId}/enabled",
            Route::IsEnabled,
        ),
        (
            Method::POST,
            "/session/{sessionId}/execute/sync",
            Route::ExecuteScript,
        ),
        (
            Method::POST,
            "/session/{sessionId}/execute/async",
            Route::ExecuteAsyncScript,
        ),
        (Method::GET, "/session/{sessionId}/cookie", Route::GetCookies),
        (
            Method::GET,
            "/session/{sessionId}/cookie/{name}",
            Route::GetNamedCookie,
        ),
        (Method::POST, "/session/{sessionId}/cookie", Route::AddCookie),
        (Method::DELETE, "/session/{sessionId}/cookie", Route::DeleteCookies),
        (
            Method::DELETE,
            "/session/{sessionId}/cookie/{name}",
            Route::DeleteCookie,
        ),
        (Method::GET, "/session/{sessionId}/timeouts", Route::GetTimeouts),
        (Method::POST, "/session/{sessionId}/timeouts", Route::SetTimeouts),
        (
            Method::POST,
            "/session/{sessionId}/element/{elementId}/click",
            Route::ElementClick,
        ),
        (
            Method::POST,
            "/session/{sessionId}/element/{elementId}/tap",
            Route::ElementTap,
        ),
        (
            Method::POST,
            "/session/{sessionId}/element/{elementId}/clear",
            Route::ElementClear,
        ),
        (
            Method::POST,
            "/session/{sessionId}/element/{elementId}/value",
            Route::ElementSendKeys,
        ),
        (
            Method::POST,
            "/session/{sessionId}/alert/dismiss",
            Route::DismissAlert,
        ),
        (
            Method::POST,
            "/session/{sessionId}/alert/accept",
            Route::AcceptAlert,
        ),
        (Method::GET, "/session/{sessionId}/alert/text", Route::GetAlertText),
        (
            Method::POST,
            "/session/{sessionId}/alert/text",
            Route::SendAlertText,
        ),
        (
            Method::GET,
            "/session/{sessionId}/screenshot",
            Route::TakeScreenshot,
        ),
        (
            Method::GET,
            "/session/{sessionId}/element/{elementId}/screenshot",
            Route::TakeElementScreenshot,
        ),
        (Method::POST, "/session/{sessionId}/actions", Route::PerformActions),
        (
            Method::DELETE,
            "/session/{sessionId}/actions",
            Route::ReleaseActions,
        ),
        (Method::GET, "/status", Route::Status),
    ];
}

#[derive(Clone, Copy, Debug)]
pub enum Route<U: WebDriverExtensionRoute> {
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
    CloseWindow,
    GetWindowSize,     // deprecated
    SetWindowSize,     // deprecated
    GetWindowPosition, // deprecated
    SetWindowPosition, // deprecated
    GetWindowRect,
    SetWindowRect,
    MinimizeWindow,
    MaximizeWindow,
    FullscreenWindow,
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
    GetElementProperty,
    GetCSSValue,
    GetElementText,
    GetElementTagName,
    GetElementRect,
    IsEnabled,
    ExecuteScript,
    ExecuteAsyncScript,
    GetCookies,
    GetNamedCookie,
    AddCookie,
    DeleteCookies,
    DeleteCookie,
    GetTimeouts,
    SetTimeouts,
    ElementClick,
    ElementTap,
    ElementClear,
    ElementSendKeys,
    PerformActions,
    ReleaseActions,
    DismissAlert,
    AcceptAlert,
    GetAlertText,
    SendAlertText,
    TakeScreenshot,
    TakeElementScreenshot,
    Status,
    Extension(U),
}

pub trait WebDriverExtensionRoute: Clone + Send + PartialEq {
    type Command: WebDriverExtensionCommand + 'static;

    fn command(&self, &Captures, &Value) -> WebDriverResult<WebDriverCommand<Self::Command>>;
}

#[derive(Clone, Debug, PartialEq)]
pub struct VoidWebDriverExtensionRoute;

impl WebDriverExtensionRoute for VoidWebDriverExtensionRoute {
    type Command = VoidWebDriverExtensionCommand;

    fn command(
        &self,
        _: &Captures,
        _: &Value,
    ) -> WebDriverResult<WebDriverCommand<VoidWebDriverExtensionCommand>> {
        panic!("No extensions implemented");
    }
}

#[derive(Clone, Debug)]
struct RequestMatcher<U: WebDriverExtensionRoute> {
    method: Method,
    path_regexp: Regex,
    match_type: Route<U>,
}

impl<U: WebDriverExtensionRoute> RequestMatcher<U> {
    pub fn new(method: Method, path: &str, match_type: Route<U>) -> RequestMatcher<U> {
        let path_regexp = RequestMatcher::<U>::compile_path(path);
        RequestMatcher {
            method,
            path_regexp,
            match_type,
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
                rv.push_str(&format!("(?P<{}>[^/]+)/", &component[1..component.len() - 1])[..]);
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

#[derive(Debug)]
pub struct WebDriverHttpApi<U: WebDriverExtensionRoute> {
    routes: Vec<(Method, RequestMatcher<U>)>,
}

impl<U: WebDriverExtensionRoute> WebDriverHttpApi<U> {
    pub fn new(extension_routes: &[(Method, &str, U)]) -> WebDriverHttpApi<U> {
        let mut rv = WebDriverHttpApi::<U> { routes: vec![] };
        debug!("Creating routes");
        for &(ref method, ref url, ref match_type) in standard_routes::<U>().iter() {
            rv.add(method.clone(), *url, (*match_type).clone());
        }
        for &(ref method, ref url, ref extension_route) in extension_routes.iter() {
            rv.add(
                method.clone(),
                *url,
                Route::Extension(extension_route.clone()),
            );
        }
        rv
    }

    fn add(&mut self, method: Method, path: &str, match_type: Route<U>) {
        let http_matcher = RequestMatcher::new(method.clone(), path, match_type);
        self.routes.push((method, http_matcher));
    }

    pub fn decode_request(
        &self,
        method: Method,
        path: &str,
        body: &str,
    ) -> WebDriverResult<WebDriverMessage<U>> {
        let mut error = ErrorStatus::UnknownPath;
        for &(ref match_method, ref matcher) in self.routes.iter() {
            if method == *match_method {
                let (method_match, captures) = matcher.get_match(method.clone(), path);
                if captures.is_some() {
                    if method_match {
                        return WebDriverMessage::from_http(
                            matcher.match_type.clone(),
                            &captures.unwrap(),
                            body,
                            method == Method::POST,
                        );
                    } else {
                        error = ErrorStatus::UnknownMethod;
                    }
                }
            }
        }
        Err(WebDriverError::new(
            error,
            format!("{} {} did not match a known command", method, path),
        ))
    }
}
