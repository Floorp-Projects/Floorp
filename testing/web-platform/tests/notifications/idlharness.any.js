// META: global=window,worker
// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js

'use strict';

// https://notifications.spec.whatwg.org/

idl_test(
  ['notifications'],
  ['service-workers', 'html', 'dom'],
  idl_array => {
    idl_array.add_objects({
      Notification: ['notification'],
      NotificationEvent: ['notificationEvent'],
    });
    if (self.ServiceWorkerGlobalScope) {
      idl_array.add_objects({ServiceWorkerGlobalScope: ['self']});
    }

    self.notification = new Notification("Running idlharness.");
    self.notificationEvent = new NotificationEvent("Running idlharness.");
  }
);
