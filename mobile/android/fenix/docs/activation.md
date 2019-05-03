# The `activation` ping

## Description
This ping provides a measure of the activation of mobile products.

## Scheduling
The `activation` ping is automatically sent at the very first startup, after Glean is initialized.
It is only sent once and only re-attempted a subsequent startups if it hasn't been sent yet.

## Contents
This ping contains the following fields:

| Field name | Type | Description |
|---|---|---|
| `identifier` | String | An hashed and salted version of the Google Advertising ID from the device. |
| `activation_id` | UUID | An alternate identifier, not correlated with the client_id, generated once and only sent with the activation ping. |

The `activation` ping also includes the common [ping sections](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/docs/pings/pings.md#ping-sections)
found in all pings, with the exclusion of the `client_id` (as defined by the [`pings.yaml`](../app/pings.yaml) file).

## Example `activation` ping

```json
{
  "ping_info": { },
  "client_info": { },
  "metrics": {
    "string": {
      "activation.identifier": "d+lnddDYN2ILBDGvhBIBHORRMrmVwTCp6rGLLFi8SMo="
    },
    "uuid": {
      "activation.activation_id": "c0c40a5f-bd75-41ca-8097-9a38103de7fe"
    }
  }
}
```
