"use strict";

extensions.registerSchemaAPI("idle", "idle", (extension, context) => {
  return {
    idle: {
      queryState: function(detectionIntervalInSeconds, callback) {
        runSafe(context, callback, "active");
      },
    },
  };
});
