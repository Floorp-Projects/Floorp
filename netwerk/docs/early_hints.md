# Early Hints

[Early Hints](https://html.spec.whatwg.org/multipage/semantics.html#early-hints) is an informational HTTP status code allowing server to send headers likely to appear in the final response before sending the final response.
This is used to send [Link headers](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Link) to start `preconnect`s and `preload`s.

This document is about the implementation details of Early Hints in Firefox.
We focus on the `preload` feature, as it is the main feature interacting with classes.
For Early Hint `preconnect` the Early Hints specific code is rather small and only touches the code path on [`103 Early Hints` responses](#early-hints-response-on-main-document-load).

```{mermaid}
sequenceDiagram
    participant F as Firefox
    participant S as Server
    autonumber
    F->>+S: Main document Request: GET /
    S-->>F: 103 Early Hints Response
    note over F: Firefox starts<br/>hinted requests
    note over S: Server Think Time
    S->>-F: 200 OK final response
```

Early Hints benefits originate from leveraging Server Think Time.
The duration between response (2) and (3) arriving is the theoretical maximal benefit Early Hints can have.
The server think time can originate from creating dynamic content by interacting with databases or more commonly when proxying the request to a different server.

```{contents}
:local:
:depth: 1
```

## `103 Early Hints` Response on Main Document Load

On `103 Early Hints` response the `nsHttpChannel` handling main document load passes the link header and a few more from the `103 Early Hints` response to the `EarlyHintsService`

When receiving a `103 Early Hints` response, the `nsHttpChannel` forwards the `Link` headers in the `103 Early Hints` response to the `EarlyHintsService`
When the `DocumentLoadListener` receives a cross-origin redirect, it cancels all preloads in progress.

```{note}
Only the first `103 Early Hints` response is processed.
The remaining `103 Early Hints` responses are ignored, even after same-origin redirects.
When we receive cross origin redirects, all ongoing Early Hint preload requests are cancelled.
```

```{mermaid}
graph TD
    MainChannel[nsHttpChannel]
    EHS[EarlyHintsService]
    EHC[EarlyHintPreconnect]
    EHP[EarlyHintPreloader]
    PreloadChannel[nsIChannel]
    PCL[ParentChannelListener]

    MainChannel
      -- "nsIEarlyHintsObserver::EarlyHint(LinkHeader, Csp, RefererPolicy)<br/>via DocumentLoadListener"
      --> EHS
    EHS
        -- "rel=preconnect"
        --> EHC
    EHS -->|"rel=preload<br/>via OngoingEarlyHints"| EHP
    EHP -->|"CSP checks then AsyncOpen"| PreloadChannel
    PreloadChannel -->|mListener| PCL
    PCL -->|mNextListener| EHP
```

## Main document Final Response

On the final response the `DocumentLoadListener` retrieves the list of link headers from the `EarlyHintsService`.
As a side effect, the `EarlyHintPreloader` also starts a 10s timer to cancel itself if the content process doesn't connect to the `EarlyHintPreloader`.
The timeout shouldn't occur in normal circumstances, because the content process connects to that `EarlyHintPreloader` immediately.
The timeout currently only occurs when:

- the main response has different CSP requirements disallowing the load ([Bug 1815884](https://bugzilla.mozilla.org/show_bug.cgi?id=1815884)),
- the main response has COEP headers disallowing the load ([Bug 1806403](https://bugzilla.mozilla.org/show_bug.cgi?id=1806403)),
- the user reloads a website and the image/css is already in the image/css-cache ([Bug 1815884](https://bugzilla.mozilla.org/show_bug.cgi?id=1815884)),
- the tab gets closed before the connect happens or possibly other corner cases.

```{mermaid}
graph TD
    DLL[DocumentLoadListener]
    EHP[EarlyHintPreloader]
    PS[PreloadService]
    EHR[EarlyHintsRegistrar]
    Timer[nsITimer]

    DLL
        -- "(1)<br/>GetAllPreloads(newCspRequirements)<br/> via EarlyHintsService and OngoingEarlyHints"
        --> EHP
    EHP -->|"Start timer to cancel on<br/>ParentConnectTimeout<br/>after 10s"| Timer
    EHP -->|"Register(earlyHintPreloaderId)"| EHR
    Timer -->|"RefPtr"| EHP
    EHR -->|"RefPtr"| EHP
    DLL
        -- "(2)<br/>Send to content process via IPC<br/>List of Links+earlyHintPreloaderId"
        --> PS
```

## Preload request from Content process

The Child process parses Link headers from the `103 Early Hints` response first and then from the main document response.
Preloads from the Link headers of the `103 Early Hints` response have an `earlyHintPreloadId` assigned to them.
The Preloader sets this `earlyHintPreloaderId` on the channel doing the preload before calling `AsyncOpen`.
The `HttpChannelParent` looks for the `earlyHintPreloaderId` in `AsyncOpen` and connects to the `EarlyHintPreloader` via the `EarlyHintRegistrar` instead of doing a network request.

```{mermaid}
graph TD
    PS[PreloadService]
    Preloader["FetchPreloader<br/>FontPreloader<br/>imgLoader<br/>ScriptLoader<br/>StyleLoader"]
    Parent["HttpChannelParent"]
    EHR["EarlyHintRegistrar"]
    EHP["EarlyHintPreloader"]

    PS -- "PreloadLinkHeader" --> Preloader
    Preloader -- "NewChannel<br/>SetEarlyHintPreloaderId<br/>AsyncOpen" --> Parent
    Parent -- "EarlyHintRegistrar::OnParentReady(this, earlyHintPreloaderId)" --> EHR
    EHR -- "OnParentConnect" --> EHP
```

## Early Hint Preload request

The `EarlyHintPreloader` follows HTTP 3xx redirects and always sets the request header `X-Moz: early hint`.

## Early Hint Preload response

When the `EarlyHintPreloader` received the `OnStartRequest` it forwards all `nsIRequestObserver` functions to the `HttpChannelParent` as soon as it knows which `HttpChannelParent` to forward the `nsIRequestObserver` functions to.

```{mermaid}
graph TD
    OPC["EHP::OnParentConnect"]
    OSR["EHP::OnStartRequest"]
    Invoke["Invoke StreamListenerFunctions"]
    End(("&shy;"))

    OPC -- "CancelTimer" --> Invoke
    OSR -- "Suspend Channel if called<br/>before OnParentReady" --> Invoke
    Invoke -- "Resume Channel if suspended<br/>Forward OSR+ODA+OSR<br/>Set Listener of ParentChanelListener to HttpChannelParent" --> End
```

## Final setup

In the end all the remaining `OnDataAvailable` and `OnStopRequest` calls are passed down this call chain from `nsIChannel` to the preloader.

```{mermaid}
graph TD
    Channel[nsIChannel]
    PCL[ParentChannelListener]
    HCP[HttpChanelParent]
    HCC[HttpChannelChild]
    Preloader[FetchPreloader/imgLoader/...]

    Channel -- "mListener" --> PCL
    PCL -- "mNextListener" --> HCP
    HCP -- "mChannel" --> Channel
    HCP -- "..." --> HCC
    HCC -- "..." --> HCP
    HCC -- "mListener" --> Preloader
    Preloader -- "mChannel" --> HCC
```
