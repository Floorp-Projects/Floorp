# Tower Service

The foundational `Service` trait that Tower is based on.

## Overview

The [`Service`] trait provides the foundation upon which Tower is built. It is a
simple, but powerful trait. At its heart, `Service` is just an asynchronous
function of request to response.

```
async fn(Request) -> Result<Response, Error>
```

Implementations of `Service` take a request, the type of which varies per
protocol, and returns a future representing the eventual completion or failure
of the response.

Services are used to represent both clients and servers. An *instance* of
`Service` is used through a client; a server *implements* `Service`.

By using standardizing the interface, middleware can be created. Middleware
*implement* `Service` by passing the request to another `Service`. The
middleware may take actions such as modify the request.

[`Service`]: https://docs.rs/tower-service/latest/tower_service/trait.Service.html

## License

This project is licensed under the [MIT license](LICENSE).

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in Tower by you, shall be licensed as MIT, without any additional
terms or conditions.
