# Kinto Schema

This document contains the information about the Kinto schema and UI schema needed to run dev experiments
on the "Dev" Kinto instance located at https://kinto.dev.mozaws.net.

## JSON Schema

```JSON
{
  "type": "object",
  "required": [
    "id",
    "description",
    "match",
    "buckets",
    "branches"
  ],
  "properties": {
    "id": {
      "title": "Experiment id",
      "type": "string",
      "minLength": 1,
      "maxLength": 100
    },
    "description": {
      "title": "Description",
      "type": "string"     
    },
    "buckets": {
      "title": "Buckets",
      "type": "object",
      "description": "Each user is assigned a random bucket from 0 to 999. Select the bucket ranges here to control the enrolled population size.",
      "required": [
        "start",
        "count"
      ],
      "properties": {
        "start": {
          "mininum": 0,
          "maximum": 999,
          "type": "number"
        },
        "count": {
          "mininum": 0,
          "maximum": 1000,
          "type": "number"
        }
      }
    },
    "branches": {
      "title": "Branches",
      "type": "array",
      "required": [
        "name",
        "ratio"
      ],
      "default": [],
      "uniqueItems": true,
      "minItems": 1,
      "description": "Each experiment needs to specify one or more branches. Each branch has a name and a ratio. An enrolled user is assigned one branch randomly, with the probabilities weighted per the ratio.",
      "items": {
        "description": "One experiment branch.",
        "title": "Branch",
        "type": "object",
        "properties": {
          "name": {
            "type": "string",
            "description": "The branch name. This is what product code uses to decide which branch logic to execute.",
            "minLength": 1,
            "maxLength": 100
          },
          "ratio": {
            "type": "number",
            "description": "The branches ratio is the probabilistic weight for random branch assignment.",
            "mininum": 1,
            "maximum": 1000,
            "default": 1
          }
        }
      }
    },
    "match": {
      "title": "Matching",
      "type": "object",
      "description": "A list of optional matchers, which allow restricting the experiment to e.g. specific application ids.",
      "properties": {
        "app_id": {
          "type": "string",
          "description": "Match specific application ids. A regex. E.g.: ^org.mozilla.fennec|org.mozilla.firefox_beta|org.mozilla.firefox$",
          "minLength": 1,
          "maxLength": 1000
        },
        "app_display_version": {
          "description": "The application's version number. A regex. E.g.: '47.0a1', '46.0'",
          "type": "string"
        },
        "app_min_version": {
          "description": "The application's minimum version number. E.g.: '47.0.11', '46.0'",
          "type": "string"
        },
        "app_max_version": {
          "description": "The application's maximum version number. E.g.: '47.0.11', '46.0'",
          "type": "string"
        },
        "locale_country": {
          "description": "Match country, pulled from the default locale. A regex. E.g.: USA|ITA",
          "type": "string",
          "minLength": 1,
          "maxLength": 1000
        },
        "locale_language": {
          "description": "Language, pulled from the default locale. A regex. E.g.: eng|esp",
          "type": "string",
          "minLength": 1,
          "maxLength": 1000
        },
        "device_model": {
          "description": "Device name. A regex.",
          "type": "string",
          "minLength": 1,
          "maxLength": 1000
        },
        "device_manufacturer": {
          "description": "Device manufacturer",
          "type": "string",
          "minLength": 1,
          "maxLength": 1000
        },
        "regions": {
          "default": [],
          "description": "Compared with GeoIP lookup, where supported.",
          "items": {
            "default": "",
            "description": "Similar to a GeoIP lookup",
            "minLength": 1,
            "maxLength": 1000,
            "title": "Regions",
            "type": "string"
          },
          "title": "Regions",
          "type": "array",
          "uniqueItems": true
        },
        "debug_tags": {
          "default": [],
          "description": "Target specific debug tags only. This allows testing of experiments for only specific active users for QA etc.",
          "items": {
            "default": "",
            "description": "A debug tag set through the libraries debug activity.",
            "minLength": 1,
            "title": "Debug tag",
            "type": "string"
          },
          "title": "Debug tags",
          "type": "array",
          "uniqueItems": true
        }
      }
    }
  }
}
```
  
## UI Schema

