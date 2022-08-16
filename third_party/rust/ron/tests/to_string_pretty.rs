use ron::ser::{to_string_pretty, PrettyConfig};
use ron::to_string;
use serde::{Deserialize, Serialize};

#[derive(Debug, PartialEq, Deserialize, Serialize)]
struct Point {
    x: f64,
    y: f64,
}

#[test]
fn test_struct_names() {
    let value = Point { x: 1.0, y: 2.0 };
    let struct_name = to_string_pretty(&value, PrettyConfig::default().struct_names(true));
    assert_eq!(
        struct_name,
        Ok("Point(\n    x: 1.0,\n    y: 2.0,\n)".to_string())
    );
    let no_struct_name = to_string(&value);
    assert_eq!(no_struct_name, Ok("(x:1.0,y:2.0)".to_string()));
}
