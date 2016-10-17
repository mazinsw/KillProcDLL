CC     = gcc
RM     = rm -f
OBJS   = KillProcDLL.o \
         pluginapi.o

LIBS   = -shared -Wl,--kill-at -m64
CFLAGS = -m64 -fno-diagnostics-show-option

.PHONY: ../bin/x64-ansi/KillProcDLL.dll clean clean-after

all: ../bin/x64-ansi/KillProcDLL.dll

clean:
	$(RM) $(OBJS) ../bin/x64-ansi/KillProcDLL.dll

clean-after:
	$(RM) $(OBJS)

../bin/x64-ansi/KillProcDLL.dll: $(OBJS)
	$(CC) -Wall -s -O2 -o $@ $(OBJS) $(LIBS)

KillProcDLL.o: KillProcDLL.c exdll.h
	$(CC) -Wall -s -O2 -c $< -o $@ $(CFLAGS)

pluginapi.o: pluginapi.c pluginapi.h
	$(CC) -Wall -s -O2 -c $< -o $@ $(CFLAGS)

