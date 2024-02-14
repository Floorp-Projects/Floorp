# Adjust

Firefox for Android tracks certain types of installs using a third-party install tracking framework called Adjust. The intention is to determine the origin of Firefox for Android installs by answering the question, “Did this user on this device install Firefox for Android in response to a specific advertising campaign performed by Mozilla?”

The framework consists of a software development kit (SDK) linked into Firefox for Android and a data-collecting Internet service backend run by the German company [adjust GmbH](https://www.adjust.com). The Adjust SDK is open source and MIT licensed. It is hosted at [https://github.com/adjust/android_sdk](https://github.com/adjust/android_sdk). Firefox for Android pulls in an unmodified copy of the SDK using Gradle. The [Dependencies.kt](https://github.com/mozilla-mobile/fenix/blob/main/buildSrc/src/main/java/Dependencies.kt) contains the version of the framework that is being used. The SDK is documented at [https://docs.adjust.com](https://docs.adjust.com).

## Adjust Integration

The Adjust framework is abstracted via the [AdjustMetricService](https://github.com/mozilla-mobile/fenix/blob/main/app/src/main/java/org/mozilla/fenix/components/metrics/AdjustMetricsService.kt) class. All interaction with Adjust happens via this class.

## Adjust Messages

The Adjust SDK collects and sends one type of message to the Adjust backend:

* At the start of a new application session, a *Session Message* is sent with basic system info and how often the app has been used since the last time.

The message is documented below in more detail of what is sent in each HTTP request. All messages are posted to a secure endpoint at `https://app.adjust.com`. They are all `application/x-www-form-urlencoded` HTTP `POST` requests.

### Session Message

#### Request

```
bundle_id:               org.mozilla.fenix
tracking_enabled:        0
language:                en
country:                 CA
app_version:             4.2
device_name:             Pixel 2
app_version_short:       2.0
needs_response_details:  0
attribution_deeplink:    1
session_count:           1
os_name:                 android
event_buffering_enabled: 0
idfv:                    8D452BFB-0692-4E8C-9DE0-7578486A872E
hardware_name:           J127AP
app_token:               xxxxxxxxxxxx
os_version:              10.1
environment:             production
created_at:              2016-11-10T20:34:39.720Z-0500
device_type:             phone
idfa:                    00000000-0000-0000-0000-000000000000
sent_at:                 2016-11-10T20:34:39.787Z-0500
```

These parameters (including ones not exposed to Mozilla) are documented at [https://partners.adjust.com/placeholders/](https://partners.adjust.com/placeholders/)

#### Response

If the application was successfully attributed to a specific campaign, details for that campaign are sent back as a JSON response:

```
{ "app_token": "xxxxxxxxxxxx",
  "adid": "00000000000000000000",
  "attribution" { "tracker_token": "xxxxxx",
                  "tracker_name": "Network::CAMPAIGN::ADGROUP::CREATIVE",
                  "network": "Network",
                  "campaign":"CAMPAIGN",
                  "adgroup":"ADGROUP",
                  "creative":"CREATIVE" } }
```

The application has no use for this information and ignores it.
