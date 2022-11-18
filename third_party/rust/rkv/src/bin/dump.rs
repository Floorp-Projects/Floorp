// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::{env::args, io, path::Path};

use rkv::migrator::{LmdbArchMigrateError, LmdbArchMigrator};

fn main() -> Result<(), LmdbArchMigrateError> {
    let mut cli_args = args();
    let mut db_name = None;
    let mut env_path = None;

    // The first arg is the name of the program, which we can ignore.
    cli_args.next();

    while let Some(arg) = cli_args.next() {
        if &arg[0..1] == "-" {
            match &arg[1..] {
                "s" => {
                    db_name = match cli_args.next() {
                        None => return Err("-s must be followed by database name".into()),
                        Some(str) => Some(str),
                    };
                }
                str => return Err(format!("arg -{str} not recognized").into()),
            }
        } else {
            if env_path.is_some() {
                return Err("must provide only one path to the LMDB environment".into());
            }
            env_path = Some(arg);
        }
    }

    let env_path = env_path.ok_or("must provide a path to the LMDB environment")?;
    let mut migrator = LmdbArchMigrator::new(Path::new(&env_path))?;
    migrator.dump(db_name.as_deref(), io::stdout()).unwrap();

    Ok(())
}
