// bindgen-flags: --enable-cxx-namespaces

namespace foo {
  struct Bar {
    int baz;
  };
}

namespace bar {
  /// <div rustbindgen replaces="foo::Bar"></div>
  struct Bar_Replacement {
    int bazz;
  };
};

typedef foo::Bar ReferencesBar;
