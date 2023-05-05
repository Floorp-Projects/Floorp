const { {{ name }}, {{ ffi_converter }} } = ChromeUtils.import(
   "{{ self.external_type_module(crate_name) }}"
);
EXPORTED_SYMBOLS.push("{{ name }}");
