// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
#![allow(non_camel_case_types)]

pub enum EnvironmentFlags {
    FIXED_MAP,
    NO_SUB_DIR,
    WRITE_MAP,
    READ_ONLY,
    NO_META_SYNC,
    NO_SYNC,
    MAP_ASYNC,
    NO_TLS,
    NO_LOCK,
    NO_READAHEAD,
    NO_MEM_INIT,
}

pub enum DatabaseFlags {
    REVERSE_KEY,
    #[cfg(feature = "db-dup-sort")]
    DUP_SORT,
    #[cfg(feature = "db-dup-sort")]
    DUP_FIXED,
    #[cfg(feature = "db-int-key")]
    INTEGER_KEY,
    INTEGER_DUP,
    REVERSE_DUP,
}

pub enum WriteFlags {
    NO_OVERWRITE,
    NO_DUP_DATA,
    CURRENT,
    APPEND,
    APPEND_DUP,
}
