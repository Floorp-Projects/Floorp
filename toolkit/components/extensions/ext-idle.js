"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
                                  "resource://devtools/shared/event-emitter.js");
XPCOMUtils.defineLazyServiceGetter(this, "idleService",
                                   "@mozilla.org/widget/idleservice;1",
                                   "nsIIdleService");

// WeakMap[Extension -> Object]
let observersMap = new WeakMap();

function getObserverInfo(extension, context) {
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
}

function getObserver(extension, context) {
  let observerInfo = getObserverInfo(extension, context);
  let {observer, detectionInterval} = observerInfo;
  if (!observer) {
    observer = {
      observe: function(subject, topic, data) {
        if (topic == "idle" || topic == "active") {
          this.emit("stateChanged", topic);
        }
      },
    };
    EventEmitter.decorate(observer);
    idleService.addIdleObserver(observer, detectionInterval);
    observerInfo.observer = observer;
    observerInfo.detectionInterval = detectionInterval;
  }
  return observer;
}

function setDetectionInterval(extension, context, newInterval) {
  let observerInfo = getObserverInfo(extension, context);
  let {observer, detectionInterval} = observerInfo;
  if (observer) {
    idleService.removeIdleObserver(observer, detectionInterval);
    idleService.addIdleObserver(observer, newInterval);
  }
  observerInfo.detectionInterval = newInterval;
}

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
        onStateChanged: new SingletonEventManager(context, "idle.onStateChanged", fire => {
          let listener = (event, data) => {
            fire.sync(data);
          };

          getObserver(extension, context).on("stateChanged", listener);
          return () => {
            getObserver(extension, context).off("stateChanged", listener);
          };
        }).api(),
      },
    };
  }
};
