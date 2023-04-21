const char* s_source_includes = R"w2c_template(#include <assert.h>
)w2c_template"
R"w2c_template(#include <math.h>
)w2c_template"
R"w2c_template(#include <stdarg.h>
)w2c_template"
R"w2c_template(#include <stddef.h>
)w2c_template"
R"w2c_template(#include <string.h>
)w2c_template"
R"w2c_template(#if defined(_MSC_VER)
)w2c_template"
R"w2c_template(#include <intrin.h>
)w2c_template"
R"w2c_template(#include <malloc.h>
)w2c_template"
R"w2c_template(#define alloca _alloca
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#include <alloca.h>
)w2c_template"
R"w2c_template(#endif
)w2c_template"
;
