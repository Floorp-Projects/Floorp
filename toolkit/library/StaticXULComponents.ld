SECTIONS {
  .data.rel.ro : {
    PROVIDE_HIDDEN(__start_kPStaticModules = .);
    *(kPStaticModules)
    PROVIDE_HIDDEN(__stop_kPStaticModules = .);
  }
}
