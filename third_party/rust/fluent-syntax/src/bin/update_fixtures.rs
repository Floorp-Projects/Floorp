use std::fs;
use std::io;

use fluent_syntax::parser::parse;

fn read_file(path: &str) -> Result<String, io::Error> {
    fs::read_to_string(path)
}

fn write_file(path: &str, source: &str) -> Result<(), io::Error> {
    fs::write(path, source)
}

fn main() {
    let samples = &["menubar", "preferences", "simple"];
    let contexts = &["browser", "preferences"];

    for sample in samples {
        let path = format!("./benches/{}.ftl", sample);
        let source = read_file(&path).unwrap();
        let ast = parse(source).unwrap();
        let target_json = serde_json::to_string_pretty(&ast).unwrap();
        let new_path = format!("./tests/fixtures/benches/{}.json", sample);
        write_file(&new_path, &target_json).unwrap();
    }

    for test in contexts {
        let paths = fs::read_dir(format!("./benches/contexts/{}", test)).unwrap();
        for path in paths.into_iter() {
            let p = path.unwrap().path();
            let file_name = p.file_name().unwrap().to_str().unwrap();
            let path = p.to_str().unwrap();
            let source = read_file(path).unwrap();
            let ast = parse(source).unwrap();
            let target_json = serde_json::to_string_pretty(&ast).unwrap();
            let new_path = format!(
                "./tests/fixtures/benches/contexts/{}/{}",
                test,
                file_name.replace(".ftl", ".json")
            );
            write_file(&new_path, &target_json).unwrap();
        }
    }
}
