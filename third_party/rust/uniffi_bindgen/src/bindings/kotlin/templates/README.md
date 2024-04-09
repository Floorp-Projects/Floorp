# Rules for the Kotlin template code

## Naming

Private variables, classes, functions, etc. should be prefixed with `uniffi`, `Uniffi`, or `UNIFFI`.
This avoids naming collisions with user-defined items.
Users will not get name collisions as long as they don't use "uniffi", which is reserved for us.

In particular, make sure to use the `uniffi` prefix for any variable names in generated functions.
If you name a variable something like `result` the code will probably work initially.
Then it will break later on when a user decides to define a function with a parameter named `result`.

Note: this doesn't apply to items that we want to expose, for example users may want to catch `InternalException` so doesn't get the `Uniffi` prefix.
