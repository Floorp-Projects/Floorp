function categoryExists(category, entry) {
  try {
    Services.catMan.getCategoryEntry(category, entry);
    return true;
  } catch (e) {
    return false;
  }
}

function run_test() {
  Components.manager.autoRegister(
    do_get_file("data/process_directive.manifest")
  );

  let isChild =
    Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

  Assert.equal(categoryExists("directives-test", "main-process"), !isChild);
  Assert.equal(categoryExists("directives-test", "content-process"), isChild);
}
