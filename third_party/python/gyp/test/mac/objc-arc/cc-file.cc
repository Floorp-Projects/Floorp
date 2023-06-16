#if __has_feature(objc_arc) || __has_feature(objc_arc_weak)
#error "C++ files shouldn't be ARC'd!"
#endif

void cc_fun() {}
