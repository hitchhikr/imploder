Imploder exe/data files cruncher/decruncher.
Based on the reversed implode/explode routines from lab313ru.

Imploder packs quite well, the decruncher is relatively fast
and can decrunch in-place without any overhead.

68000 asm decruncher taken and improved from a Team 17 game.

Windows and Amiga executables provided.

v1.2b:

- Fixed exit().

v1.2:

- Added support for Amiga executables compression
  with an overlay decruncher.

v1.1:

- Fixed lha archive date stamp.
- Added C stuff in explode_68000.asm.