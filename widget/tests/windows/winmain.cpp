#include <windows.h>

extern void WidgetTest();

void main(int argc, char **argv)
{
    WidgetTest();
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, 
    int nCmdShow)
{
    WidgetTest();
}

