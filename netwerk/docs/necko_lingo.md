# Necko Lingo<!-- omit from toc -->
 Words We Keep Throwing Around Like Internet Confetti!

- __B__
    - [Background Thread](#background-thread)
- __C__
    - [Channel](#channel)
    - [Child Process](#child-process)
  - [Content Process](#content-process)
- __D__
    - [DoH](#doh)
- __E__
    - [Eventsource](#eventsource)
    - [Electrolysis](#electrolysis)
- __F__
    - [Fetch](#fetch)
    - [Fetch API](#fetch-api)
    - [Fission](#fission)
- __H__
    - [H1/H2/H3](#h1h2h3)
    - [Happy eyeballs](#happy-eyeballs)
    - [HSTS](#hsts)
- __L__
    - [LoadInfo](#loadinfo)
    - [Listener](#listener)
- __M__
    - [MainThread](#mainthread)
    - [Mochitest](#mochitest)
- __N__
    - [neqo v/s Necko](#neqo-vs-necko)
    - [NSS](#nss)
    - [NSPR](#nspr)
- __P__
    - [PSM](#psm)
    - [Parent Process](#parent-process)
  - [Principal](#principal)
- __O__
    - [OMT](#omt)
    - [OnStartRequest/OnDataAvailable/OnStopRequest/](#onstartrequestondataavailableonstoprequest)
- __Q__
    - [QUIC](#quic)
- __R__
    - [RCWN](#rcwn)
- __S__
    - [Socket Process](#socket-process)
    - [Socket Thread](#socket-thread)
    - [SOCKS Proxy](#socks-proxy)
- __T__
    - [TLS](#tls)
    - [Triage](#triage)
    - [TRR](#trr)
- __W__
    - [WebSocket](#websocket)
    - [WebTransport](#webtransport)
- __X__
    - [Xpcshell-tests](#xpcshell-tests)
    - [XHR](#xhr)




## Background Thread
Any thread that is not main thread.\
Or this thread created [here](https://searchfox.org/mozilla-central/rev/23e7e940337d0e0b29aabe0080e4992d3860c940/ipc/glue/BackgroundImpl.cpp#880) for PBackground for IPC\
Usually threads either have a dedicated name but background threads might also refer to the background thread pool: [NS_DispatchBackgroundTask](https://searchfox.org/mozilla-central/rev/23e7e940337d0e0b29aabe0080e4992d3860c940/xpcom/threads/nsThreadUtils.cpp#516).

## Channel
See [nsIChannel.idl](https://searchfox.org/mozilla-central/rev/bc6a50e6f08db0bb371ef7197c472555499e82c0/netwerk/base/nsIChannel.idl).
It usually means nsHttpChannel.

## Child Process
Usually a firefox forked process - not the main process.\
See [GeckoProcessTypes.h](https://searchfox.org/mozilla-central/source/__GENERATED__/xpcom/build/GeckoProcessTypes.h) for all process types in gecko.

## Content Process
Usually a firefox forked process running untrusted web content.

## DoH
[DNS](https://developer.mozilla.org/en-US/docs/Glossary/DNS) over HTTPS.\
Refer [RFC 8484 - DNS Queries over HTTPS (DoH)](https://datatracker.ietf.org/doc/html/rfc8484).\
Resolves DNS names by using a HTTPS server.\
Refer [this link](https://firefox-source-docs.mozilla.org/networking/dns/dns-over-https-trr.html) for more details.

## Eventsource
Web API for [server-sent-events](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events). \
Refer [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/API/EventSource) for more details. \
Necko is responsible for maintaining part of this code along with DOM.



## Electrolysis
Also known as E10S (E + 10 chars + S).\
The process to make web content run in its own process.\
Extended by the ‘Fission’ project, which introduced isolation for sites (really [eTLD+1’s](https://developer.mozilla.org/en-US/docs/Glossary/eTLD)).\
Refer wiki [page](https://wiki.mozilla.org/Electrolysis) for more details.

## Fetch
[Fetch](https://fetch.spec.whatwg.org/) standard [aims](https://fetch.spec.whatwg.org/#goals) specifies standard for fetching resources across web. \
Fetch and [Fetch API](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API) slightly different things.

## Fetch API
[Web API](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API) for fetching resources from the web.
The code is jointly maintained by DOM team and Necko team

## Fission
Similar to Electrolysis, but different domains ([eTLD+1’s](https://developer.mozilla.org/en-US/docs/Glossary/eTLD)) get their own content process to avoid [Spectre attacks](https://meltdownattack.com/). \
Max 4 processes per [eTLD+1](https://developer.mozilla.org/en-US/docs/Glossary/eTLD). \
Iframes get isolated from the parent. \
Also referred to as origin isolation.

## H1/H2/H3
HTTP version: 0.9 / 1.0 / 1.1 / 2 / 3.

## Happy Eyeballs
RFC 6555/8305 – connecting via IPv4 and IPv6 simultaneously. \
We implement this in a [different](https://searchfox.org/mozilla-central/rev/23e7e940337d0e0b29aabe0080e4992d3860c940/netwerk/protocol/http/DnsAndConnectSocket.cpp#202-206) way.

## HSTS
HTTP [Strict Transport Security](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Strict-Transport-Security). \
HSTS preload - a list of websites that will be upgraded to HTTPS without first needing a response.


## LoadInfo
Object containing information about about the load (who triggered the load, in which context, etc).
Refer [nsILoadContext](https://searchfox.org/mozilla-central/source/netwerk/base/nsILoadContextInfo.idl) for more details.


## Listener
In async programming, we usually trigger an action, and set a listener to receive the result. \
In necko it’s most usually referring to the listener of a channel, which is an object implementing [nsIStreamListener](https://searchfox.org/mozilla-central/rev/bc6a50e6f08db0bb371ef7197c472555499e82c0/netwerk/base/nsIStreamListener.idl) and/or [nsIRequestObserver](https://searchfox.org/mozilla-central/rev/bc6a50e6f08db0bb371ef7197c472555499e82c0/netwerk/base/nsIRequestObserver.idl#14).


## MainThread
Every process has a MainThread, which is also the master event loop for the process. \
Many things are scoped to run on MainThread (DispatchToMainThread, etc).\
For content processes, MainThread is where normal JS content runs ([Worker](https://developer.mozilla.org/en-US/docs/Web/API/Worker) JavaScript runs on DOM Worker threads).


## Mochitest
Browser tests, run with full UI. \
Refer [this](https://firefox-source-docs.mozilla.org/testing/mochitest-plain/index.html) for more details.

## neqo v/s Necko
_neqo_ is the name of the Mozilla [QUIC](https://github.com/mozilla/neqo) stack.
Sometimes pronounced “knee-ko” or “neck-ou”, “knee-q-oo”.

_Necko_ is the project name of the Mozilla networking stack.


## NSS
An acronym for the Network Security Services.
Refer  [this](https://firefox-source-docs.mozilla.org/security/nss/index.html) for more details.


## NSPR
An acronym for [Netscape Portable Runtime](https://firefox-source-docs.mozilla.org/nspr/index.html)
Necko’s scope?

## Observer
Terminology used for referring the classes implementing the Observer design pattern. \
Refer the following interfaces for more details:
- [nsIObserver](https://searchfox.org/mozilla-central/source/xpcom/ds/nsIObserver.idl)
- [nsIObserverService](https://searchfox.org/mozilla-central/source/xpcom/ds/nsIObserverService.idl)

## PSM
The PSM acronym may also be described as "Platform Security Module". \
Glue code between Gecko and NSS. \
Refer [this](https://wiki.mozilla.org/PSM:Topics) for more details.


## Parent Process
The process that runs the main structure of the browser, including the UI.
It spawns the other processes needed for the browser. \
Generally it is unrestricted, while most other processes have some level of sandboxing of permissions applied. \
Before e10s, all code ran in the Parent Process.

### Principal
Abstraction encapsulating security details of a web page.
Refer the following links for more details:
- [sec-necko-components](https://firefox-source-docs.mozilla.org/networking/sec-necko-components.html)
- [nsIPrincipal](https://searchfox.org/mozilla-central/source/caps/nsIPrincipal.idl)

## OMT
Abbreviation for Off Main Thread. OMT refers to processing data in non-main thread.
There has been efforts in the past to move the processing of data to non-main thread to free up main thread resources.


## OnStartRequest/OnDataAvailable/OnStopRequest/
- OnStartRequest is listener notification sent when the necko has parsed the status and the header.
- OnDataAvailable is a listener notification sent when necko has received the the data/body.
- OnStopRequest is a listener notification sent when necko has received the complete response.
- Refer to the following interface documentation for more details:
  - [nsIRequestObserver](https://searchfox.org/mozilla-central/source/netwerk/base/nsIRequestObserver.idl)
  - [nsIStreamListener.idl](https://searchfox.org/mozilla-central/source/netwerk/base/nsIStreamListener.idl)

## QUIC
An IETF transport protocol [RFC9000](https://datatracker.ietf.org/doc/html/rfc9000) primarily designed to carry HTTP/3, but now also used as a general-purpose Internet transport protocol for other workloads.
Implemented in RUST and maintained by [neqo](https://github.com/mozilla/neqo).

## RCWN
Race cache with network. Feature that will send a request to network and cache at the same time and take the first to resolve.

## Socket Process
WIP project to move the actions of the socket thread into its own process for the purposes of isolation for security and stability (during crashes).

## Socket Thread
From the main process, a thread that handles opening and reading from the sockets for network communication. \
We also use socket thread in content process for PHttpBackgroundChannel.ipdl

## SOCKS Proxy
Necko supports SOCKS proxy. \
Refer [RFC 1928](https://datatracker.ietf.org/doc/html/rfc1928) for more details.

## TLS
Transport Layer Security. It’s implementation is maintained by NSS team.

## Triage
Team process of bug intake, analysis and categorization.

## TRR
Trusted recursive resolver.This is the name of our DoH implementation, as well as the name of the program that ensures DoH providers included in Firefox have agreed not to spy on users. \
Refer the following for more details:
- [Trusted_Recursive_Resolver](https://wiki.mozilla.org/Trusted_Recursive_Resolver)
- [DOH-resolver-policy](https://wiki.mozilla.org/Security/DOH-resolver-policy)

## WebSocket
Server/client connections over TCP to pass data. A replacement for long-poll HTTP connection. \
Refer [RFC 6455](https://datatracker.ietf.org/doc/html/rfc6455) and [whatwg spec](https://websockets.spec.whatwg.org/).


## WebTransport
A mechanism similar to WebSockets to transfer data between server and client, but built for HTTP/3; can also run over HTTP/2 (being implemented in gecko). \
References:
- [MDN Documentation on WebTransport](https://developer.mozilla.org/en-US/docs/Web/API/WebTransport)
- [WebTransport RFC Draft](https://datatracker.ietf.org/doc/html/draft-ietf-webtrans-http3/)

## Xpcshell-tests
Unit tests for testing our XPCOM code from JS context.
Runs without a UI in a simpler  setup (typically single-process). \
Refer [firefox-source-docs](https://firefox-source-docs.mozilla.org/testing/xpcshell/index.html) for detailed explanation.

## XHR
[XMLHttpRequest](https://developer.mozilla.org/en-US/docs/Web/API/XMLHttpRequest). One way to perform AJAX, essentially a way to dynamically make network requests for use in the callers webpage, similar to Fetch.
