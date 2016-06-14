"use strict";

extensions.registerSchemaAPI("idle", "idle", (extension, context) => {
  return {
    idle: {
      queryState: function(detectionIntervalInSeconds) {
        return Promise.resolve("active");
      },
    },
  };
});
