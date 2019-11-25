class P {};

void bar(P) {}

void foo(int n) {
    P x;
    for (int i = n; i >= 0; --i) {
        bar(static_cast<P&&>(x));
    }
}
