// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use clap::Parser;

#[tokio::main]
async fn main() -> Result<(), neqo_bin::server::Error> {
    let args = neqo_bin::server::Args::parse();

    neqo_bin::server::server(args).await
}
