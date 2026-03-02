CC = gcc
AS = tools/vasm.exe
BIN2C = tools/bin2c.exe
RM = rm
CFLAGS = -Os -mconsole
LDFLAGS = -s

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

bin2c_file = bin2c.o
packer_files = imploder.o amiga_parser.o x68000_parser.o endian.o
amiga_depacker_file = amiga_depacker.bin
x68000_depacker_file = x68000_depacker.bin

all: bin2c.exe amiga_depacker.bin x68000_depacker.bin imploder.exe

bin2c.exe: $(bin2c_file)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN2C) $(bin2c_file) $(LIBS)

amiga_depacker.bin: amiga_depacker.s
	$(AS) -quiet -devpac -nomsg=2050 -nomsg=2054 -nosym -nomsg=2052 -Fbin -o $(amiga_depacker_file) $<
	$(BIN2C) $(amiga_depacker_file) $(basename $(amiga_depacker_file)).h amiga_depacker

x68000_depacker.bin: x68000_depacker.s
	$(AS) -quiet -devpac -nomsg=2050 -nomsg=2054 -nosym -nomsg=2052 -Fbin -o $(x68000_depacker_file) $<
	$(BIN2C) $(x68000_depacker_file) $(basename $(x68000_depacker_file)).h x68000_depacker

imploder.exe: $(packer_files)
	$(CC) $(CFLAGS) $(LDFLAGS) -o ../$@ $(packer_files) $(LIBS)

clean:
	-$(RM) $(packer_files) $(amiga_depacker_file) $(x68000_depacker_file) $(bin2c_file)
