# Events ping

The `events` ping is normally sent when the application goes into the
background. It is also sent when the queue of events exceeds
`Glean.configuration.maxEvents` (default 500). This happens automatically, with
no intervention or configuration required by the application.

At the top-level, this ping contains the following keys:

- `ping_info`: The information [common to all pings](pings.md#the-ping_info-section).

- `events`: An array of all of the events that have occurred since the last time
  the `events` ping was sent.

Each entry in the `events` array is an array with the following items:

- [0]: `msSinceStart`: The milliseconds since the start of the process.

- [1]: `category`: The category of the event, as defined by its location in the
  `metrics.yaml` file.

- [2]: `name`: The name of the event, as definded in the `metrics.yaml` file.

- [3]: `objectId`: An arbitrary string representing the id of the object on
  which the event occurred. The application is responsible for providing the id
  to glean when events are recorded.

- [4]: `value` (optional): A application-provided value providing additional
  context for the event.  Values will never exceed 80 characters.

- [5]: `extra` (optional): A mapping of strings to strings providing additional
  data about the event. Both the keys and values in this map will never exceed
  80 characters.
