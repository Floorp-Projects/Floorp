struct header
{
    char proto;
    unsigned int size __attribute__ ((packed));
    unsigned char data[] __attribute__ ((aligned(8)));
} __attribute__ ((aligned, packed));