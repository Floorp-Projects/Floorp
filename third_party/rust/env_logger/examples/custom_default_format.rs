/*!
Disabling parts of the default format.

Before running this example, try setting the `MY_LOG_LEVEL` environment variable to `info`:

```no_run,shell
$ export MY_LOG_LEVEL='info'
```

Also try setting the `MY_LOG_STYLE` environment variable to `never` to disable colors
or `auto` to enable them:

```no_run,shell
$ export MY_LOG_STYLE=never
```

If you want to control the logging output completely, see the `custom_logger` example.
*/

#[macro_use]
extern crate log;
extern crate env_logger;

use env_logger::{Env, Builder};

fn init_logger() {
    let env = Env::default()
        .filter("MY_LOG_LEVEL")
        .write_style("MY_LOG_STYLE");

    let mut builder = Builder::from_env(env);

    builder
        .default_format_level(false)
        .default_format_timestamp_nanos(true);

    builder.init();
}

fn main() {
    init_logger();

    info!("a log from `MyLogger`");
}