```JSON
{
  "sort": "-last_modified",
  "displayFields": [
    "id",
    "description"
  ],
  "attachment": {
    "enabled": false,
    "required": false
  },
  "schema": {
    "properties": {
      "id": {
        "type": "string",
        "maxLength": 100,
        "title": "Experiment id",
        "minLength": 1
      },
      "buckets": {
        "description": "Each user is assigned a random bucket from 0 to 999. Select the bucket ranges here to control the enrolled population size.",
        "properties": {
          "start": {
            "type": "number",
            "mininum": 0,
            "maximum": 999
          },
          "count": {
            "type": "number",
            "mininum": 0,
            "maximum": 1000
          }
        },
        "type": "object",
        "required": [
          "start",
          "count"
        ],
        "title": "Buckets"
      },
      "description": {
        "type": "string",
        "title": "Description"
      },
      "branches": {
        "description": "Each experiment needs to specify one or more branches. Each branch has a name and a ratio. An enrolled user is assigned one branch randomly, with the probabilities weighted per the ratio.",
        "required": [
          "name",
          "ratio"
        ],
        "title": "Branches",
        "items": {
          "description": "One experiment branch.",
          "properties": {
            "ratio": {
              "description": "The branches ratio is the probabilistic weight for random branch assignment.",
              "type": "number",
              "default": 1,
              "mininum": 1,
              "maximum": 1000
            },
            "name": {
              "description": "The branch name. This is what product code uses to decide which branch logic to execute.",
              "type": "string",
              "maxLength": 100,
              "minLength": 1
            }
          },
          "type": "object",
          "title": "Branch"
        },
        "type": "array",
        "minItems": 1,
        "uniqueItems": true,
        "default": []
      },
      "match": {
        "description": "A list of optional matchers, which allow restricting the experiment to e.g. specific application ids.",
        "properties": {
          "app_id": {
            "description": "Match specific application ids. A regex. E.g.: ^org.mozilla.fennec|org.mozilla.firefox_beta|org.mozilla.firefox$",
            "type": "string",
            "maxLength": 1000,
            "minLength": 1
          },
          "app_display_version": {
            "description": "The application's version number. A regex. E.g.: '47.0a1', '46.0'",
            "type": "string"
          },
          "app_min_version": {
            "description": "The application's minimum version number. E.g.: '47.0.11', '46.0'",
            "type": "string"
          },
          "app_max_version": {
            "description": "The application's maximum version number. E.g.: '47.0.11', '46.0'",
            "type": "string"
          },
          "device_manufacturer": {
            "description": "Device manufacturer",
            "type": "string",
            "maxLength": 1000,
            "minLength": 1
          },
          "debug_tags": {
            "description": "Target specific debug tags only. This allows testing of experiments for only specific active users for QA etc.",
            "title": "Debug tags",
            "items": {
              "description": "A debug tag set through the libraries debug activity.",
              "type": "string",
              "title": "Debug tag",
              "default": "",
              "minLength": 1
            },
            "type": "array",
            "uniqueItems": true,
            "default": []
          },
          "locale_country": {
            "description": "Match country, pulled from the default locale. A regex. E.g.: USA|ITA",
            "type": "string",
            "maxLength": 1000,
            "minLength": 1
          },
          "regions": {
            "description": "Compared with GeoIP lookup, where supported.",
            "title": "Regions",
            "items": {
              "description": "Similar to a GeoIP lookup",
              "maxLength": 1000,
              "title": "Regions",
              "type": "string",
              "minLength": 1,
              "default": ""
            },
            "type": "array",
            "uniqueItems": true,
            "default": []
          },
          "device_model": {
            "description": "Device name. A regex.",
            "type": "string",
            "maxLength": 1000,
            "minLength": 1
          },
          "locale_language": {
            "description": "Language, pulled from the default locale. A regex. E.g.: eng|esp",
            "type": "string",
            "maxLength": 1000,
            "minLength": 1
          }
        },
        "type": "object",
        "title": "Matching"
      }
    },
    "type": "object",
    "required": [
      "id",
      "description",
      "match",
      "buckets",
      "branches"
    ]
  },
  "uiSchema": {
    "buckets": {
      "ui:order": [
        "start",
        "count"
      ]
    },
    "description": {
      "ui:widget": "textarea"
    },
    "match": {
      "ui:order": [
        "app_id",
        "app_display_version",
        "app_min_version",
        "app_max_version",
        "locale_language",
        "locale_country",
        "device_model",
        "device_manufacturer",
        "regions",
        "debug_tags"
      ]
    },
    "ui:order": [
      "id",
      "description",
      "buckets",
      "branches",
      "match"
    ]
  },
  "cache_expires": 0
}
```
  
## Where to add this

For testing create a collection `mobile-experiments` in the `main` bucket on the [Kinto dev server](https://kinto.dev.mozaws.net/v1/admin/).

## Records list columns

What's added in "Records list columns" is what get's shown in the record lists overview.
We want:
- id
- description