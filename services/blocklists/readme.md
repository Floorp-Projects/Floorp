# Blocklist

The blocklist entries are synchronized locally from the Firefox Settings service.

https://firefox.settings.services.mozilla.com

In order to reduce the amount of data to be downloaded on first synchronization,
a JSON dump from the records present on the remote server is shipped with the
release.

## How to update the JSON files ?

Even though it is not a problem if the dumps are not up-to-date when shipped, here
are the commands to update them:

```
SERVICE_URL="https://firefox.settings.services.mozilla.com/v1"

curl "$SERVICE_URL/buckets/blocklists/collections/certificates/records?"  > services/blocklists/certificates.json
curl "$SERVICE_URL/buckets/blocklists/collections/gfx/records?"  > services/blocklists/gfx.json
curl "$SERVICE_URL/buckets/blocklists/collections/plugins/records?"  > services/blocklists/plugins.json
curl "$SERVICE_URL/buckets/blocklists/collections/addons/records?"  > services/blocklists/addons.json

curl "$SERVICE_URL/buckets/pinning/collections/pins/records?"  > services/blocklists/pins.json
```

## TODO

- Setup a bot to update it regularly.
