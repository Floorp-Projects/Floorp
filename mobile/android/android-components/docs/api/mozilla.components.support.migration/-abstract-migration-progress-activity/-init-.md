[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [AbstractMigrationProgressActivity](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`AbstractMigrationProgressActivity()`

An activity that notifies on migration progress. Should be used in tandem with [MigrationIntentProcessor](../-migration-intent-processor/index.md).

Implementation notes: this abstraction exists to ensure that migration apps do not allow the user to
invoke [AppCompatActivity.onBackPressed](#).

