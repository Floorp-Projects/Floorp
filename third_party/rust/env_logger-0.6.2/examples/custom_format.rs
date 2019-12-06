/*!
Changing the default logging format.

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

use std::io::Write;

use env_logger::{Env, Builder, fmt};

fn init_logger() {
    let env = Env::default()
        .filter("MY_LOG_LEVEL")
        .write_style("MY_LOG_STYLE");

    let mut builder = Builder::from_env(env);

    // Use a different format for writing log records
    // The colors are only available when the `termcolor` dependency is (which it is by default)
    #[cfg(feature = "termcolor")]
    builder.format(|buf, record| {
        let mut style = buf.style();
        style.set_bg(fmt::Color::Yellow).set_bold(true);

        let timestamp = buf.timestamp();

        writeln!(buf, "My formatted log ({}): {}", timestamp, style.value(record.args()))
    });

    builder.init();
}

fn main() {
    init_logger();

    info!("a log from `MyLogger`");
}
