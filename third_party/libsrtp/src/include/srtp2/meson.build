# Copy public headers scattered across the source tree into a single directory
# so that we can use it in declare_dependency()
foreach h : public_headers
  configure_file(input: h,
    output: '@BASENAME@.h',
    copy: true)
endforeach
public_incs = include_directories('.')
