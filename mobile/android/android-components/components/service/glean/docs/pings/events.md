# The `events` ping

## Description
The events ping's purpose is to transport all of the event metric information.

## Scheduling
The `events` ping is normally sent when the application goes into the
[background](pings.md#defining-background-state). It is also sent when the queue of events exceeds
`Glean.configuration.maxEvents` (default 500). This happens automatically, with
no intervention or configuration required by the application.

## Contents
At the top-level, this ping contains the following keys:

- `ping_info`: The information [common to all pings](pings.md#the-ping_info-section).

- `events`: An array of all of the events that have occurred since the last time
  the `events` ping was sent.

Each entry in the `events` array is an array with the following items:

- [0]: `msSinceStart`: The milliseconds since the start of the process.

- [1]: `category`: The category of the event, as defined by its location in the
  `metrics.yaml` file.

- [2]: `name`: The name of the event, as definded in the `metrics.yaml` file.

- [3]: `extra` (optional): A mapping of strings to strings providing additional
  data about the event. The keys are restricted to 40 characters and values in 
  this map will never exceed 100 characters.
