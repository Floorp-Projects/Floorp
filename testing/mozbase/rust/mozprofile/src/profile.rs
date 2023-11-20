/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::preferences::{Pref, Preferences};
use crate::prefreader::{parse, serialize, PrefReaderError};
use std::collections::btree_map::Iter;
use std::fs::File;
use std::io::prelude::*;
use std::io::Result as IoResult;
use std::path::{Path, PathBuf};
use tempfile::{Builder, TempDir};

#[derive(Debug)]
pub struct Profile {
    pub path: PathBuf,
    pub temp_dir: Option<TempDir>,
    prefs: Option<PrefFile>,
    user_prefs: Option<PrefFile>,
}

impl PartialEq for Profile {
    fn eq(&self, other: &Profile) -> bool {
        self.path == other.path
    }
}

impl Profile {
    pub fn new(temp_root: Option<&Path>) -> IoResult<Profile> {
        let mut dir_builder = Builder::new();
        dir_builder.prefix("rust_mozprofile");
        let dir = if let Some(temp_root) = temp_root {
            dir_builder.tempdir_in(temp_root)
        } else {
            dir_builder.tempdir()
        }?;
        let path = dir.path().to_path_buf();
        let temp_dir = Some(dir);
        Ok(Profile {
            path,
            temp_dir,
            prefs: None,
            user_prefs: None,
        })
    }

    pub fn new_from_path(p: &Path) -> IoResult<Profile> {
        let path = p.to_path_buf();
        let temp_dir = None;
        Ok(Profile {
            path,
            temp_dir,
            prefs: None,
            user_prefs: None,
        })
    }

    pub fn prefs(&mut self) -> Result<&mut PrefFile, PrefReaderError> {
        if self.prefs.is_none() {
            let mut pref_path = PathBuf::from(&self.path);
            pref_path.push("prefs.js");
            self.prefs = Some(PrefFile::new(pref_path)?)
        };
        // This error handling doesn't make much sense
        Ok(self.prefs.as_mut().unwrap())
    }

    pub fn user_prefs(&mut self) -> Result<&mut PrefFile, PrefReaderError> {
        if self.user_prefs.is_none() {
            let mut pref_path = PathBuf::from(&self.path);
            pref_path.push("user.js");
            self.user_prefs = Some(PrefFile::new(pref_path)?)
        };
        // This error handling doesn't make much sense
        Ok(self.user_prefs.as_mut().unwrap())
    }
}

#[derive(Debug)]
pub struct PrefFile {
    pub path: PathBuf,
    pub prefs: Preferences,
}

impl PrefFile {
    pub fn new(path: PathBuf) -> Result<PrefFile, PrefReaderError> {
        let prefs = if !path.exists() {
            Preferences::new()
        } else {
            let mut f = File::open(&path)?;
            let mut buf = String::with_capacity(4096);
            f.read_to_string(&mut buf)?;
            parse(buf.as_bytes())?
        };

        Ok(PrefFile { path, prefs })
    }

    pub fn write(&self) -> IoResult<()> {
        let mut f = File::create(&self.path)?;
        serialize(&self.prefs, &mut f)
    }

    pub fn insert_slice<K>(&mut self, preferences: &[(K, Pref)])
    where
        K: Into<String> + Clone,
    {
        for (name, value) in preferences.iter() {
            self.insert((*name).clone(), (*value).clone());
        }
    }

    pub fn insert<K>(&mut self, key: K, value: Pref)
    where
        K: Into<String>,
    {
        self.prefs.insert(key.into(), value);
    }

    pub fn remove(&mut self, key: &str) -> Option<Pref> {
        self.prefs.remove(key)
    }

    pub fn get(&mut self, key: &str) -> Option<&Pref> {
        self.prefs.get(key)
    }

    pub fn contains_key(&self, key: &str) -> bool {
        self.prefs.contains_key(key)
    }

    pub fn iter(&self) -> Iter<String, Pref> {
        self.prefs.iter()
    }
}
