extern crate rustc_serialize;
extern crate toml;
use toml::encode_str;

#[derive(Debug, Clone, Hash, PartialEq, Eq, RustcEncodable, RustcDecodable)]
struct User {
    pub name: String,
    pub surname: String,
}

#[derive(Debug, Clone, Hash, PartialEq, Eq, RustcEncodable, RustcDecodable)]
struct Users {
    pub user: Vec<User>,
}

#[derive(Debug, Clone, Hash, PartialEq, Eq, RustcEncodable, RustcDecodable)]
struct TwoUsers {
    pub user0: User,
    pub user1: User,
}

#[test]
fn no_unnecessary_newlines_array() {
    assert!(!encode_str(&Users {
            user: vec![
                    User {
                        name: "John".to_string(),
                        surname: "Doe".to_string(),
                    },
                    User {
                        name: "Jane".to_string(),
                        surname: "Dough".to_string(),
                    },
                ],
        })
        .starts_with("\n"));
}

#[test]
fn no_unnecessary_newlines_table() {
    assert!(!encode_str(&TwoUsers {
            user0: User {
                name: "John".to_string(),
                surname: "Doe".to_string(),
            },
            user1: User {
                name: "Jane".to_string(),
                surname: "Dough".to_string(),
            },
        })
        .starts_with("\n"));
}
