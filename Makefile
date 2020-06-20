CC=gcc
WIN32CC=i686-w64-mingw32-gcc-win32
WIN64CC=x86_64-w64-mingw32-gcc-win32
CFLAGS = -s -Wall -Wpedantic
TARGETS = neresex neresex.exe neresex.x64.exe

default: $(TARGETS)

neresex: neresex.c
	$(CC) $(CFLAGS) $^ -o $@

neresex.exe: neresex.c
	$(WIN32CC) $(CFLAGS) $^ -o $@

neresex.x64.exe: neresex.c
	$(WIN64CC) $(CFLAGS) $^ -o $@

clean:
	rm $(TARGETS)

