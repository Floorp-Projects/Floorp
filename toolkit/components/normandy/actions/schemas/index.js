var EXPORTED_SYMBOLS = ["ActionSchemas"];

const ActionSchemas = {
  consoleLog: {
    "$schema": "http://json-schema.org/draft-04/schema#",
    "title": "Log a message to the console",
    "type": "object",
    "required": [
      "message"
    ],
    "properties": {
      "message": {
        "description": "Message to log to the console",
        "type": "string",
        "default": ""
      }
    }
  }
};

if (typeof module !== "undefined") {
  /* globals module */
  module.exports = ActionSchemas;
}
