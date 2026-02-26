CC = gcc
AS = tools/vasm.exe
BIN2C = tools/bin2c.exe
RM = rm
STRIP = strip
CFLAGS = -Os -mconsole
LDFLAGS = 

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

bin2c_file = bin2c.o
packer_files = imploder.o amiga_hunk_parser.o
depacker_file = ovl_depacker.bin

all: bin2c.exe ovl_depacker.bin imploder.exe

bin2c.exe: $(bin2c_file)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN2C) $(bin2c_file) $(LIBS)
	$(STRIP) $(BIN2C)

ovl_depacker.bin: ovl_depacker.s
	$(AS) -quiet -devpac -nomsg=2050 -nomsg=2054 -nosym -nomsg=2052 -Fbin -o $(depacker_file) $<
	$(BIN2C) $(depacker_file) $(basename $(depacker_file)).h ovl_depacker

imploder.exe: $(packer_files)
	$(CC) $(CFLAGS) $(LDFLAGS) -o ../$@ $(packer_files) $(LIBS)
	$(STRIP) ../$@

clean:
	-$(RM) $(packer_files) $(depacker_file) $(bin2c_file)
