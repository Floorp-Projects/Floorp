struct A {
  unsigned char x;
  unsigned b1 : 1;
  unsigned b2 : 1;
  unsigned b3 : 1;
  unsigned b4 : 1;
  unsigned b5 : 1;
  unsigned b6 : 1;
  unsigned b7 : 1;
  unsigned b8 : 1;
  unsigned b9 : 1;
  unsigned b10 : 1;
  unsigned char y;
};

struct B {
  unsigned foo : 31;
  unsigned char bar : 1;
};

struct C {
  unsigned char x;
  unsigned b1 : 1;
  unsigned b2 : 1;
  unsigned baz;
};

struct Date1 {
   unsigned short nWeekDay  : 3;    // 0..7   (3 bits)
   unsigned short nMonthDay : 6;    // 0..31  (6 bits)
   unsigned short nMonth    : 5;    // 0..12  (5 bits)
   unsigned short nYear     : 8;    // 0..100 (8 bits)
};

struct Date2 {
   unsigned short nWeekDay  : 3;    // 0..7   (3 bits)
   unsigned short nMonthDay : 6;    // 0..31  (6 bits)
   unsigned short nMonth    : 5;    // 0..12  (5 bits)
   unsigned short nYear     : 8;    // 0..100 (8 bits)
   unsigned char byte;
};
