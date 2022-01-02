use fluent_syntax::parser::parse;
use std::env;
use std::fs::File;
use std::io;
use std::io::Read;

fn read_file(path: &str) -> Result<String, io::Error> {
    let mut f = File::open(path)?;
    let mut s = String::new();
    f.read_to_string(&mut s)?;
    Ok(s)
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let source = read_file(args.get(1).expect("Pass an argument")).expect("Failed to fetch file");

    let (ast, errors) = match parse(source.as_str()) {
        Ok(ast) => (ast, None),
        Err((ast, err)) => (ast, Some(err)),
    };

    #[cfg(feature = "json")]
    {
        let target_json = serde_json::to_string_pretty(&ast).unwrap();
        println!("{}", target_json);
    }
    #[cfg(not(feature = "json"))]
    {
        use std::fmt::Write;
        let mut result = String::new();
        write!(result, "{:#?}", ast).unwrap();
        println!("{}", result);
    }

    if let Some(errors) = errors {
        println!("\n======== Errors ========== \n");
        for err in errors {
            println!("Err: {:#?}", err);
        }
    }
}
