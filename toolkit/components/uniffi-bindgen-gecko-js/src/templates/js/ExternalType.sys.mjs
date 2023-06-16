import {
  {{ ffi_converter }},
  {{ name }},
} from "{{ self.external_type_module(crate_name) }}";

// Export the FFIConverter object to make external types work.
export { {{ ffi_converter }}, {{ name }} };
