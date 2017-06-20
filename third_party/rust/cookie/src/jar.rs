//! A cookie jar implementation for storing a set of cookies.
//!
//! This CookieJar type can be used to manage a session of cookies by keeping
//! track of cookies that are added and deleted over time. It provides a method,
//! `delta`, which will calculate the number of `Set-Cookie` headers that need
//! to be sent back to a client which tracks the changes in the lifetime of the
//! jar itself.
//!
//! A cookie jar can also be borrowed to a child cookie jar with new
//! functionality such as automatically signing cookies, storing permanent
//! cookies, etc. This functionality can also be chained together.

use std::collections::{HashMap, HashSet};
use std::cell::RefCell;
use std::fmt;
use std::borrow::Cow;

use time::{self, Duration};

use ::Cookie;

/// A jar of cookies for managing a session.
///
/// # Example
///
/// ```
/// use cookie::{Cookie, CookieJar};
///
/// let c = CookieJar::new(b"f8f9eaf1ecdedff5e5b749c58115441e");
///
/// // Add a cookie to this jar
/// c.add(Cookie::new("key".to_string(), "value".to_string()));
///
/// // Remove the added cookie
/// c.remove("key");
/// ```
pub struct CookieJar<'a> {
    flavor: Flavor<'a>,
}

enum Flavor<'a> {
    Child(Child<'a>),
    Root(Root),
}

struct Child<'a> {
    parent: &'a CookieJar<'a>,
    read: Read,
    write: Write,
}

