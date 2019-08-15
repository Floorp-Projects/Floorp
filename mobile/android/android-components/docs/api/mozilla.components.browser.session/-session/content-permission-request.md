[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [Session](index.md) / [contentPermissionRequest](./content-permission-request.md)

# contentPermissionRequest

`var contentPermissionRequest: `[`Consumable`](../../mozilla.components.support.base.observer/-consumable/index.md)`<`[`PermissionRequest`](../../mozilla.components.concept.engine.permission/-permission-request/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/Session.kt#L387)

[Consumable](../../mozilla.components.support.base.observer/-consumable/index.md) permission request from web content. A [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md)
must be consumed i.e. either [PermissionRequest.grant](../../mozilla.components.concept.engine.permission/-permission-request/grant.md) or
[PermissionRequest.reject](../../mozilla.components.concept.engine.permission/-permission-request/reject.md) must be called. A content permission request
can also be cancelled, which will result in a new empty [Consumable](../../mozilla.components.support.base.observer/-consumable/index.md).

**Getter**

[Consumable](../../mozilla.components.support.base.observer/-consumable/index.md) permission request from web content. A [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md)
must be consumed i.e. either [PermissionRequest.grant](../../mozilla.components.concept.engine.permission/-permission-request/grant.md) or
[PermissionRequest.reject](../../mozilla.components.concept.engine.permission/-permission-request/reject.md) must be called. A content permission request
can also be cancelled, which will result in a new empty [Consumable](../../mozilla.components.support.base.observer/-consumable/index.md).

**Setter**

[Consumable](../../mozilla.components.support.base.observer/-consumable/index.md) permission request from web content. A [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md)
must be consumed i.e. either [PermissionRequest.grant](../../mozilla.components.concept.engine.permission/-permission-request/grant.md) or
[PermissionRequest.reject](../../mozilla.components.concept.engine.permission/-permission-request/reject.md) must be called. A content permission request
can also be cancelled, which will result in a new empty [Consumable](../../mozilla.components.support.base.observer/-consumable/index.md).

