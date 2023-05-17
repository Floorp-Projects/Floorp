#if __has_feature(objc_arc)
#error "ObjC++ files without CLANG_ENABLE_OBJC_ARC should not be ARC'd!"
#endif

#if !__has_feature(objc_arc_weak)
#error "With CLANG_ENABLE_OBJC_WEAK, weak references should be enabled."
#endif

void mm_fun() {}