type Read = fn(&Root, Cookie<'static>) -> Option<Cookie<'static>>;
type Write = fn(&Root, Cookie<'static>) -> Cookie<'static>;

struct Root {
    map: RefCell<HashMap<Cow<'static, str>, Cookie<'static>>>,
    new_cookies: RefCell<HashSet<Cow<'static, str>>>,
    removed_cookies: RefCell<HashSet<Cow<'static, str>>>,
    _key: secure::SigningKey,
}

/// Iterator over the cookies in a cookie jar
pub struct Iter<'a> {
    jar: &'a CookieJar<'a>,
    keys: Vec<Cow<'static, str>>,
}

impl<'a> CookieJar<'a> {
    /// Creates a new empty cookie jar with the given signing key.
    ///
    /// The given key is used to sign cookies in the signed cookie jar. If
    /// signed cookies aren't used then you can pass an empty array.
    pub fn new(key: &[u8]) -> CookieJar<'static> {
        CookieJar {
            flavor: Flavor::Root(Root {
                map: RefCell::new(HashMap::new()),
                new_cookies: RefCell::new(HashSet::new()),
                removed_cookies: RefCell::new(HashSet::new()),
                _key: secure::prepare_key(key),
            }),
        }
    }

    fn root<'b>(&'b self) -> &'b Root {
        let mut cur = self;
        loop {
            match cur.flavor {
                Flavor::Child(ref child) => cur = child.parent,
                Flavor::Root(ref me) => return me,
            }
        }
    }

    /// Adds an original cookie from a request.
    ///
    /// This method only works on the root cookie jar and is not intended for
    /// use during the lifetime of a request, it is intended to initialize a
    /// cookie jar from an incoming request.
    ///
    /// # Panics
    ///
    /// Panics if `self` is not the root cookie jar.
    pub fn add_original(&mut self, cookie: Cookie<'static>) {
        match self.flavor {
            Flavor::Child(..) => panic!("can't add an original cookie to a child jar!"),
            Flavor::Root(ref mut root) => {
                let name = cookie.name().to_string();
                root.map.borrow_mut().insert(name.into(), cookie);
            }
        }
    }

    /// Adds a new cookie to this cookie jar.
    ///
    /// If this jar is a child cookie jar, this will walk up the chain of
    /// borrowed jars, modifying the cookie as it goes along.
    pub fn add(&self, mut cookie: Cookie<'static>) {
        let mut cur = self;
        let root = self.root();
        loop {
            match cur.flavor {
                Flavor::Child(ref child) => {
                    cookie = (child.write)(root, cookie);
                    cur = child.parent;
                }
                Flavor::Root(..) => break,
            }
        }
        let name = cookie.name().to_string();
        root.map.borrow_mut().insert(name.clone().into(), cookie);
        root.removed_cookies.borrow_mut().remove(&*name);
        root.new_cookies.borrow_mut().insert(name.into());
    }

    /// Removes a cookie from this cookie jar.
    pub fn remove<N: Into<Cow<'static, str>>>(&self, cookie_name: N) {
        let root = self.root();
        let name = cookie_name.into();
        root.map.borrow_mut().remove(&name);
        root.new_cookies.borrow_mut().remove(&name);
        root.removed_cookies.borrow_mut().insert(name);
    }

    /// Clears all cookies from this cookie jar.
    pub fn clear(&self) {
        let root = self.root();
        let all_cookies: Vec<_> = root.map
            .borrow()
            .keys()
            .map(|n| n.to_owned())
            .collect();

        root.map.borrow_mut().clear();
        root.new_cookies.borrow_mut().clear();
        root.removed_cookies.borrow_mut().extend(all_cookies);
    }

    /// Finds a cookie inside of this cookie jar.
    ///
    /// The cookie is subject to modification by any of the child cookie jars
    /// that are currently borrowed. A copy of the cookie is returned.
    pub fn find(&self, name: &str) -> Option<Cookie<'static>> {
        let root = self.root();
        if root.removed_cookies.borrow().contains(name) {
            return None;
        }
        root.map.borrow().get(name).and_then(|c| self.try_read(root, c.clone()))
    }

    /// Creates a child signed cookie jar.
    ///
    /// All cookies read from the child jar will require a valid signature and
    /// all cookies written will be signed automatically.
    ///
    /// This API is only available when the `secure` feature is enabled on this
    /// crate.
    ///
    /// # Example
    ///
    /// ```rust
    /// # use cookie::{Cookie, CookieJar};
    /// let c = CookieJar::new(b"f8f9eaf1ecdedff5e5b749c58115441e");
    ///
    /// // Add a signed cookie to the jar
    /// c.signed().add(Cookie::new("key".to_string(), "value".to_string()));
    ///
    /// // Add a permanently signed cookie to the jar
    /// c.permanent().signed()
    ///  .add(Cookie::new("key".to_string(), "value".to_string()));
    /// ```
    #[cfg(feature = "secure")]
    pub fn signed<'b>(&'b self) -> CookieJar<'b> {
        return CookieJar {
            flavor: Flavor::Child(Child {
                parent: self,
                read: verify,
                write: sign,
            }),
        };

        fn verify(root: &Root, cookie: Cookie<'static>) -> Option<Cookie<'static>> {
            secure::verify(&root._key, cookie)
        }

        fn sign(root: &Root, cookie: Cookie<'static>) -> Cookie<'static> {
            secure::sign(&root._key, cookie)
        }
    }

    /// Creates a child encrypted cookie jar.
    ///
    /// All cookies read from the child jar must be encrypted and signed by a
    /// valid key and all cookies written will be encrypted and signed
    /// automatically.
    ///
    /// This API is only available when the `secure` feature is enabled on this
    /// crate.
    ///
    /// # Example
    ///
    /// ```rust
    /// # use cookie::{Cookie, CookieJar};
    /// let c = CookieJar::new(b"f8f9eaf1ecdedff5e5b749c58115441e");
    ///
    /// // Add a signed and encrypted cookie to the jar
    /// c.encrypted().add(Cookie::new("key".to_string(), "value".to_string()));
    ///
    /// // Add a permanently signed and encrypted cookie to the jar
    /// c.permanent().encrypted()
    ///  .add(Cookie::new("key".to_string(), "value".to_string()));
    /// ```
    #[cfg(feature = "secure")]
    pub fn encrypted<'b>(&'b self) -> CookieJar<'b> {
        return CookieJar {
            flavor: Flavor::Child(Child {
                parent: self,
                read: read,
                write: write,
            }),
        };

        fn read(root: &Root, cookie: Cookie<'static>) -> Option<Cookie<'static>> {
            secure::verify_and_decrypt(&root._key, cookie)
        }

        fn write(root: &Root, cookie: Cookie<'static>) -> Cookie<'static> {
            secure::encrypt_and_sign(&root._key, cookie)
        }
    }

    /// Creates a child jar for permanent cookie storage.
    ///
    /// All cookies written to the child jar will have an expiration date 20
    /// years into the future to ensure they stick around for a long time.
    pub fn permanent<'b>(&'b self) -> CookieJar<'b> {
        return CookieJar {
            flavor: Flavor::Child(Child {
                parent: self,
                read: read,
                write: write,
            }),
        };

        fn read(_root: &Root, cookie: Cookie<'static>) -> Option<Cookie<'static>> {
            Some(cookie)
        }

        fn write(_root: &Root, mut cookie: Cookie<'static>) -> Cookie<'static> {
            // Expire 20 years in the future
            cookie.set_max_age(Duration::days(365 * 20));
            let mut now = time::now();
            now.tm_year += 20;
            cookie.set_expires(now);
            cookie
        }
    }

    /// Calculates the changes that have occurred to this cookie jar over time,
    /// returning a vector of `Set-Cookie` headers.
    pub fn delta(&self) -> Vec<Cookie<'static>> {
        let mut ret = Vec::new();
        let root = self.root();
        for cookie in root.removed_cookies.borrow().iter() {
            let mut c = Cookie::new(cookie.clone(), String::new());
            c.set_max_age(Duration::zero());
            let mut now = time::now();
            now.tm_year -= 1;
            c.set_expires(now);
            ret.push(c);
        }

        let map = root.map.borrow();
        for cookie in root.new_cookies.borrow().iter() {
            ret.push(map.get(cookie).unwrap().clone());
        }

        return ret;
    }

    fn try_read(&self, root: &Root, mut cookie: Cookie<'static>) -> Option<Cookie<'static>> {
        let mut jar = self;
        loop {
            match jar.flavor {
                Flavor::Child(Child { read, parent, .. }) => {
                    cookie = match read(root, cookie) {
                        Some(c) => c,
                        None => return None,
                    };
                    jar = parent;
                }
                Flavor::Root(..) => return Some(cookie),
            }
        }
    }

    /// Return an iterator over the cookies in this jar.
    ///
    /// This iterator will only yield valid cookies for this jar. For example if
    /// this is an encrypted child jar then only valid encrypted cookies will be
    /// yielded. If the root cookie jar is iterated over then all cookies will
    /// be yielded.
    pub fn iter(&self) -> Iter {
        let map = self.root().map.borrow();
        Iter {
            jar: self,
            keys: map.keys().cloned().collect(),
        }
    }
}

