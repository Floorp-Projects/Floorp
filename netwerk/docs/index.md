# Networking

The Necko (aka Networking) component is Gecko's implementation of the web's networking protocols.
Most of the component's source lives in `netwerk` directory and this document's source can be found in `netwerk/docs`.
This page mostly just points to helpful resources for contributing to and understanding Necko.
More details can be found on [Necko's wiki](https://wiki.mozilla.org/Networking).
The team can be reached:
* on Matrix: [#necko:mozilla.org](https://chat.mozilla.org/#/room/#necko:mozilla.org)
* by email: necko@mozilla.com
* or by submitting a `Core::Networking` bug on [bugzilla.mozilla.org](https://bugzilla.mozilla.org/enter_bug.cgi?product=Core&component=Networking)

## Contributing to Necko
```{toctree}
:maxdepth: 1
http/logging
submitting_networking_bugs.md
```

## Team resources
```{toctree}
:maxdepth: 1
new_to_necko_resources
neqo_triage_guideline.md
```

## Testing
```{toctree}
:maxdepth: 1
network_test_guidelines.md
http_server_for_testing
mochitest_with_http3.md
```

## Component/Feature details
```{toctree}
:maxdepth: 1
http/lifecycle
sec-necko-components.md
cache2/doc
http/http3.md
dns/dns-over-https-trr
url_parsers.md
webtransport/webtransport
captive_portals.md
early_hints.md
```
