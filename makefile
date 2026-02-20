CC = gcc
RM = rm
STRIP = strip
CFLAGS = -Os -mconsole

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

files = imploder.o

all: imploder.exe

imploder.exe: $(files)
	$(CC) $(CFLAGS) -o $@ $(files) $(LIBS)
	$(STRIP) $@

clean:
	-$(RM) $(files)
