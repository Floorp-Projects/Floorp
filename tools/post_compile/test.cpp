static short  gLook = 0;

static int add( int a, int b)
{
    return a + b;
}

long main( void )
{
    long	retVal = 0;
    for (int i=0; i<100; i++)
    {
        retVal = add( retVal, i) + gLook;
    }

    return retVal;
}
