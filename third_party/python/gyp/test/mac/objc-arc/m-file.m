#if !__has_feature(objc_arc)
#error "ObjC files with CLANG_ENABLE_OBJC_ARC should be ARC'd!"
#endif

#if !__has_feature(objc_arc_weak)
#error "Weak references should always be enabled for ARC."
#endif

void m_fun() {}
