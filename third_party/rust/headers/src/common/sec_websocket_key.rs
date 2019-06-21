/// The `Sec-Websocket-Key` header.
#[derive(Clone, Debug, PartialEq, Eq, Hash, Header)]
pub struct SecWebsocketKey(pub(super) ::HeaderValue);


