/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern "C" int mult(int l, int r);

extern "C" {

inline
int
farfle(int a, int b)
{
    return a * b + a / b;
}

static
int
ballywhoo(int a, int b)
{
    // So it can't get inlined
    if (a > 4)
        ballywhoo(a - 1, a + 1);

    return a + b;
}

static
int
blimpyburger(char a, int b)
{
    if (a == 'f')
        return b;

    return 0;
}

}

class foo
{
public:
    static int get_flooby() { static int flooby = 12; return flooby; }

    static int divide(int a, int b);

    foo() {}
    foo(int a);
    virtual ~foo() {}

    int bar();

    int baz(int a) { return a ? baz(a - 1) : 0; }
};

foo::foo(int a)
{
}

int
foo::divide(int a, int b)
{
    static int phlegm = 12;
    return a / b * ++phlegm;
}

int
foo::bar()
{
    return 12;
}

int main(int argc, char* argv[])
{
    int a = mult(5, 4);
    a = ballywhoo(5, 2);
    a = farfle(a, 6);
    a = blimpyburger('q', 4);

    foo f;
    f.get_flooby();
    a = f.bar();

    a = foo::divide(a, 12) + f.baz(6);

    foo f2(12);
    f2.baz(15);

    return a > 99 ? 1 : 0;
}
