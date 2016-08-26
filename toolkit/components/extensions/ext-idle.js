"use strict";

extensions.registerSchemaAPI("idle", "addon_parent", context => {
  return {
    idle: {
      queryState: function(detectionIntervalInSeconds) {
        return Promise.resolve("active");
      },
    },
  };
});
