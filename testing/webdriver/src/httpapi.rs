use http::Method;
use serde_json::Value;

use crate::command::{VoidWebDriverExtensionCommand, WebDriverCommand, WebDriverExtensionCommand};
use crate::error::WebDriverResult;
use crate::Parameters;

pub(crate) fn standard_routes<U: WebDriverExtensionRoute>() -> Vec<(Method, &'static str, Route<U>)>
{
    return vec![
        (Method::POST, "/session", Route::NewSession),
        (Method::DELETE, "/session/{sessionId}", Route::DeleteSession),
        (Method::POST, "/session/{sessionId}/url", Route::Get),
        (
            Method::GET,
            "/session/{sessionId}/url",
            Route::GetCurrentUrl,
        ),
        (Method::POST, "/session/{sessionId}/back", Route::GoBack),
        (
            Method::POST,
            "/session/{sessionId}/forward",
            Route::GoForward,
        ),
        (Method::POST, "/session/{sessionId}/refresh", Route::Refresh),
        (Method::GET, "/session/{sessionId}/title", Route::GetTitle),
        (
            Method::GET,
            "/session/{sessionId}/source",
            Route::GetPageSource,
        ),
        (
            Method::GET,
            "/session/{sessionId}/window",
            Route::GetWindowHandle,
        ),
        (
            Method::GET,
            "/session/{sessionId}/window/handles",
            Route::GetWindowHandles,
        ),
        (
            Method::POST,
            "/session/{sessionId}/window/new",
            Route::NewWindow,
        ),
        (
            Method::DELETE,
            "/session/{sessionId}/window",
            Route::CloseWindow,
        ),
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
        (
            Method::POST,
            "/session/{sessionId}/window",
            Route::SwitchToWindow,
        ),
        (
            Method::POST,
            "/session/{sessionId}/frame",
            Route::SwitchToFrame,
        ),
        (
            Method::POST,
            "/session/{sessionId}/frame/parent",
            Route::SwitchToParentFrame,
        ),
        (
            Method::POST,
            "/session/{sessionId}/element",
            Route::FindElement,
        ),
        (
            Method::POST,
            "/session/{sessionId}/elements",
            Route::FindElements,
        ),
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
        (
            Method::GET,
            "/session/{sessionId}/cookie",
            Route::GetCookies,
        ),
        (
            Method::GET,
            "/session/{sessionId}/cookie/{name}",
            Route::GetNamedCookie,
        ),
        (
            Method::POST,
            "/session/{sessionId}/cookie",
            Route::AddCookie,
        ),
        (
            Method::DELETE,
            "/session/{sessionId}/cookie",
            Route::DeleteCookies,
        ),
        (
            Method::DELETE,
            "/session/{sessionId}/cookie/{name}",
            Route::DeleteCookie,
        ),
        (
            Method::GET,
            "/session/{sessionId}/timeouts",
            Route::GetTimeouts,
        ),
        (
            Method::POST,
            "/session/{sessionId}/timeouts",
            Route::SetTimeouts,
        ),
        (
            Method::POST,
            "/session/{sessionId}/element/{elementId}/click",
            Route::ElementClick,
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
        (
            Method::GET,
            "/session/{sessionId}/alert/text",
            Route::GetAlertText,
        ),
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
        (
            Method::POST,
            "/session/{sessionId}/actions",
            Route::PerformActions,
        ),
        (
            Method::DELETE,
            "/session/{sessionId}/actions",
            Route::ReleaseActions,
        ),
        (Method::POST, "/session/{sessionId}/print", Route::Print),
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
    NewWindow,
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
    Print,
    Status,
    Extension(U),
}

pub trait WebDriverExtensionRoute: Clone + Send + PartialEq {
    type Command: WebDriverExtensionCommand + 'static;

    fn command(
        &self,
        _: &Parameters,
        _: &Value,
    ) -> WebDriverResult<WebDriverCommand<Self::Command>>;
}

#[derive(Clone, Debug, PartialEq)]
pub struct VoidWebDriverExtensionRoute;

impl WebDriverExtensionRoute for VoidWebDriverExtensionRoute {
    type Command = VoidWebDriverExtensionCommand;

    fn command(
        &self,
        _: &Parameters,
        _: &Value,
    ) -> WebDriverResult<WebDriverCommand<VoidWebDriverExtensionCommand>> {
        panic!("No extensions implemented");
    }
}
