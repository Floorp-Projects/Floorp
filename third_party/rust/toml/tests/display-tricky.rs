extern crate toml;
#[macro_use] extern crate serde_derive;

#[derive(Debug, Serialize, Deserialize)]
pub struct Recipe {
    pub name: String,
    pub description: Option<String>,
    #[serde(default)]
    pub modules: Vec<Modules>,
    #[serde(default)]
    pub packages: Vec<Packages>
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Modules {
    pub name: String,
    pub version: Option<String>
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Packages {
    pub name: String,
    pub version: Option<String>
}

#[test]
fn both_ends() {
    let recipe_works = toml::from_str::<Recipe>(r#"
        name = "testing"
        description = "example"
        modules = []

        [[packages]]
        name = "base"
    "#).unwrap();
    toml::to_string(&recipe_works).unwrap();

    let recipe_fails = toml::from_str::<Recipe>(r#"
        name = "testing"
        description = "example"
        packages = []

        [[modules]]
        name = "base"
    "#).unwrap();

    let recipe_toml = toml::Value::try_from(recipe_fails).unwrap();
    recipe_toml.to_string();
}
