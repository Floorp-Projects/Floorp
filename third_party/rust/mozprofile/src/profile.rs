use preferences::{Preferences, Pref};
use prefreader::{parse, serialize, PrefReaderError};
use std::collections::btree_map::Iter;
use std::fs::File;
use std::io::Result as IoResult;
use std::io::prelude::*;
use std::path::{Path, PathBuf};
use tempdir::TempDir;

#[derive(Debug)]
pub struct Profile {
    pub path: PathBuf,
    pub temp_dir: Option<TempDir>,
    prefs: Option<PrefFile>,
    user_prefs: Option<PrefFile>,
}

impl Profile {
    pub fn new(opt_path: Option<&Path>) -> IoResult<Profile> {
        let mut temp_dir = None;
        let path = match opt_path {
            Some(p) => p.to_path_buf(),
            None => {
                let dir = try!(TempDir::new("rust_mozprofile"));
                let temp_path = dir.path().to_path_buf();
                temp_dir = Some(dir);
                temp_path
            }
        };

        Ok(Profile {
            path: path,
            temp_dir: temp_dir,
            prefs: None,
            user_prefs: None
        })
    }

    pub fn prefs(&mut self) -> Result<&mut PrefFile, PrefReaderError> {
        if self.prefs.is_none() {
            let mut pref_path = PathBuf::from(&self.path);
            pref_path.push("prefs.js");
            self.prefs = Some(try!(PrefFile::new(pref_path)))
        };
        // This error handling doesn't make much sense
        Ok(self.prefs.as_mut().unwrap())
    }

    pub fn user_prefs(&mut self) -> Result<&mut PrefFile, PrefReaderError> {
        if self.user_prefs.is_none() {
            let mut pref_path = PathBuf::from(&self.path);
            pref_path.push("user.js");
            self.user_prefs = Some(try!(PrefFile::new(pref_path)))
        };
        // This error handling doesn't make much sense
        Ok(self.user_prefs.as_mut().unwrap())
    }
}

#[derive(Debug)]
pub struct PrefFile {
    path: PathBuf,
    pub prefs: Preferences,
}

impl PrefFile {
    pub fn new(path: PathBuf) -> Result<PrefFile, PrefReaderError> {
        let prefs = if !path.exists() {
            Preferences::new()
        } else {
            let mut f = try!(File::open(&path));
            let mut buf = String::with_capacity(4096);
            try!(f.read_to_string(&mut buf));
            try!(parse(buf.as_bytes()))
        };

        Ok(PrefFile {
            path: path,
            prefs: prefs
        })
    }

    pub fn write(&self) -> IoResult<()> {
        let mut f = try!(File::create(&self.path));
        serialize(&self.prefs, &mut f)
    }

    pub fn insert_slice<K>(&mut self, preferences: &[(K, Pref)])
        where K: Into<String> + Clone {
        for &(ref name, ref value) in preferences.iter() {
            self.insert((*name).clone(), (*value).clone());
        }
    }

    pub fn insert<K>(&mut self, key: K, value: Pref)
        where K: Into<String> {
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
