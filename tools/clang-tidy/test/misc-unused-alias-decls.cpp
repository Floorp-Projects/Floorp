// https://clang.llvm.org/extra/clang-tidy/checks/misc-unused-alias-decls.html

namespace n1 {
    namespace n2 {
         namespace n3 {
             int qux = 42;
         }
    }
}

namespace n1_unused = ::n1;     // WARNING
namespace n12_unused = n1::n2;  // WARNING
namespace n123 = n1::n2::n3;    // OK

int test()
{
  return n123::qux;
}
