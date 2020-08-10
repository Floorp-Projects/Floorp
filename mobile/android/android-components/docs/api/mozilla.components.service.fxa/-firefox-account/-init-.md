[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FirefoxAccount](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`FirefoxAccount(config: `[`ServerConfig`](../-server-config.md)`, persistCallback: `[`PersistCallback`](../-persist-callback.md)`? = null)`

Construct a FirefoxAccount from a [Config](#), a clientId, and a redirectUri.

### Parameters

`persistCallback` -

This callback will be called every time the [FirefoxAccount](index.md)
internal state has mutated.
The FirefoxAccount instance can be later restored using the
[FirefoxAccount.fromJSONString](from-j-s-o-n-string.md)` class method.
It is the responsibility of the consumer to ensure the persisted data
is saved in a secure location, as it can contain Sync Keys and
OAuth tokens.



Note that it is not necessary to `close` the Config if this constructor is used (however
doing so will not cause an error).

