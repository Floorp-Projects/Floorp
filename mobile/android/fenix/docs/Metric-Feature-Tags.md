To help find metrics in the [Glean Dictionary] and other tools, metrics should contain tag metadata corresponding to the
feature area(s) that they belong to. In the case of Firefox for Android, tag information is based off of the GitHub feature labels for this repository:

https://github.com/mozilla-mobile/fenix/labels?q=feature%3A

You can see how tag information is rendered here:

https://dictionary.telemetry.mozilla.org/apps/fenix?itemType=tags&page=1

## Adding feature tags to metrics

Adding tag information to a metric used to involve editing the [Glean Annotations repository], but you can now add this
information directly when adding or modifying `metrics.yaml`. Just add a section called `metadata` to the metric and add a list of tags that correspond to it.

For example:

```yaml
  search_bar_tapped:
    type: event
    description: |
      A user tapped the search bar
    metadata:
      tags:
        - Search
    ...
```

## Updating the feature tags

The set of valid tags is documented in a file called `tags.yaml`, but should never be updated by hand.
If a feature labels is ever added or removed, you can synchronize the tags file in the source tree by running `./tools/update-glean-tags.py` in the root of the repository.
Note that a tag *must* be specified in `tags.yaml` for it to be usable in a metric, so if a tag is removed from `tags.yaml` all uses of it must be removed from `metrics.yaml`.

[Glean Dictionary]: https://dictionary.telemetry.mozilla.org
[Glean Annotations repository]: https://github.com/mozilla/glean-annotations
