// https://clang.llvm.org/extra/clang-tidy/checks/bugprone-unused-raii.html

struct scoped_lock
{
  scoped_lock() {}
  ~scoped_lock() {}
};

#define SCOPED_LOCK_MACRO(m) scoped_lock()

struct trivial_scoped_lock
{
  trivial_scoped_lock() {}
};

scoped_lock test()
{
  scoped_lock(); // misc-unused-raii warning!

  SCOPED_LOCK_MACRO(); // no warning for macros

  trivial_scoped_lock(); // no warning for trivial objects without destructors

  return scoped_lock(); // no warning for return values
}
