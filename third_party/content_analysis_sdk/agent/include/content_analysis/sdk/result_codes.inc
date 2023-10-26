// This file is #included from C++ headers and source code to generate code
// specific to each ResultCode.  The including code is expected to #define
// macros for RC_RECOVERABLE and RC_UNRECOVERABLE before #including this file
// and then #undef then after use.

RC_RECOVERABLE(OK, "Operation completed successfully.")
RC_RECOVERABLE(ERR_MISSING_RESULT, "Response is missing a result message.")
RC_RECOVERABLE(ERR_RESPONSE_ALREADY_SENT, "A resonse has already been sent for this request.")
RC_RECOVERABLE(ERR_MISSING_REQUEST_TOKEN, "The request is missing a request token.")
RC_RECOVERABLE(ERR_AGENT_NOT_INITIALIZED, "The agent is not proplerly initialized to handle events.")
RC_RECOVERABLE(ERR_INVALID_REQUEST_FROM_BROWSER, "The browser sent an incorrectly formatted message.")
RC_RECOVERABLE(ERR_IO_PENDING, "IO incomplete, the operation is still pending.")
RC_RECOVERABLE(ERR_MORE_DATA, "There is more data to read before the entire message has been received.")
RC_RECOVERABLE(ERR_CANNOT_GET_BROWSER_PID, "Cannot get process Id of browser.")
RC_RECOVERABLE(ERR_CANNOT_GET_BROWSER_BINARY_PATH, "Cannot get the full path to the brower's main binary file.")
RC_RECOVERABLE(ERR_BROKEN_PIPE, "Browser process has disconnected.")
RC_RECOVERABLE(ERR_UNEXPECTED, "An internal error has occured.")

// All unrecoverable errors should be declared below ERR_FIRST_UNRECOVERABLE_ERROR.
RC_UNRECOVERABLE(ERR_FIRST_UNRECOVERABLE_ERROR, "Marker for the first unrecoverable error.")
RC_UNRECOVERABLE(ERR_AGENT_ALREADY_EXISTS, "Another process is already running as an agent on this computer.")
RC_UNRECOVERABLE(ERR_AGENT_EVENT_HANDLER_NOT_SPECIFIED, "An agent handler was not specified when creating an agent.")
RC_UNRECOVERABLE(ERR_CANNOT_CREATE_AGENT_STOP_EVENT, "Could not create event to signal the agent to stop.")
RC_UNRECOVERABLE(ERR_INVALID_CHANNEL_NAME, "Invalid channel name specified in Agent::Config.")
RC_UNRECOVERABLE(ERR_CANNOT_CREATE_CHANNEL_IO_EVENT, "Could not create event to perform async IO with a client.")
