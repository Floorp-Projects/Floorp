use fluent_syntax::parser::parse_runtime;

fn iai_parse_ctx_runtime() {
    let files = &[
        include_str!("contexts/browser/appmenu.ftl"),
        include_str!("contexts/browser/browser.ftl"),
        include_str!("contexts/browser/menubar.ftl"),
        include_str!("contexts/preferences/preferences.ftl"),
    ];
    for source in files {
        parse_runtime(*source).expect("Parsing of the FTL failed.");
    }
}

iai::main!(iai_parse_ctx_runtime);
