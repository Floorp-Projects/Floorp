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

  "addon-study": {
    $schema: "http://json-schema.org/draft-04/schema#",
    title: "Enroll a user in an opt-out SHIELD study",
    type: "object",
    required: [
      "name",
      "description",
      "addonUrl",
    ],
    properties: {
      name: {
        description: "User-facing name of the study",
        type: "string",
        minLength: 1,
      },
      description: {
        description: "User-facing description of the study",
        type: "string",
        minLength: 1,
      },
      addonUrl: {
        description: "URL of the add-on XPI file",
        type: "string",
        format: "uri",
        minLength: 1,
      },
      isEnrollmentPaused: {
        description: "If true, new users will not be enrolled in the study.",
        type: "boolean",
        default: false,
      },
    },
  },
};

// Legacy name used on Normandy server
ActionSchemas["opt-out-study"] = ActionSchemas["addon-study"];

// If running in Node.js, export the schemas.
if (typeof module !== "undefined") {
  /* globals module */
  module.exports = ActionSchemas;
}
