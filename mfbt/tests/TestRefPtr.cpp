/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RefPtr.h"
#include "mozilla/RefCounted.h"

using mozilla::RefCounted;

class Foo : public RefCounted<Foo>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(Foo)

  Foo() : mDead(false) {}

  static int sNumDestroyed;

  ~Foo()
  {
    MOZ_ASSERT(!mDead);
    mDead = true;
    sNumDestroyed++;
  }

private:
  bool mDead;
};
int Foo::sNumDestroyed;

struct Bar : public Foo {};

already_AddRefed<Foo>
NewFoo()
{
  RefPtr<Foo> f(new Foo());
  return f.forget();
}

already_AddRefed<Foo>
NewBar()
{
  RefPtr<Bar> bar = new Bar();
  return bar.forget();
}

void
GetNewFoo(Foo** aFoo)
{
  *aFoo = new Bar();
  // Kids, don't try this at home
  (*aFoo)->AddRef();
}

void
GetNewFoo(RefPtr<Foo>* aFoo)
{
  *aFoo = new Bar();
}

already_AddRefed<Foo>
GetNullFoo()
{
  return 0;
}

int
main()
{
  MOZ_RELEASE_ASSERT(0 == Foo::sNumDestroyed);
  {
    RefPtr<Foo> f = new Foo();
    MOZ_RELEASE_ASSERT(f->refCount() == 1);
  }
  MOZ_RELEASE_ASSERT(1 == Foo::sNumDestroyed);

  {
    RefPtr<Foo> f1 = NewFoo();
    RefPtr<Foo> f2(NewFoo());
    MOZ_RELEASE_ASSERT(1 == Foo::sNumDestroyed);
  }
  MOZ_RELEASE_ASSERT(3 == Foo::sNumDestroyed);

  {
    RefPtr<Foo> b = NewBar();
    MOZ_RELEASE_ASSERT(3 == Foo::sNumDestroyed);
  }
  MOZ_RELEASE_ASSERT(4 == Foo::sNumDestroyed);

  {
    RefPtr<Foo> f1;
    {
      f1 = new Foo();
      RefPtr<Foo> f2(f1);
      RefPtr<Foo> f3 = f2;
      MOZ_RELEASE_ASSERT(4 == Foo::sNumDestroyed);
    }
    MOZ_RELEASE_ASSERT(4 == Foo::sNumDestroyed);
  }
  MOZ_RELEASE_ASSERT(5 == Foo::sNumDestroyed);

  {
    {
      RefPtr<Foo> f = new Foo();
      RefPtr<Foo> g = f.forget();
    }
    MOZ_RELEASE_ASSERT(6 == Foo::sNumDestroyed);
  }

  {
    RefPtr<Foo> f = new Foo();
    GetNewFoo(getter_AddRefs(f));
    MOZ_RELEASE_ASSERT(7 == Foo::sNumDestroyed);
  }
  MOZ_RELEASE_ASSERT(8 == Foo::sNumDestroyed);

  {
    RefPtr<Foo> f = new Foo();
    GetNewFoo(&f);
    MOZ_RELEASE_ASSERT(9 == Foo::sNumDestroyed);
  }
  MOZ_RELEASE_ASSERT(10 == Foo::sNumDestroyed);

  {
    RefPtr<Foo> f1 = new Bar();
  }
  MOZ_RELEASE_ASSERT(11 == Foo::sNumDestroyed);

  {
    RefPtr<Foo> f = GetNullFoo();
    MOZ_RELEASE_ASSERT(11 == Foo::sNumDestroyed);
  }
  MOZ_RELEASE_ASSERT(11 == Foo::sNumDestroyed);

  return 0;
}

