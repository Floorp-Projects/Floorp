
// misc-suspicious-semicolon
void nop();
void fail1()
{
    int x = 0;
    if(x > 5); nop();
}
