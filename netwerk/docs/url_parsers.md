# URL parsers

```{warning}
In order to ensure thread safety it is important that all of the objects and interfaces of URI objects are immutable.
If you are implementing a new URI type, please make sure that none of the type's public methods change the URL.
```

###  Definitions
- URI - Uniform Resource Identifier
- URL - Uniform Resource Locator

These two terms are used interchangeably throughout the codebase and essentially represent the same thing - a string of characters that represents a specific resource.

### Motivation

While we could simply pass strings around and leave it to the final consumer to deal with it, that creates a burden for the consumer and would probably be inefficient. Instead we parse the string into a nsIURI object as soon as possible and pass that object through function calls. This allows the consumer to easily extract only the part of the string they are interested in (eg. the hostname or the path).

## Interfaces
- [nsIURI](https://searchfox.org/mozilla-central/source/netwerk/base/nsIURI.idl)
    - This is the most important interface for URI parsing. It contains a series of readonly attributes that consumers can use to extract information from the URI.
- [nsIURL](https://searchfox.org/mozilla-central/source/netwerk/base/nsIURL.idl)
    - Defines a structure for the URI's path (directory, fileName, fileBaseName, fileExtension)
- [nsIFileURL](https://searchfox.org/mozilla-central/source/netwerk/base/nsIFileURL.idl)
    - Has a file attribute of type `nsIFile`
    - Used for local protocols to access the file represented by the `nsIURI`
- [nsIMozIconURI](https://searchfox.org/mozilla-central/source/image/nsIIconURI.idl)
    - Used to represent an icon. Contains additional attributes such as the size and contentType or state of the URL.
- [nsIJARURI](https://searchfox.org/mozilla-central/source/modules/libjar/nsIJARURI.idl)
    - Used to represent a resource inside of a JAR (zip archive) file.
    - For example `jar:http://www.example.com/blue.jar!/ocean.html` represents the `/ocean.html` resource located inside the `blue.jar` archive that can be fetched via HTTP from example.com.
- [nsIStandardURL](https://searchfox.org/mozilla-central/source/netwerk/base/nsIStandardURL.idl)
    - Defines a few constant flags used to determine the type of the URL. No other attributes.
- [nsINestedURI](https://searchfox.org/mozilla-central/source/netwerk/base/nsINestedURI.idl)
    - Defines `innerURI` and `innermostURI`.
    - `innermostURI` is just a helper - one could also get it by going through `innerURI` repeatedly until the attribute no longer QIs to nsINestedURI.
- [nsISensitiveInfoHiddenURI](https://searchfox.org/mozilla-central/source/netwerk/base/nsISensitiveInfoHiddenURI.idl)
    - Objects that implement this interface will have a `getSensitiveInfoHiddenSpec()` method that returns the spec of the URI with sensitive info (such as the password) replaced by the `*` symbol.

### Diagram of interfaces
```{mermaid}
classDiagram  
nsISupports <-- nsIURI
nsIURI <-- nsIURL
nsIURL <-- nsIFileURL
nsIURI <-- nsIMozIconURI
nsIURL <-- nsIJARURI
nsISupports <-- nsIStandardURL
nsISupports <-- nsINestedURI
nsISupports <-- nsISensitiveInfoHiddenURI
```

### Mutation

To ensure thread safety all implementations of nsIURI must be immutable.
To change a URI the consumer must call `nsIURI.mutate()` which returns a `nsIMutator`. The `nsIMutator` has several setter methods that can be used change attributes on the concrete object. Once done changing the object, the consumer will call `nsIMutator.finalize()` to obtain an immutable `nsIURI`.

- [nsIURIMutator](https://searchfox.org/mozilla-central/source/netwerk/base/nsIURIMutator.idl)
    - This interface contains a series of setters that can be used to mutate and/or construct a `nsIURI`


### Additional interfaces

- [nsISerializable](https://searchfox.org/mozilla-central/source/xpcom/ds/nsISerializable.idl)
    - Allows us to serialize and deserialize URL objects into strings for persistent storage (such as session restore).

## Implementations
- [nsStandardURL](https://searchfox.org/mozilla-central/source/netwerk/base/nsStandardURL.h)
- [SubstitutingURL](https://searchfox.org/mozilla-central/source/netwerk/protocol/res/SubstitutingURL.h)
    - overrides nsStandardURL::GetFile to provide nsIFile resolution.
    - This allows us to map URLs such as `resource://gre/actors/RemotePageChild.jsm` to the actual file on the disk.
- [nsMozIconURI](https://searchfox.org/mozilla-central/source/image/decoders/icon/nsIconURI.h)
    - Used to represent icon URLs
- [nsSimpleURI](https://searchfox.org/mozilla-central/source/netwerk/base/nsSimpleURI.h)
    - Used for simple URIs that normally don't have an authority (username, password, host, port)
- [nsSimpleNestedURI](https://searchfox.org/mozilla-central/source/netwerk/base/nsSimpleNestedURI.h)
    - eg. `view-source:http://example.com/path`
    - Normally only the extra scheme of the nestedURI is relevant (eg. `view-source:`)
    - Most of the getter/setters are delegated to the innerURI
- [nsNestedAboutURI](https://searchfox.org/mozilla-central/source/netwerk/protocol/about/nsAboutProtocolHandler.h)
    - Similar to nsSimpleNestedURI, but has an extra `mBaseURI` member that allows us to propagate the base URI to about:blank correctly`
- [BlobURL](https://searchfox.org/mozilla-central/source/dom/file/uri/BlobURL.h)
    - Used for javascript blobs
    - Similar to nsSimpleURI, but also has a revoked field.
- [DefaultURI](https://searchfox.org/mozilla-central/source/netwerk/base/DefaultURI.h)
    - This class wraps an object parsed by the `rust-url` crate.
    - While not yet enabled by default, due to small bugs in that parser, the plan is to eventually use this implementation for all _unknown protocols_ that don't have their own URL parser.
- [nsJSURI](https://searchfox.org/mozilla-central/source/dom/jsurl/nsJSProtocolHandler.h)
    - Used to represent javascript code (eg. `javascript:alert('hello')`)
- [nsJARURI](https://searchfox.org/mozilla-central/source/modules/libjar/nsJARURI.h)
    - Used to represent resources inside of JAR files.

### Diagram of implementations

```{mermaid}
classDiagram
nsSimpleURI o-- BlobURL
nsIMozIconURI o-- nsMozIconURI
nsIFileURL o-- nsStandardURL
nsIStandardURL o-- nsStandardURL
nsISensitiveInfoHiddenURI o-- nsStandardURL
nsStandardURL o-- SubstitutingURL
nsIURI o-- nsSimpleURI
nsSimpleURI o-- nsSimpleNestedURI
nsSimpleNestedURI o-- nsNestedAboutURI

nsIURI o-- DefaultURI

nsSimpleURI o-- nsJSURI

nsINestedURI o-- nsJARURI
nsIJARURI o-- nsJARURI
```

## Class and interface diagram

```{mermaid}
classDiagram  
nsISupports <-- nsIURI
nsIURI <-- nsIURL
nsIURL <-- nsIFileURL
nsIURI <-- nsIMozIconURI
nsIURL <-- nsIJARURI
nsISupports <-- nsIStandardURL
nsISupports <-- nsINestedURI
nsISupports <-- nsISensitiveInfoHiddenURI

%% classes

nsSimpleURI o-- BlobURL
nsSimpleURI o-- nsJSURI
nsIMozIconURI o-- nsMozIconURI
nsIFileURL o-- nsStandardURL
nsIStandardURL o-- nsStandardURL
nsISensitiveInfoHiddenURI o-- nsStandardURL
nsStandardURL o-- SubstitutingURL
nsIURI o-- nsSimpleURI
nsINestedURI o-- nsJARURI
nsIJARURI o-- nsJARURI
nsSimpleURI o-- nsSimpleNestedURI
nsSimpleNestedURI o-- nsNestedAboutURI
nsIURI o-- DefaultURI

```
