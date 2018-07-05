// https://clang.llvm.org/extra/clang-tidy/checks/bugprone-bool-pointer-implicit-conversion.html

bool test(bool* pointer_to_bool, int* pointer_to_int)
{
  if (pointer_to_bool) { // warning for pointer to bool
  }

  if (pointer_to_int) { // no warning for pointer to int
  }

  if (!pointer_to_bool) { // no warning, but why not??
  }

  if (pointer_to_bool != nullptr) { // no warning for nullptr comparison
  }

  // no warning on return, but why not??
  // clang-tidy bug: https://bugs.llvm.org/show_bug.cgi?id=38060
  return pointer_to_bool;
}
