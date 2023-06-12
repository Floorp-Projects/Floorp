# Security and Networking Components

This diagram models a high-level call flow upon performing an asyncOpen on an nsHttpChannel down into the NSS layer for a typical resource load.

## Necko
1. The LoadInfo, which contains [security related info](https://searchfox.org/mozilla-central/rev/27e4816536c891d85d63695025f2549fd7976392/netwerk/base/LoadInfo.h#284-294),
 is passed to the channel (nsHttpChannel) on the parent process.
2. The channel creates a transaction and the nsHttpConnectionMgr on the socket thread is signalled to handle the transaction.
3. The transaction is then picked up on the socket thread and "dispatched" to a new or existing ConnectionEntry that is hashed by it's ConnectionInfo.
4. The underlying connection, nsHttpConnection for Http/1.1 and Http/2 and HttpConnectionUDP for Http/3, will call into NSS for security functionality.

## NSS
Necko interacts with NSS through two distinct interfaces.
 Primarily, most access flows via PSM which handles the configuration of TLS sockets, client certificate selection and server certificate verification.
 However, Neqo (Mozilla's QUIC library) also relies directly on the TLS implementation inside NSS and uses it as an interface directly.

NSS's internal structure is fairly convoluted, but there are five main areas relevant for Necko. Starting from the lowest level:
1. [blapi.h](https://searchfox.org/mozilla-central/source/security/nss/lib/freebl/blapi.h) - exposes the wrappers for each cryptographic primitive supported by NSS and dispatches them to platform specific implementations.
2. [pkcs11c.c](https://searchfox.org/mozilla-central/source/security/nss/lib/softoken/pkcs11c.c) - This wraps those underlying crypto primitives to provide a PKCS11 interface as a single module.
3. [pk11pub.h](https://searchfox.org/mozilla-central/source/security/nss/lib/pk11wrap/pk11pub.h) - This wraps any module providing a PKCS11 interface and exposes high level cryptographic operations. It is widely used across Firefox.
4. [ssl.h](https://searchfox.org/mozilla-central/source/security/nss/lib/ssl/ssl.h) and [sslexp.h](https://searchfox.org/mozilla-central/source/security/nss/lib/ssl/sslexp.h) expose our TLS interface for use in Necko's TLS and Neqo's QUIC connections.
5. [cert.h](https://searchfox.org/mozilla-central/source/security/nss/lib/certdb/cert.h) exposes the certificate database functionality. [pkix.h](https://searchfox.org/mozilla-central/source/security/nss/lib/mozpkix/include/pkix/pkix.h) exposes the MozPkix certificate chain validation functions.


```{mermaid}
classDiagram

class LoadInfo{
    +Principal(s) (loading, triggering, toInherit)
    +Context
}

nsHttpChannel --> nsHttpTransaction
nsHttpTransaction --> nsHttpConnectionMgr
nsHttpConnectionMgr --> ConnectionEntry : Via ConnectionInfo hash
ConnectionEntry --> HttpConnectionBase

HttpConnectionBase <-- nsHttpConnection : Is A
HttpConnectionBase <-- HttpConnectionUDP : Is A

nsHttpConnection --> nsSocketTransport2
nsSocketTransport2 --> PSM
PSM --> NSPR
PSM --> `Off Main Thread CertVerifier`
Neqo --> `Off Main Thread CertVerifier`

%% for Http/3
HttpConnectionUDP --> Http3Session : Http/3
HttpConnectionUDP --> nsUDPSocket : Http/3
nsUDPSocket --> NSPR : Http/3
Http3Session --> Neqo : Http/3

%% security TCP stack
PSM --> TLS
`Off Main Thread CertVerifier` --> Pcks11
TLS --> Pcks11
Pcks11 --> Blapi
Blapi --> `Crypto Primitives`
`Crypto Primitives` --> `Platform-Specific Crypto Implementations`

%% transport security info
PSM -- Transport Security Info
Transport Security Info --> nsHttpChannel

%% security UDP stack
Neqo --> TLS
`Off Main Thread CertVerifier`--> CertDB
CertDB --> Builtins


%% classes

nsHttpChannel o-- LoadInfo
nsHttpChannel o-- StreamListener
nsHttpConnectionMgr o-- ConnectionEntry : Many

```
