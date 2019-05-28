# Internal behavior

## Clearing metrics when disabling/enabling Glean

When disabling upload (`Glean.setUploadEnabled(false)`), metrics are also
cleared to prevent their storage on the local device, and lessen the likelihood
of accidentally sending them.  There are a couple of exceptions to this:

- `first_run_date` is retained so it isn't reset if metrics are later re-enabled.

- `client_id` is set to the special value
  `"c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0"`. This should make it possible to
  detect the error case when metrics are sent from a client that has been disabled.

When re-enabling metrics:

- `first_run_date` is left as-is. (It should remain a correct date of first run
  of the application, unaffected by disabling/enabling Glean).

- The `client_id` is set to a newly-generated random UUID. It has no connection
  to the `client_id` used prior to disabling Glean.

- Application lifetime metrics owned by Glean are regenerated from scratch so
  that they will appear in subsequent pings. This is the same process that
  happens during every startup of the application when Glean is enabled. The
  application is also responsible for setting any application lifetime metrics
  that it manages at this time.

- Ping lifetime metrics do not need special handling.  They will begin recording
  again after metric uploading is re-enabled.
