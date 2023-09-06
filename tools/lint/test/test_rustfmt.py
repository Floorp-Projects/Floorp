import mozunit

LINTER = "rustfmt"
fixed = 0


def test_good(lint, config, paths):
    results = lint(paths("subdir/good.rs"))
    print(results)
    assert len(results) == 0


def test_basic(lint, config, paths):
    results = lint(paths("subdir/bad.rs"))
    print(results)
    assert len(results) >= 1

    assert "Reformat rust" in results[0].message
    assert results[0].level == "warning"
    assert results[0].lineno == 4
    assert "bad.rs" in results[0].path
    assert "Print text to the console" in results[0].diff


def test_dir(lint, config, paths):
    results = lint(paths("subdir/"))
    print(results)
    assert len(results) >= 4

    assert "Reformat rust" in results[0].message
    assert results[0].level == "warning"
    assert results[0].lineno == 4
    assert "bad.rs" in results[0].path
    assert "Print text to the console" in results[0].diff

    assert "Reformat rust" in results[1].message
    assert results[1].level == "warning"
    assert results[1].lineno == 4
    assert "bad2.rs" in results[1].path
    assert "Print text to the console" in results[1].diff


def test_fix(lint, create_temp_file):
    contents = """fn main() {
    // Statements here are executed when the compiled binary is called

    // Print text to the console
    println!("Hello World!");
    let mut a;
    let mut b=1;
    let mut vec = Vec::new();
    vec.push(1);
    vec.push(2);


    for x in 5..10 - 5 {
        a = x;
    }

    }
"""

    path = create_temp_file(contents, "bad.rs")
    lint([path], fix=True)
    assert fixed == 3


if __name__ == "__main__":
    mozunit.main()
