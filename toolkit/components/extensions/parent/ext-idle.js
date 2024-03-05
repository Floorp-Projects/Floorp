/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "idleService",
  "@mozilla.org/widget/useridleservice;1",
  "nsIUserIdleService"
);

var { DefaultWeakMap } = ExtensionUtils;

// WeakMap[Extension -> Object]
const idleObserversMap = new DefaultWeakMap(() => {
  return {
    observer: null,
    detectionInterval: 60,
  };
});

const getIdleObserver = extension => {
  let observerInfo = idleObserversMap.get(extension);
  let { observer, detectionInterval } = observerInfo;
  let interval =
    extension.startupData?.idleDetectionInterval || detectionInterval;

  if (!observer) {
    observer = new (class extends ExtensionCommon.EventEmitter {
      observe(subject, topic) {
        if (topic == "idle" || topic == "active") {
          this.emit("stateChanged", topic);
        }
      }
    })();
    idleService.addIdleObserver(observer, interval);
    observerInfo.observer = observer;
    observerInfo.detectionInterval = interval;
  }
  return observer;
};

this.idle = class extends ExtensionAPIPersistent {
  PERSISTENT_EVENTS = {
    onStateChanged({ fire }) {
      let { extension } = this;
      let listener = (event, data) => {
        fire.sync(data);
      };

      getIdleObserver(extension).on("stateChanged", listener);
      return {
        async unregister() {
          let observerInfo = idleObserversMap.get(extension);
          let { observer, detectionInterval } = observerInfo;
          if (observer) {
            observer.off("stateChanged", listener);
            if (!observer.has("stateChanged")) {
              idleService.removeIdleObserver(observer, detectionInterval);
              observerInfo.observer = null;
            }
          }
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
  };

  getAPI(context) {
    let { extension } = context;
    let self = this;

    return {
      idle: {
        queryState(detectionIntervalInSeconds) {
          if (idleService.idleTime < detectionIntervalInSeconds * 1000) {
            return "active";
          }
          return "idle";
        },
        setDetectionInterval(detectionIntervalInSeconds) {
          let observerInfo = idleObserversMap.get(extension);
          let { observer, detectionInterval } = observerInfo;
          if (detectionInterval == detectionIntervalInSeconds) {
            return;
          }
          if (observer) {
            idleService.removeIdleObserver(observer, detectionInterval);
            idleService.addIdleObserver(observer, detectionIntervalInSeconds);
          }
          observerInfo.detectionInterval = detectionIntervalInSeconds;
          // There is no great way to modify a persistent listener param, but we
          // need to keep this for the startup listener.
          if (!extension.persistentBackground) {
            extension.startupData.idleDetectionInterval =
              detectionIntervalInSeconds;
            extension.saveStartupData();
          }
        },
        onStateChanged: new EventManager({
          context,
          module: "idle",
          event: "onStateChanged",
          extensionApi: self,
        }).api(),
      },
    };
  }
};
