# Using Oblivious HTTP in Firefox on Glean

[Oblivious HTTP (RFC 9458)][ohttp-spec]
is an Internet standard transport that permits a separation of privacy concerns.

A client sending an HTTP(S) request necessarily exposes both
their network address and the request's contents to the destination server.
OHTTP allows, through the introduction of encapsulation and a relay,
for a system by which a third-party relay may learn only the network address and not the contents,
and the server may learn only the request contents and not the network address.

This can be a useful risk mitigation for data collections we do not wish to associate with an IP address.

## Can I use OHTTP for my Data?

Any data collection that meets the following criteria can use OHTTP:
* Your data must be solely collected on Firefox Desktop
    * At this time, no other Mozilla project supports OHTTP.
* Your data must be recorded via Glean.
    * It is the sole data collection system at Mozilla that supports OHTTP.
* Your data must be in its own [custom ping][custom-ping-doc].
    * OHTTP is a transport-level decision and pings are Glean's transport payload.
* Your data (like all new or expanded data collections in Mozilla projects)
  must have gone through [Data Collection Review][data-review].
    * If you're considering OHTTP it's likely because the data you intend to collect is sensitive.
      That'll mean you'll probably specifically need to go through
      [Sensitive Data Collection Review][sensitive-review].
* Your data must not need to be associated with an id that is sent without OHTTP.
    * This includes `client_id` and the Mozilla Accounts identifier.
      The `client_id` and other fingerprinting information are explicitly excluded
      from pings using OHTTP.

## How can I use OHTTP for my Data?

### Short Version: add two metadata fields to your ping definition

Most simply, you opt a ping into using OHTTP by augmenting its
`pings.yaml` definition with these three lines:

```yaml
  metadata:
    include_info_sections: false
    use_ohttp: true
```

[Here is a convenience link to a searchfox search for `use_ohttp: true`][use-ohttp-searchfox]
if you'd like to see existing uses in tree.

### Longer Version

0. Ensure you've followed the necessary steps for
   [adding new instrumentation to Firefox Desktop][new-instrumentation-doc]:
    * Name your ping,
    * Design and implement your instrumentation,
    * Design and implement your ping submission schedule,
    * Arrange for [data review][data-review] (probably [sensitive][sensitive-review]).
1. Augment your ping's definition in its `pings.yaml` with
   `metadata.include_info_sections: false` and
   `metadata.use_ohttp: true`:
    * `include_info_sections: false` ensures that there is no
      `client_id` or fingerprintable pieces of `client_info` or `ping_info`
      fields that would allow us to trivially map this ping to a specific client.
    * `use_ohttp: true` signals to Firefox on Glean's (FOG's) `glean_parser` extensions to
      generate the necessary code to recognize this ping as needing OHTTP transport.
      It is read in FOG's uploader to ensure the ping is only sent using OHTTP.
2. [Test your instrumentation][instrumentation-tests].

And that's it!


[ohttp-spec]: https://datatracker.ietf.org/doc/rfc9458/
[custom-ping-doc]: https://mozilla.github.io/glean/book/reference/pings/index.html
[data-review]: https://wiki.mozilla.org/Data_Collection
[sensitive-review]: https://wiki.mozilla.org/Data_Collection#Step_3:_Sensitive_Data_Collection_Review_Process
[use-ohttp-searchfox]: https://searchfox.org/mozilla-central/search?q=use_ohttp%3A%20true
[new-instrumentation-doc]: ./new_definitions_file.md
[instrumentation-tests]: ./instrumentation_tests.md
