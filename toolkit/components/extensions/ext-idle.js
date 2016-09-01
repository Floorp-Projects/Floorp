"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "idleService",
                                   "@mozilla.org/widget/idleservice;1",
                                   "nsIIdleService");

extensions.registerSchemaAPI("idle", "addon_parent", context => {
  return {
    idle: {
      queryState: function(detectionIntervalInSeconds) {
        if (idleService.idleTime < detectionIntervalInSeconds * 1000) {
          return Promise.resolve("active");
        }
        return Promise.resolve("idle");
      },
    },
  };
});
