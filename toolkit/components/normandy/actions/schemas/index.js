/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        description:
          "Unique identifer for the rollout, used in telemetry and rollbacks",
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
              description: "Full dotted-path of the preference being changed",
              type: "string",
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
    required: ["name", "description", "addonUrl", "extensionApiId"],
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
      extensionApiId: {
        description:
          "The record ID of the extension used for Normandy API calls.",
        type: "integer",
      },
      isEnrollmentPaused: {
        description: "If true, new users will not be enrolled in the study.",
        type: "boolean",
        default: false,
      },
    },
  },

  "addon-rollout": {
    $schema: "http://json-schema.org/draft-04/schema#",
    title: "Install add-on permanently",
    type: "object",
    required: ["extensionApiId", "slug"],
    properties: {
      extensionApiId: {
        description:
          "The record ID of the extension used for Normandy API calls.",
        type: "integer",
      },
      slug: {
        description:
          "Unique identifer for the rollout, used in telemetry and rollbacks.",
        type: "string",
        pattern: "^[a-z0-9\\-_]+$",
      },
    },
  },

  "addon-rollback": {
    $schema: "http://json-schema.org/draft-04/schema#",
    title: "Undo an add-on rollout",
    type: "object",
    required: ["rolloutSlug"],
    properties: {
      rolloutSlug: {
        description: "Unique identifer for the rollout to undo.",
        type: "string",
        pattern: "^[a-z0-9\\-_]+$",
      },
    },
  },

  "branched-addon-study": {
    $schema: "http://json-schema.org/draft-04/schema#",
    title: "Enroll a user in an add-on experiment, with managed branches",
    type: "object",
    required: ["slug", "userFacingName", "userFacingDescription", "branches"],
    properties: {
      slug: {
        description: "Machine-readable identifier",
        type: "string",
        minLength: 1,
      },
      userFacingName: {
        description: "User-facing name of the study",
        type: "string",
        minLength: 1,
      },
      userFacingDescription: {
        description: "User-facing description of the study",
        type: "string",
        minLength: 1,
      },
      isEnrollmentPaused: {
        description: "If true, new users will not be enrolled in the study.",
        type: "boolean",
        default: false,
      },
      branches: {
        description: "List of experimental branches",
        type: "array",
        minItems: 1,
        items: {
          type: "object",
          required: ["slug", "ratio", "extensionApiId"],
          properties: {
            slug: {
              description:
                "Unique identifier for this branch of the experiment.",
              type: "string",
              pattern: "^[A-Za-z0-9\\-_]+$",
            },
            ratio: {
              description:
                "Ratio of users who should be grouped into this branch.",
              type: "integer",
              minimum: 1,
            },
            extensionApiId: {
              description:
                "The record ID of the add-on uploaded to the Normandy server. May be null, in which case no add-on will be installed.",
              type: ["number", "null"],
              default: null,
            },
          },
        },
      },
    },
  },

  "show-heartbeat": {
    $schema: "http://json-schema.org/draft-04/schema#",
    title: "Show a Heartbeat survey.",
    description: "This action shows a single survey.",

    type: "object",
    required: [
      "surveyId",
      "message",
      "thanksMessage",
      "postAnswerUrl",
      "learnMoreMessage",
      "learnMoreUrl",
    ],
    properties: {
      repeatOption: {
        type: "string",
        enum: ["once", "xdays", "nag"],
        description: "Determines how often a prompt is shown executes.",
        default: "once",
      },
      repeatEvery: {
        description:
          "For repeatOption=xdays, how often (in days) the prompt is displayed.",
        default: null,
        type: ["number", "null"],
      },
      includeTelemetryUUID: {
        type: "boolean",
        description: "Include unique user ID in post-answer-url and Telemetry",
        default: false,
      },
      surveyId: {
        type: "string",
        description: "Slug uniquely identifying this survey in telemetry",
      },
      message: {
        description: "Message to show to the user",
        type: "string",
      },
      engagementButtonLabel: {
        description:
          "Text for the engagement button. If specified, this button will be shown instead of rating stars.",
        default: null,
        type: ["string", "null"],
      },
      thanksMessage: {
        description:
          "Thanks message to show to the user after they've rated Firefox",
        type: "string",
      },
      postAnswerUrl: {
        description:
          "URL to redirect the user to after rating Firefox or clicking the engagement button",
        default: null,
        type: ["string", "null"],
      },
      learnMoreMessage: {
        description: "Message to show to the user to learn more",
        default: null,
        type: ["string", "null"],
      },
      learnMoreUrl: {
        description: "URL to show to the user when they click Learn More",
        default: null,
        type: ["string", "null"],
      },
    },
  },

  "multi-preference-experiment": {
    $schema: "http://json-schema.org/draft-04/schema#",
    title: "Run a feature experiment activated by a set of preferences.",
    type: "object",
    required: ["slug", "userFacingName", "userFacingDescription", "branches"],
    properties: {
      slug: {
        description: "Unique identifier for this experiment",
        type: "string",
        pattern: "^[A-Za-z0-9\\-_]+$",
      },
      userFacingName: {
        description: "User-facing name of the experiment",
        type: "string",
        minLength: 1,
      },
      userFacingDescription: {
        description: "User-facing description of the experiment",
        type: "string",
        minLength: 1,
      },
      experimentDocumentUrl: {
        description: "URL of a document describing the experiment",
        type: "string",
        format: "uri",
        default: "",
      },
      isHighPopulation: {
        description:
          "Marks the preference experiment as a high population experiment, that should be excluded from certain types of telemetry",
        type: "boolean",
        default: "false",
      },
      isEnrollmentPaused: {
        description: "If true, new users will not be enrolled in the study.",
        type: "boolean",
        default: false,
      },
      branches: {
        description: "List of experimental branches",
        type: "array",
        minItems: 1,
        items: {
          type: "object",
          required: ["slug", "ratio", "preferences"],
          properties: {
            slug: {
              description:
                "Unique identifier for this branch of the experiment",
              type: "string",
              pattern: "^[A-Za-z0-9\\-_]+$",
            },
            ratio: {
              description:
                "Ratio of users who should be grouped into this branch",
              type: "integer",
              minimum: 1,
            },
            preferences: {
              description:
                "The set of preferences to be set if this branch is chosen",
              type: "object",
              patternProperties: {
                ".*": {
                  type: "object",
                  properties: {
                    preferenceType: {
                      description:
                        "Data type of the preference that controls this experiment",
                      type: "string",
                      enum: ["string", "integer", "boolean"],
                    },
                    preferenceBranchType: {
                      description:
                        "Controls whether the default or user value of the preference is modified",
                      type: "string",
                      enum: ["user", "default"],
                      default: "default",
                    },
                    preferenceValue: {
                      description:
                        "Value for this preference when this branch is chosen",
                      type: ["string", "number", "boolean"],
                    },
                  },
                  required: ["preferenceType", "preferenceValue"],
                },
              },
            },
          },
        },
      },
    },
  },

  "single-preference-experiment": {
    $schema: "http://json-schema.org/draft-04/schema#",
    title: "Run a feature experiment activated by a preference.",
    type: "object",
    required: ["slug", "preferenceName", "preferenceType", "branches"],
    properties: {
      slug: {
        description: "Unique identifier for this experiment",
        type: "string",
        pattern: "^[A-Za-z0-9\\-_]+$",
      },
      experimentDocumentUrl: {
        description: "URL of a document describing the experiment",
        type: "string",
        format: "uri",
        default: "",
      },
      preferenceName: {
        description:
          "Full dotted-path of the preference that controls this experiment",
        type: "string",
      },
      preferenceType: {
        description:
          "Data type of the preference that controls this experiment",
        type: "string",
        enum: ["string", "integer", "boolean"],
      },
      preferenceBranchType: {
        description:
          "Controls whether the default or user value of the preference is modified",
        type: "string",
        enum: ["user", "default"],
        default: "default",
      },
      isHighPopulation: {
        description:
          "Marks the preference experiment as a high population experiment, that should be excluded from certain types of telemetry",
        type: "boolean",
        default: "false",
      },
      isEnrollmentPaused: {
        description: "If true, new users will not be enrolled in the study.",
        type: "boolean",
        default: false,
      },
      branches: {
        description: "List of experimental branches",
        type: "array",
        minItems: 1,
        items: {
          type: "object",
          required: ["slug", "value", "ratio"],
          properties: {
            slug: {
              description:
                "Unique identifier for this branch of the experiment",
              type: "string",
              pattern: "^[A-Za-z0-9\\-_]+$",
            },
            value: {
              description: "Value to set the preference to for this branch",
              type: ["string", "number", "boolean"],
            },
            ratio: {
              description:
                "Ratio of users who should be grouped into this branch",
              type: "integer",
              minimum: 1,
            },
          },
        },
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