impl<'a> fmt::Debug for CookieJar<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let root = self.root();
        try!(write!(f, "CookieJar {{"));
        let mut first = true;
        for (name, cookie) in &*root.map.borrow() {
            if !first {
                try!(write!(f, ", "));
            }
            first = false;
            try!(write!(f, "{:?}: {:?}", name, cookie));
        }
        try!(write!(f, " }}"));
        Ok(())
    }
}

impl<'a> Iterator for Iter<'a> {
    type Item = Cookie<'static>;

    fn next(&mut self) -> Option<Cookie<'static>> {
        loop {
            let key = match self.keys.pop() {
                Some(v) => v,
                None => return None,
            };
            let root = self.jar.root();
            let map = root.map.borrow();
            let cookie = match map.get(&key) {
                Some(cookie) => cookie.clone(),
                None => continue,
            };
            match self.jar.try_read(root, cookie) {
                Some(cookie) => return Some(cookie),
                None => {}
            }
        }
    }
}


#[cfg(not(feature = "secure"))]
mod secure {
    pub type SigningKey = ();

    pub fn prepare_key(_key: &[u8]) -> () {
        ()
    }
}

#[cfg(feature = "secure")]
mod secure {
    extern crate openssl;
    extern crate rustc_serialize;

    use ::Cookie;
    use self::openssl::{hash, memcmp, symm};
    use self::openssl::pkey::PKey;
    use self::openssl::sign::Signer;
    use self::openssl::hash::MessageDigest;
    use self::rustc_serialize::base64::{ToBase64, FromBase64, STANDARD};

