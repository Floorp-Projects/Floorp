
#define MOZ_IMPLICIT __attribute__((annotate("moz_implicit")))

void takesChar(char);
void takesShort(short);
void takesInt(int);
void takesLong(long);

void takesUChar(unsigned char);
void takesUShort(unsigned short);
void takesUInt(unsigned int);
void takesULong(unsigned long);

struct InitializedWithInt {
  MOZ_IMPLICIT InitializedWithInt(int);
};

void f() {
  bool b = true;
  char s0 = b;
  short s1 = b;
  int s2 = b;
  long s3 = b;

  unsigned char u0 = b;
  unsigned short u1 = b;
  unsigned u2 = b;
  unsigned long u3 = b;

  takesChar(b);
  takesShort(b);
  takesInt(b);
  takesLong(b);
  takesUChar(b);
  takesUShort(b);
  takesUInt(b);
  takesULong(b);

  InitializedWithInt i = b;
  (InitializedWithInt(b));

  bool x = b;

  int exp = (int)true;

  if (x == b) {}
  if (x != b) {}

  if (b == exp) {}
  if (exp == b) {}

  char* ptr;
  // Shouldn't trigger a checker warning since we are using AllowPointerConditions
  if (ptr) {}
}
