.macro zip_data name, path
  .global \name
  .data
  .balign 16
  \name:
  .incbin "\path"
  .L\name\()_END:
  .size \name, .L\name\()_END-\name
  .global \name\()_SIZE
  .data
  .balign 4
  \name\()_SIZE:
  .int .L\name\()_END-\name
.endm

zip_data TEST_ZIP, "test.zip"
zip_data NO_CENTRAL_DIR_ZIP, "no_central_dir.zip"
