#include "gtest/gtest.h"

#include "nsHttp.h"

TEST(TestHttpAtom, AtomComparison)
{
  mozilla::net::nsHttpAtom atom(mozilla::net::nsHttp::Host);
  mozilla::net::nsHttpAtom same_atom(mozilla::net::nsHttp::Host);
  mozilla::net::nsHttpAtom different_atom(mozilla::net::nsHttp::Accept);

  ASSERT_EQ(atom, atom);
  ASSERT_EQ(atom, mozilla::net::nsHttp::Host);
  ASSERT_EQ(mozilla::net::nsHttp::Host, atom);
  ASSERT_EQ(atom, same_atom);
  ASSERT_EQ(atom.get(), same_atom.get());
  ASSERT_EQ(atom.get(), mozilla::net::nsHttp::Host.get());

  ASSERT_NE(atom, different_atom);
  ASSERT_NE(atom.get(), different_atom.get());
}

TEST(TestHttpAtom, LiteralComparison)
{
  ASSERT_EQ(mozilla::net::nsHttp::Host, mozilla::net::nsHttp::Host);
  ASSERT_NE(mozilla::net::nsHttp::Host, mozilla::net::nsHttp::Accept);

  ASSERT_EQ(mozilla::net::nsHttp::Host.get(), mozilla::net::nsHttp::Host.get());
  ASSERT_NE(mozilla::net::nsHttp::Host.get(),
            mozilla::net::nsHttp::Accept.get());
}

TEST(TestHttpAtom, Validity)
{
  mozilla::net::nsHttpAtom atom(mozilla::net::nsHttp::Host);
  ASSERT_TRUE(atom);

  mozilla::net::nsHttpAtom atom_empty;
  ASSERT_FALSE(atom_empty);
}
