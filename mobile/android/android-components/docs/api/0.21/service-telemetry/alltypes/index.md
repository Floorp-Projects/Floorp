---
title: alltypes - 
---

### All Types

| [org.mozilla.telemetry.measurement.ArchMeasurement](../org.mozilla.telemetry.measurement/-arch-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.ClientIdMeasurement](../org.mozilla.telemetry.measurement/-client-id-measurement/index.html) | A unique, randomly generated UUID for this client. |
| [org.mozilla.telemetry.util.ContextUtils](../org.mozilla.telemetry.util/-context-utils/index.html) |  |
| [org.mozilla.telemetry.measurement.CreatedDateMeasurement](../org.mozilla.telemetry.measurement/-created-date-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.CreatedDateMeasurementNew](../org.mozilla.telemetry.measurement/-created-date-measurement-new/index.html) | The field 'created' from CreatedDateMeasurement will be deprecated for the `createdDate` field |
| [org.mozilla.telemetry.measurement.CreatedTimestampMeasurement](../org.mozilla.telemetry.measurement/-created-timestamp-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.CreatedTimestampMeasurementNew](../org.mozilla.telemetry.measurement/-created-timestamp-measurement-new/index.html) | The field 'created' from CreatedTimestampMeasurement will be deprecated for the `createdTimestamp` field |
| [org.mozilla.telemetry.net.DebugLogClient](../org.mozilla.telemetry.net/-debug-log-client/index.html) | This client just prints pings to logcat instead of uploading them. Therefore this client is only useful for debugging purposes. |
| [org.mozilla.telemetry.measurement.DefaultSearchMeasurement](../org.mozilla.telemetry.measurement/-default-search-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.DeviceMeasurement](../org.mozilla.telemetry.measurement/-device-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.EventsMeasurement](../org.mozilla.telemetry.measurement/-events-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.ExperimentsMeasurement](../org.mozilla.telemetry.measurement/-experiments-measurement/index.html) |  |
| [org.mozilla.telemetry.storage.FileTelemetryStorage](../org.mozilla.telemetry.storage/-file-telemetry-storage/index.html) | TelemetryStorage implementation that stores pings as files on disk. |
| [org.mozilla.telemetry.util.FileUtils](../org.mozilla.telemetry.util/-file-utils/index.html) |  |
| [org.mozilla.telemetry.measurement.FirstRunProfileDateMeasurement](../org.mozilla.telemetry.measurement/-first-run-profile-date-measurement/index.html) | This measurement will save the timestamp of the first time it was instantiated and report this as profile creation date. |
| [org.mozilla.telemetry.net.HttpURLConnectionTelemetryClient](../org.mozilla.telemetry.net/-http-u-r-l-connection-telemetry-client/index.html) |  |
| [org.mozilla.telemetry.util.IOUtils](../org.mozilla.telemetry.util/-i-o-utils/index.html) |  |
| [org.mozilla.telemetry.serialize.JSONPingSerializer](../org.mozilla.telemetry.serialize/-j-s-o-n-ping-serializer/index.html) | TelemetryPingSerializer that uses the org.json library provided by the Android system. |
| [org.mozilla.telemetry.schedule.jobscheduler.JobSchedulerTelemetryScheduler](../org.mozilla.telemetry.schedule.jobscheduler/-job-scheduler-telemetry-scheduler/index.html) | TelemetryScheduler implementation that uses Android's JobScheduler API to schedule ping uploads. |
| [org.mozilla.telemetry.measurement.LocaleMeasurement](../org.mozilla.telemetry.measurement/-locale-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.MetricsMeasurement](../org.mozilla.telemetry.measurement/-metrics-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.OperatingSystemMeasurement](../org.mozilla.telemetry.measurement/-operating-system-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.OperatingSystemVersionMeasurement](../org.mozilla.telemetry.measurement/-operating-system-version-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.ProcessStartTimestampMeasurement](../org.mozilla.telemetry.measurement/-process-start-timestamp-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.SearchesMeasurement](../org.mozilla.telemetry.measurement/-searches-measurement/index.html) | A TelemetryMeasurement implementation to count the number of times a user has searched with a specific engine from a specific location. |
| [org.mozilla.telemetry.measurement.SequenceMeasurement](../org.mozilla.telemetry.measurement/-sequence-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.SessionCountMeasurement](../org.mozilla.telemetry.measurement/-session-count-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.SessionDurationMeasurement](../org.mozilla.telemetry.measurement/-session-duration-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.SettingsMeasurement](../org.mozilla.telemetry.measurement/-settings-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.StaticMeasurement](../org.mozilla.telemetry.measurement/-static-measurement/index.html) |  |
| [org.mozilla.telemetry.util.StringUtils](../org.mozilla.telemetry.util/-string-utils/index.html) |  |
| [org.mozilla.telemetry.Telemetry](../org.mozilla.telemetry/-telemetry/index.html) |  |
| [org.mozilla.telemetry.net.TelemetryClient](../org.mozilla.telemetry.net/-telemetry-client/index.html) |  |
| [org.mozilla.telemetry.config.TelemetryConfiguration](../org.mozilla.telemetry.config/-telemetry-configuration/index.html) | The TelemetryConfiguration class collects the information describing the telemetry setup of an app. There are some parts that every app needs to configure: Where should measurements store data? What servers are we actually uploading pings to? This class should provide good defaults so that in the best case it is not needed to modify the configuration. |
| [org.mozilla.telemetry.ping.TelemetryCorePingBuilder](../org.mozilla.telemetry.ping/-telemetry-core-ping-builder/index.html) | This mobile-specific ping is intended to provide the most critical data in a concise format, allowing for frequent uploads. Since this ping is used to measure retention, it should be sent each time the app is opened. https://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/data/core-ping.html |
| [org.mozilla.telemetry.event.TelemetryEvent](../org.mozilla.telemetry.event/-telemetry-event/index.html) | TelemetryEvent specifies a common events data format, which allows for broader, shared usage of data processing tools. |
| [org.mozilla.telemetry.ping.TelemetryEventPingBuilder](../org.mozilla.telemetry.ping/-telemetry-event-ping-builder/index.html) | A telemetry ping builder for pings of type "focus-event". |
| [org.mozilla.telemetry.TelemetryHolder](../org.mozilla.telemetry/-telemetry-holder/index.html) | Holder of a static reference to the Telemetry instance. This is required for background services that somehow need to get access to the configuration and storage. This is not particular nice. Hopefully we can replace this with something better. |
| [org.mozilla.telemetry.schedule.jobscheduler.TelemetryJobService](../org.mozilla.telemetry.schedule.jobscheduler/-telemetry-job-service/index.html) |  |
| [org.mozilla.telemetry.measurement.TelemetryMeasurement](../org.mozilla.telemetry.measurement/-telemetry-measurement/index.html) |  |
| [org.mozilla.telemetry.ping.TelemetryMobileEventPingBuilder](../org.mozilla.telemetry.ping/-telemetry-mobile-event-ping-builder/index.html) | A telemetry ping builder for events of type "mobile-event". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-event/mobile-event.1.schema.json |
| [org.mozilla.telemetry.ping.TelemetryMobileMetricsPingBuilder](../org.mozilla.telemetry.ping/-telemetry-mobile-metrics-ping-builder/index.html) | A telemetry ping builder for events of type "mobile-metrics". See the schema for more details: https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-metrics/mobile-metrics.1.schema.json |
| [org.mozilla.telemetry.ping.TelemetryPing](../org.mozilla.telemetry.ping/-telemetry-ping/index.html) |  |
| [org.mozilla.telemetry.ping.TelemetryPingBuilder](../org.mozilla.telemetry.ping/-telemetry-ping-builder/index.html) |  |
| [org.mozilla.telemetry.serialize.TelemetryPingSerializer](../org.mozilla.telemetry.serialize/-telemetry-ping-serializer/index.html) |  |
| [org.mozilla.telemetry.schedule.TelemetryScheduler](../org.mozilla.telemetry.schedule/-telemetry-scheduler/index.html) |  |
| [org.mozilla.telemetry.storage.TelemetryStorage](../org.mozilla.telemetry.storage/-telemetry-storage/index.html) |  |
| [org.mozilla.telemetry.measurement.TimezoneOffsetMeasurement](../org.mozilla.telemetry.measurement/-timezone-offset-measurement/index.html) |  |
| [org.mozilla.telemetry.measurement.VersionMeasurement](../org.mozilla.telemetry.measurement/-version-measurement/index.html) |  |

