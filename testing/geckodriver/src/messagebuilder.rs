use std::collections::{TreeMap, HashMap};
use regex::{Regex, Captures};
use serialize::json;

use hyper::method::{Method, Get, Post};

use command::{WebDriverMessage, WebDriverCommand};
use common::{WebDriverResult, WebDriverError, UnknownCommand};

#[deriving(Clone)]
pub enum MatchType {
    MatchNewSession,
    MatchGet,
    MatchGetCurrentUrl
}

#[deriving(Clone)]
pub struct RequestMatcher {
    method: Method,
    path_regexp: Regex,
    match_type: MatchType
}

impl RequestMatcher {
    pub fn new(method: Method, path: &str, match_type: MatchType) -> RequestMatcher {
        let path_regexp = RequestMatcher::compile_path(path);
        RequestMatcher {
            method: method,
            path_regexp: path_regexp,
            match_type: match_type
        }
    }

    pub fn get_match<'t>(&'t self, method: Method, path: &'t str) -> Option<Captures> {
        println!("{} {}", method, path);
        if method == self.method {
            self.path_regexp.captures(path)
        } else {
            None
        }
    }

    fn compile_path(path: &str) -> Regex {
        let mut rv = String::new();
        rv.push_str("^");
        let mut components = path.split('/');
        for component in components {
            if component.starts_with("{") {
                if !component.ends_with("}") {
                    fail!("Invalid url pattern")
                }
                rv.push_str(format!("(?P<{}>[^/]+)/", component[1..component.len()-1])[]);
            } else {
                rv.push_str(format!("{}/", component)[]);
            }
        }
        //Remove the trailing /
        rv.pop();
        rv.push_str("$");
        //This will fail at runtime if the regexp is invalid
        Regex::new(rv[]).unwrap()
    }
}

pub struct MessageBuilder {
    http_matchers: Vec<(Method, RequestMatcher)>
}

impl MessageBuilder {
    pub fn new() -> MessageBuilder {
        MessageBuilder {
            http_matchers: vec![]
        }
    }

    pub fn from_http(&self, method: Method, path: &str, body: &str) -> WebDriverResult<WebDriverMessage> {
        println!("{} {}", method, path)
        for &(ref match_method, ref matcher) in self.http_matchers.iter() {
            println!("{} {}", match_method, matcher.path_regexp);
            if method == *match_method {
                let captures = matcher.get_match(method.clone(), path);
                if captures.is_some() {
                    return WebDriverMessage::from_http(matcher.match_type,
                                                       &captures.unwrap(),
                                                       body)
                }
            }
        }
        Err(WebDriverError::new(None,
                                UnknownCommand,
                                format!("{} did not match a known command", path)[]))
    }

    pub fn add(&mut self, method: Method, path: &str, match_type: MatchType) {
        let http_matcher = RequestMatcher::new(method.clone(), path, match_type);
        self.http_matchers.push((method, http_matcher));
    }
}

pub fn get_builder() -> MessageBuilder {
    let mut builder = MessageBuilder::new();
    let matchers = vec![(Post, "/session", MatchNewSession),
                        (Post, "/session/{sessionId}/url", MatchGet),
                        (Get, "/session/{sessionId}/url", MatchGetCurrentUrl)];
    for &(ref method, ref url, ref match_type) in matchers.iter() {
        println!("{} {}", method, url);
        builder.add(method.clone(), *url, *match_type);
    }
    builder
}
