"use strict";

extensions.registerSchemaAPI("idle", context => {
  return {
    idle: {
      queryState: function(detectionIntervalInSeconds) {
        return Promise.resolve("active");
      },
    },
  };
});
