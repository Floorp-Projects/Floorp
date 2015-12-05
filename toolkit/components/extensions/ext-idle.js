"use strict";

extensions.registerPrivilegedAPI("idle", (extension, context) => {
  return {
    idle: {
      queryState: function(detectionIntervalInSeconds, callback) {
        runSafe(context, callback, "active");
      },
    },
  };
});
