var EXPORTED_SYMBOLS = ["ActionSchemas"];

const ActionSchemas = {
  "console-log": {
    $schema: "http://json-schema.org/draft-04/schema#",
    title: "Log a message to the console",
    type: "object",
    required: ["message"],
    properties: {
      message: {
        description: "Message to log to the console",
        type: "string",
        default: "",
      },
    },
  },

  "preference-rollout": {
    $schema: "http://json-schema.org/draft-04/schema#",
    title: "Change preferences permanently",
    type: "object",
    required: ["slug", "preferences"],
    properties: {
      slug: {
        description: "Unique identifer for the rollout, used in telemetry and rollbacks",
        type: "string",
        pattern: "^[a-z0-9\\-_]+$",
      },
      preferences: {
        description: "The preferences to change, and their values",
        type: "array",
        minItems: 1,
        items: {
          type: "object",
          required: ["preferenceName", "value"],
          properties: {
            preferenceName: {
              "description": "Full dotted-path of the preference being changed",
              "type": "string",
            },
            value: {
              description: "Value to set the preference to",
              type: ["string", "number", "boolean"],
            },
          },
        },
      },
    },
  },

  "preference-rollback": {
    $schema: "http://json-schema.org/draft-04/schema#",
    title: "Undo a preference rollout",
    type: "object",
    required: ["rolloutSlug"],
    properties: {
      rolloutSlug: {
        description: "Unique identifer for the rollout to undo",
        type: "string",
        pattern: "^[a-z0-9\\-_]+$",
      },
    },
  },
};

// If running in Node.js, export the schemas.
if (typeof module !== "undefined") {
  /* globals module */
  module.exports = ActionSchemas;
}