    pub type SigningKey = Vec<u8>;
    pub const MIN_KEY_LEN: usize = 32;

    pub fn prepare_key(key: &[u8]) -> Vec<u8> {
        if key.len() >= MIN_KEY_LEN {
            key.to_vec()
        } else {
            // Using a SHA-256 hash to normalize key as Rails suggests.
            hash::hash(MessageDigest::sha256(), key).unwrap()
        }
    }

    // If a SHA1 HMAC is good enough for rails, it's probably good enough
    // for us as well:
    //
    // https://github.com/rails/rails/blob/master/activesupport/lib
    //                   /active_support/message_verifier.rb#L70
    pub fn sign(key: &[u8], mut cookie: Cookie<'static>) -> Cookie<'static> {
        let signature = dosign(key, cookie.value()).to_base64(STANDARD);
        let new_cookie_val = format!("{}--{}", cookie.value(), signature);
        cookie.set_value(new_cookie_val);
        cookie
    }

    fn split_value(val: &str) -> Option<(&str, Vec<u8>)> {
        let parts = val.split("--");
        let ext = match parts.last() {
            Some(ext) => ext,
            _ => return None,
        };
        let val_len = val.len();
        if ext.len() == val_len {
            return None;
        }
        let text = &val[..val_len - ext.len() - 2];
        let ext = match ext.from_base64() {
            Ok(sig) => sig,
            Err(..) => return None,
        };

        Some((text, ext))
    }

    pub fn verify(key: &[u8], mut cookie: Cookie<'static>) -> Option<Cookie<'static>> {
        let (text, signature) = match split_value(cookie.value()) {
            Some((text, sig)) => (text.to_string(), sig),
            None => return None,
        };


        let expected = dosign(key, &text);
        if expected.len() != signature.len() || !memcmp::eq(&expected, &signature) {
            return None;
        }

        cookie.set_value(text);
        Some(cookie)
    }

    fn dosign(key: &[u8], val: &str) -> Vec<u8> {
        let pkey = PKey::hmac(key).unwrap();
        let mut signer = Signer::new(MessageDigest::sha1(), &pkey).unwrap();
        signer.update(val.as_bytes()).unwrap();
        signer.finish().unwrap()
    }

    // Implementation details were taken from Rails. See
    // https://github.com/rails/rails/blob/master/activesupport/lib/active_support/message_encryptor.rb#L57
    pub fn encrypt_and_sign(key: &[u8], mut cookie: Cookie<'static>) -> Cookie<'static> {
        let encrypted_data = encrypt_data(key, cookie.value());
        cookie.set_value(encrypted_data);
        sign(key, cookie)
    }

    fn encrypt_data(key: &[u8], val: &str) -> String {
        let iv = random_iv();
        let iv_str = iv.to_base64(STANDARD);

        let cipher = symm::Cipher::aes_256_cbc();
        let encrypted = symm::encrypt(cipher, &key[..MIN_KEY_LEN], Some(&iv), val.as_bytes());
        let mut encrypted_data = encrypted.unwrap().to_base64(STANDARD);

        encrypted_data.push_str("--");
        encrypted_data.push_str(&iv_str);
        encrypted_data
    }

    pub fn verify_and_decrypt(key: &[u8], cookie: Cookie<'static>) -> Option<Cookie<'static>> {
        let mut cookie = match verify(key, cookie) {
            Some(cookie) => cookie,
            None => return None,
        };

        decrypt_data(key, cookie.value())
            .and_then(|data| String::from_utf8(data).ok())
            .map(move |val| {
                cookie.set_value(val);
                cookie
            })
    }

    fn decrypt_data(key: &[u8], val: &str) -> Option<Vec<u8>> {
        let (val, iv) = match split_value(val) {
            Some(pair) => pair,
            None => return None,
        };

        let actual = match val.from_base64() {
            Ok(actual) => actual,
            Err(_) => return None,
        };

        Some(symm::decrypt(symm::Cipher::aes_256_cbc(),
                           &key[..MIN_KEY_LEN],
                           Some(&iv),
                           &actual)
            .unwrap())
    }

    fn random_iv() -> Vec<u8> {
        let mut ret = vec![0; 16];
        openssl::rand::rand_bytes(&mut ret).unwrap();
        return ret;
    }

}

#[cfg(test)]
mod test {
    use {Cookie, CookieJar};

    const KEY: &'static [u8] = b"f8f9eaf1ecdedff5e5b749c58115441e";

    #[test]
    fn short_key() {
        CookieJar::new(b"foo");
    }

    #[test]
    fn simple() {
        let c = CookieJar::new(KEY);

        c.add(Cookie::new("test", ""));
        c.add(Cookie::new("test2", ""));
        c.remove("test");

        assert!(c.find("test").is_none());
        assert!(c.find("test2").is_some());

        c.add(Cookie::new("test3", ""));
        c.clear();

        assert!(c.find("test").is_none());
        assert!(c.find("test2").is_none());
        assert!(c.find("test3").is_none());
    }

    macro_rules! secure_behaviour {
        ($c:ident, $secure:ident) => ({
            $c.$secure().add(Cookie::new("test", "test"));
            assert!($c.find("test").unwrap().value() != "test");
            assert!($c.$secure().find("test").unwrap().value() == "test");

            let mut cookie = $c.find("test").unwrap();
            let new_val = format!("{}l", cookie.value());
            cookie.set_value(new_val);
            $c.add(cookie);
            assert!($c.$secure().find("test").is_none());

            let mut cookie = $c.find("test").unwrap();
            cookie.set_value("foobar");
            $c.add(cookie);
            assert!($c.$secure().find("test").is_none());
        })
    }

    #[cfg(feature = "secure")]
    #[test]
    fn signed() {
        let c = CookieJar::new(KEY);
        secure_behaviour!(c, signed)
    }

    #[cfg(feature = "secure")]
    #[test]
    fn encrypted() {
        let c = CookieJar::new(KEY);
        secure_behaviour!(c, encrypted)
    }

    #[test]
    fn permanent() {
        let c = CookieJar::new(KEY);

        c.permanent().add(Cookie::new("test", "test"));

        let cookie = c.find("test").unwrap();
        assert_eq!(cookie.value(), "test");
        assert_eq!(c.permanent().find("test").unwrap().value(), "test");
        assert!(cookie.expires().is_some());
        assert!(cookie.max_age().is_some());
    }

    #[cfg(feature = "secure")]
    #[test]
    fn chained() {
        let c = CookieJar::new(KEY);
        c.permanent().signed().add(Cookie::new("test", "test"));

        let cookie = c.signed().find("test").unwrap();
        assert_eq!(cookie.value(), "test");
        assert!(cookie.expires().is_some());
        assert!(cookie.max_age().is_some());
    }

    #[cfg(features = "secure")]
    #[test]
    fn iter() {
        let mut c = CookieJar::new(KEY);

        c.add_original(Cookie::new("original", "original"));

        c.add(Cookie::new("test", "test"));
        c.add(Cookie::new("test2", "test2"));
        c.add(Cookie::new("test3", "test3"));
        c.add(Cookie::new("test4", "test4"));

        c.signed().add(Cookie::new("signed", "signed"));

        c.encrypted().add(Cookie::new("encrypted", "encrypted"));

        c.remove("test");

        let cookies = c.iter().collect::<Vec<_>>();
        assert_eq!(cookies.len(), 6);

        let encrypted_cookies = c.encrypted().iter().collect::<Vec<_>>();
        assert_eq!(encrypted_cookies.len(), 1);
        assert_eq!(encrypted_cookies[0].name, "encrypted");

        let signed_cookies = c.signed().iter().collect::<Vec<_>>();
        assert_eq!(signed_cookies.len(), 2);
        assert!(signed_cookies[0].name == "signed" || signed_cookies[1].name == "signed");
    }
}
