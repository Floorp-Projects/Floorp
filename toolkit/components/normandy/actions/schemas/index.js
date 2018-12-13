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
              type: ["string", "integer", "boolean"],
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

  "show-heartbeat": {
    "$schema": "http://json-schema.org/draft-04/schema#",
    "title": "Show a Heartbeat survey.",
    "description": "This action shows a single survey.",

    "type": "object",
    "required": [
      "surveyId",
      "message",
      "thanksMessage",
      "postAnswerUrl",
      "learnMoreMessage",
      "learnMoreUrl",
    ],
    "properties": {
      "repeatOption": {
        "type": "string",
        "enum": ["once", "xdays", "nag"],
        "description": "Determines how often a prompt is shown executes.",
        "default": "once",
      },
      "repeatEvery": {
        "description": "For repeatOption=xdays, how often (in days) the prompt is displayed.",
        "default": null,
        "type": ["number", "null"],
      },
      "includeTelemetryUUID": {
        "type": "boolean",
        "description": "Include unique user ID in post-answer-url and Telemetry",
        "default": false,
      },
      "surveyId": {
        "type": "string",
        "description": "Slug uniquely identifying this survey in telemetry",
      },
      "message": {
        "description": "Message to show to the user",
        "type": "string",
      },
      "engagementButtonLabel": {
        "description": "Text for the engagement button. If specified, this button will be shown instead of rating stars.",
        "default": null,
        "type": ["string", "null"],
      },
      "thanksMessage": {
        "description": "Thanks message to show to the user after they've rated Firefox",
        "type": "string",
      },
      "postAnswerUrl": {
        "description": "URL to redirect the user to after rating Firefox or clicking the engagement button",
        "default": null,
        "type": ["string", "null"],
      },
      "learnMoreMessage": {
        "description": "Message to show to the user to learn more",
        "default": null,
        "type": ["string", "null"],
      },
      "learnMoreUrl": {
        "description": "URL to show to the user when they click Learn More",
        "default": null,
        "type": ["string", "null"],
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
