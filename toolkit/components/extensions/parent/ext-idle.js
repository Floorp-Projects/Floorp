"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "idleService",
                                   "@mozilla.org/widget/idleservice;1",
                                   "nsIIdleService");

// WeakMap[Extension -> Object]
let observersMap = new WeakMap();

const getIdleObserverInfo = (extension, context) => {
  let observerInfo = observersMap.get(extension);
  if (!observerInfo) {
    observerInfo = {
      observer: null,
      detectionInterval: 60,
    };
    observersMap.set(extension, observerInfo);
    context.callOnClose({
      close: () => {
        let {observer, detectionInterval} = observersMap.get(extension);
        if (observer) {
          idleService.removeIdleObserver(observer, detectionInterval);
        }
        observersMap.delete(extension);
      },
    });
  }
  return observerInfo;
};

const getIdleObserver = (extension, context) => {
  let observerInfo = getIdleObserverInfo(extension, context);
  let {observer, detectionInterval} = observerInfo;
  if (!observer) {
    observer = new class extends ExtensionCommon.EventEmitter {
      observe(subject, topic, data) {
        if (topic == "idle" || topic == "active") {
          this.emit("stateChanged", topic);
        }
      }
    }();
    idleService.addIdleObserver(observer, detectionInterval);
    observerInfo.observer = observer;
    observerInfo.detectionInterval = detectionInterval;
  }
  return observer;
};

const setDetectionInterval = (extension, context, newInterval) => {
  let observerInfo = getIdleObserverInfo(extension, context);
  let {observer, detectionInterval} = observerInfo;
  if (observer) {
    idleService.removeIdleObserver(observer, detectionInterval);
    idleService.addIdleObserver(observer, newInterval);
  }
  observerInfo.detectionInterval = newInterval;
};

this.idle = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      idle: {
        queryState: function(detectionIntervalInSeconds) {
          if (idleService.idleTime < detectionIntervalInSeconds * 1000) {
            return Promise.resolve("active");
          }
          return Promise.resolve("idle");
        },
        setDetectionInterval: function(detectionIntervalInSeconds) {
          setDetectionInterval(extension, context, detectionIntervalInSeconds);
        },
        onStateChanged: new EventManager({
          context,
          name: "idle.onStateChanged",
          register: fire => {
            let listener = (event, data) => {
              fire.sync(data);
            };

            getIdleObserver(extension, context).on("stateChanged", listener);
            return () => {
              getIdleObserver(extension, context).off("stateChanged", listener);
            };
          },
        }).api(),
      },
    };
  }
};
