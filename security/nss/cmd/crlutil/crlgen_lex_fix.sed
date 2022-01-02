/<unistd.h>/ {
        i #ifdef _WIN32
	i #include <io.h>
	i #else
        a #endif
}
