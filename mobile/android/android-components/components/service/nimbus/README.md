# [Android Components](../../../README.md) > Service > Nimbus

A wrapper for the Nimbus SDK.

Contents:

- [Usage](#usage)

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:service-nimbus:{latest-version}"
```

### Initializing the Experiments library

**TODO**

### Updating of experiments

**TODO**

### Checking if a user is part of an experiment

**TODO**

## Testing Nimbus

This section contains information about the Kinto and UI schemas needed to set up and run experiments on the "Dev" Kinto instance located at https://kinto.dev.mozaws.net.
**NOTE** The dev server instance requires LDAP authorization, but does not require connection to the internal Mozilla VPN.

## Where to add the Kinto and UI schemas

For testing purposes, create a collection with an id of `nimbus-mobile-experiments` in the `main` bucket on the [Kinto dev server](https://kinto.dev.mozaws.net/v1/admin/).

### JSON Schema

```JSON
{
  "$schema": "http://json-schema.org/draft-07/schema",
  "$id": "http://mozilla.org/example.json",
  "type": "object",
  "title": "Nimbus Schema",
  "description": "This is the description of the current Nimbus experiment schema, which can be found at https://github.com/mozilla/nimbus-shared",
  "default": {},
  "examples": [
    {
      "slug": "secure-gold",
      "endDate": null,
      "branches": [
        {
          "slug": "control",
          "ratio": 1
        },
        {
          "slug": "treatment",
          "ratio": 1
        }
      ],
      "probeSets": [],
      "startDate": null,
      "application": "org.mozilla.fenix",
      "bucketConfig": {
        "count": 100,
        "start": 0,
        "total": 10000,
        "namespace": "secure-gold",
        "randomizationUnit": "nimbus_id"
      },
      "userFacingName": "Diagnostic test experiment",
      "referenceBranch": "control",
      "isEnrollmentPaused": false,
      "proposedEnrollment": 7,
      "userFacingDescription": "This is a test experiment for diagnostic purposes.",
      "id": "secure-gold"
    }
  ],
  "required": [
    "slug",
    "branches",
    "application",
    "bucketConfig",
    "userFacingName",
    "referenceBranch",
    "isEnrollmentPaused",
    "proposedEnrollment",
    "userFacingDescription",
    "id"
  ],
  "properties": {
    "slug": {
      "$id": "#/properties/slug",
      "type": "string",
      "title": "Slug",
      "description": "The slug is the unique identifier for the experiment.",
      "default": "",
      "examples": ["fenix-search-widget-experiment"]
    },
    "endDate": {
      "$id": "#/properties/endDate",
      "type": ["string", "null"],
      "format": "date-time",
      "title": "End Date",
      "description": "This is the date that the experiment will end.",
      "default": null,
      "examples": [null]
    },
    "branches": {
      "$id": "#/properties/branches",
      "type": "array",
      "title": "Branches",
      "description": "Branches relate to the specific treatments to be applied for the experiment.",
      "default": [],
      "examples": [
        [
          {
            "slug": "control",
            "ratio": 1
          },
          {
            "slug": "treatment",
            "ratio": 1
          }
        ]
      ],
      "additionalItems": true,
      "items": {
        "$id": "#/properties/branches/items",
        "anyOf": [
          {
            "$id": "#/properties/branches/items/anyOf/0",
            "type": "object",
            "title": "Branch Items",
            "description": "Each branch has a slug, or name, and a ratio that weights selection into that branch",
            "default": {},
            "examples": [
              {
                "slug": "control",
                "ratio": 1
              }
            ],
            "required": ["slug", "ratio"],
            "properties": {
              "slug": {
                "$id": "#/properties/branches/items/anyOf/0/properties/slug",
                "type": "string",
                "title": "Branch Slug",
                "description": "The branch slug is the unique name of the branch, within this experiment.",
                "default": "control",
                "examples": ["control"]
              },
              "ratio": {
                "$id": "#/properties/branches/items/anyOf/0/properties/ratio",
                "type": "integer",
                "title": "Branch Ratio",
                "description": "This is the weighting of the branch for branch selection.",
                "default": 1,
                "examples": [1]
              }
            },
            "additionalProperties": true
          }
        ]
      }
    },
    "probeSets": {
      "$id": "#/properties/probeSets",
      "type": "array",
      "title": "Probe Sets",
      "description": "Currently unimplemented/used",
      "default": [],
      "examples": [[]],
      "additionalItems": true,
      "items": {
        "$id": "#/properties/probeSets/items"
      }
    },
    "startDate": {
      "$id": "#/properties/startDate",
      "type": ["string", "null"],
      "format": "date-time",
      "title": "Start Date",
      "description": "The date that the experiment will start",
      "default": null,
      "examples": [null]
    },
    "application": {
      "$id": "#/properties/application",
      "type": "string",
      "title": "Application",
      "description": "This is the application to target",
      "default": "",
      "examples": [
        "org.mozilla.fenix",
        "org.mozilla.firefox",
        "org.mozilla.ios.firefox"
      ]
    },
    "bucketConfig": {
      "$id": "#/properties/bucketConfig",
      "type": "object",
      "title": "Bucket Configuration",
      "description": "This is the configuration of the bucketing for determining the experiment sample size",
      "default": {},
      "examples": [
        {
          "count": 2000,
          "start": 0,
          "total": 10000,
          "namespace": "performance-experiments",
          "randomizationUnit": "nimbus_id"
        }
      ],
      "required": ["count", "start", "total", "namespace", "randomizationUnit"],
      "properties": {
        "count": {
          "$id": "#/properties/bucketConfig/properties/count",
          "type": "integer",
          "title": "Count",
          "description": "The total count of buckets to assign to be eligible to enroll in the experiment.",
          "default": 0,
          "examples": [2000]
        },
        "start": {
          "$id": "#/properties/bucketConfig/properties/start",
          "type": "integer",
          "title": "Starting Bucket",
          "description": "This is the bucket that the count of buckets will start from.",
          "default": 0,
          "examples": [0]
        },
        "total": {
          "$id": "#/properties/bucketConfig/properties/total",
          "type": "integer",
          "title": "Total Buckets",
          "description": "This is the total number of buckets to divide the population into for enrollment purposes.",
          "default": 10000,
          "examples": [10000]
        },
        "namespace": {
          "$id": "#/properties/bucketConfig/properties/namespace",
          "type": "string",
          "title": "Namespace",
          "description": "This is the bucket namespace and should always match the experiment slug",
          "default": "",
          "examples": ["secure-gold"]
        },
        "randomizationUnit": {
          "$id": "#/properties/bucketConfig/properties/randomizationUnit",
          "type": "string",
          "title": "Randomization Unit",
          "description": "This is the id to use for randomization for the purpose of bucketing. Currently only nimbus_id implemented.",
          "default": "nimbus_id",
          "examples": ["nimbus_id"]
        }
      },
      "additionalProperties": true
    },
    "userFacingName": {
      "$id": "#/properties/userFacingName",
      "type": "string",
      "title": "User Facing Name",
      "description": "The user-facing name of the experiment.",
      "default": "",
      "examples": ["Diagnostic test experiment"]
    },
    "referenceBranch": {
      "$id": "#/properties/referenceBranch",
      "type": "string",
      "title": "Reference Branch",
      "description": "Not currently implemented, do not change default",
      "default": "control",
      "examples": ["control"]
    },
    "isEnrollmentPaused": {
      "$id": "#/properties/isEnrollmentPaused",
      "type": "boolean",
      "title": "Enrollment Paused",
      "description": "True if the enrollment is paused, false if enrollment is active.",
      "default": false,
      "examples": [false]
    },
    "proposedEnrollment": {
      "$id": "#/properties/proposedEnrollment",
      "type": "integer",
      "title": "Proposed Enrollment",
      "description": "The length in days that enrollment is proposed.",
      "default": 7,
      "examples": [7]
    },
    "userFacingDescription": {
      "$id": "#/properties/userFacingDescription",
      "type": "string",
      "title": "User Facing Description",
      "description": "This is the description of the experiment that would be presented to the user.",
      "default": "",
      "examples": ["This is a test experiment for diagnostic purposes."]
    },
    "id": {
      "$id": "#/properties/id",
      "type": "string",
      "title": "ID",
      "description": "An analog of the slug? Not sure, make this match slug...",
      "default": "",
      "examples": ["secure-gold"]
    }
  },
  "additionalProperties": true
}
```
  
## UI Schema

```JSON
{
  "ui:order": [
    "slug",
    "userFacingName",
    "userFacingDescription",
    "application",
    "startDate",
    "endDate",
    "bucketConfig",
    "branches",
    "referenceBranch",
    "isEnrollmentPaused",
    "proposedEnrollment",
    "id",
    "probeSets"
  ],
  "userFacingDescription": {
    "ui:widget": "textarea"
  },
  "bucketConfig": {
    "ui:order": ["start", "count", "total", "namespace", "randomizationUnit"]
  },
  "branches": {
    "ui:order": ["slug", "ratio"]
  }
}

```
  
## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
