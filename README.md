Imploder exe/data files cruncher/decruncher.
Based on the reversed implode/explode routines from lab313ru.

Imploder packs quite well, the decruncher is relatively fast
and can decrunch in-place without any overhead.

68000 asm decruncher taken and improved from a Team 17 game.

Windows and Amiga executables provided.

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