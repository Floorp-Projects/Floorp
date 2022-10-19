# WebTransportSessionProxy

WebTransportSessionProxy is introduced to enable the creation of a Http3WebTransportSession and coordination of actions that are performed on the main thread and on the socket thread.

WebTransportSessionProxy can be in different states and the following diagram depicts the transition between the states. “MT” and “ST” mean: the action is happening on the main and socket thread. More details about this class can be found in [WebTransportSessionProxy.h](https://searchfox.org/mozilla-central/source/netwerk/protocol/webtransport/WebTransportSessionProxy.h).

```{mermaid}
graph TD
    A[INIT] -->|"nsIWebTransport::AsyncConnect; MT"| B[NEGOTIATING]
    B -->|"200 response; ST"| C[NEGOTIATING_SUCCEEDED]
    B -->|"nsHttpChannel::OnStart/OnStop failed; MT"| D[DONE]
    B -->|"nsIWebTransport::CloseSession; MT"| D
    C -->|"nsHttpChannel::OnStart/OnStop failed; MT"| F[SESSION_CLOSE_PENDING]
    C -->|"nsHttpChannel::OnStart/OnStop succeeded; MT"| E[ACTIVE]
    E -->|"nsIWebTransport::CloseSession; MT"| F
    E -->|"The peer closed the session or HTTP/3 connection error; ST"| G[CLOSE_CALLBACK_PENDING]
    F -->|"CloseSessionInternal called, The peer closed the session or HTTP/3 connection error; ST"| D
    G -->|"CallOnSessionClosed or nsIWebTransport::CloseSession; MT"| D
```
