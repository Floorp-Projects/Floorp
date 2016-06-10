"use strict";

extensions.registerSchemaAPI("idle", (extension, context) => {
  return {
    idle: {
      queryState: function(detectionIntervalInSeconds) {
        return Promise.resolve("active");
      },
    },
  };
});
