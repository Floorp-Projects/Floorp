// bindgen-flags: --objc-extern-crate -- -x objective-c
// bindgen-osx-only

@interface Foo
- (void)method;
- (void)methodWithInt:(int)foo;
- (void)methodWithFoo:(Foo*)foo;
- (int)methodReturningInt;
- (Foo*)methodReturningFoo;
- (void)methodWithArg1:(int)intvalue andArg2:(char*)ptr andArg3:(float)floatvalue;
@end
