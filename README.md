Imploder exe/data files cruncher/decruncher.
Based on the reversed implode/explode routines from lab313ru.

Imploder packs quite well, the decruncher is relatively fast
and can decrunch in-place without any overhead.

It can pack Amiga executables, X68000 executables or plain data files.

Packed executables use "overlay" depackers:

A forged header instructing the OS to only load a fraction of the file
(just containing a 2nd level loader and a data depacker),
the exe then loads, depacks and relocates the data it contains "in place"
thus only using a very tiny amount of memory overhead.

68000 asm decruncher taken and improved from a Team 17 game.

Windows and Amiga executables provided.

v1.4b:

- Fixed some potential memory leaks after packing Amiga exes.

v1.4:

- Can now optionally specify imploding mode/strength
  as command argument (from 0 to 11) (default is 11).
- Can now pack X68000 .x executables with an overlay depacker.

v1.3:

- Crashed when trying to pack exe with bss sections.
- Tried to pack destination file when supplying
  a destination exe filename.

v1.2b:

- Fixed exit().

v1.2:

- Added support for Amiga executables compression
  with an overlay decruncher.

v1.1:

- Fixed lha archive date stamp.
- Added C stuff in explode_68000.asm.