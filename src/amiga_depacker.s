; ============================================================
; Impoder v1.4b overlay Amiga depacker
; Written by hitchhikr (inspired by Titanics Cruncher).
; ============================================================
                    opt     o+
                    opt     all+

; ============================================================
_LVOAllocMem        equ     -198
_LVOFreeMem         equ     -210
_LVOOldOpenLibrary  equ     -408
_LVOCloseLibrary    equ     -414
_LVOCacheClearU     equ     -636
_LVORead            equ     -42
_LVOSeek            equ     -66

LIB_VERSION         equ     20
OFFSET_BEGINNING	equ     -1
RETURN_OK	        equ     0
RETURN_FAIL         equ     20

                    rsreset
RELOCATIONS         rs.l    1
DEPACKED_SIZE       rs.l    1
MEM_TYPE            rs.l    1
PACKED_SIZE         rs.l    1
STRUCT_LEN          rs.b    0

; ============================================================
depacker_start:
                    bra.w   begin
                    ; magic
                    dc.l    $ABCD
file_handle:
                    dc.l    0
                    dc.l    0,0,0,$5BA0
                    dc.b    7,"Overlay"
DOSName:
                    dc.b    "dos.library",0
                    ; +36 for exe header
                    ; +16 for exe footer
overlay_start:
                    dc.l    (depacker_end+16)-depacker_start+36
old_stack:          dc.l    0

; ============================================================
begin:
                    movem.l d0-a3/a5/a6,-(a7)
                    move.l  a7,old_stack-depacker_start(a4)
                    lea     DOSName(pc),a1
                    move.l  4.w,a6
                    jsr     _LVOOldOpenLibrary(a6)
                    move.l  d0,a5
                    bne     .dos_opened
.error:
                    move.l  old_stack(pc),a7
                    movem.l (a7)+,d0-a3/a5/a6
                    moveq   #RETURN_FAIL,d0
                    rts
.dos_opened:
                    ; go to the start of the data
                    move.l  file_handle(pc),d1
                    move.l  overlay_start(pc),d2
                    moveq   #OFFSET_BEGINNING,d3
                    jsr     _LVOSeek(a5)
                    ; number of hunks to depack
                    subq.l  #4,a7
                    lea     (a7),a0
                    moveq   #4,d3
                    bsr     .read_data
                    bne     .error
                    move.l  (a7)+,d7
                    move.l  d7,d6
                    move.l  d7,d0
                    ; -(16 bytes * number of hunks)
                    addq.l  #1,d0
                    lsl.l   #4,d0
                    move.l  d0,-(a7)
                    moveq   #1,d1
                    jsr     _LVOAllocMem(a6)
                    tst.l   d0
                    beq     .error
                    move.l  d0,a2
                    move.l  a2,-(a7)
                    ; prepare hunks memory blocks
                    move.l  depacker_start-4(pc),a1
                    add.l   a1,a1
                    add.l   a1,a1
                    lea     (a1),a4
.alloc_hunks:
                    lea     RELOCATIONS(a2),a0
                    moveq   #STRUCT_LEN,d3
                    bsr     .read_data
                    bne.b   .error
                    movem.l DEPACKED_SIZE(a2),d0/d1
                    move.l  a1,-(a7)
                    jsr     _LVOAllocMem(a6)
                    move.l  (a7)+,a1
                    tst.l   d0
                    beq     .error
                    addq.l  #4,d0
                    ; block address
                    move.l  d0,d1
                    lsr.l   #2,d0
                    ; chain it with the new memblock address
                    ; at previous block pointer
                    move.l  d0,(a1)
                    move.l  d1,a1
                    ; allocated new memblock size
                    ; in new memblock size slot (offset 0)
                    move.l  DEPACKED_SIZE(a2),-4(a1)
                    lea     STRUCT_LEN(a2),a2
                    dbf     d7,.alloc_hunks
                    ; last in memblocks chain
                    clr.l   (a1)
                    move.l  (a7),a2
.depack_hunks:
                    ; new memblock address
                    move.l  (a4),a4
                    add.l   a4,a4
                    add.l   a4,a4
                    lea     4(a4),a0
                    ; packed size
                    move.l  PACKED_SIZE(a2),d3
                    bmi     .no_relocs
                    ; read the packed data
                    bsr     .read_data
                    bne     .error
                    ; depack them
                    lea     4(a4),a0
                    lea     (a0),a1
                    bsr     _explode
                    beq     .error
                    ; relocations pos in data
                    add.l   RELOCATIONS(a2),a0
                    move.l  a0,d7
.reloc_hunks:
                    ; number of relocs
                    move.l  (a0)+,d0
                    beq     .no_relocs
                    ; hunk number to patch
                    move.l  (a0)+,d1
                    move.l  depacker_start-4(pc),a1
                    add.l   a1,a1
                    add.l   a1,a1
.go_to_hunk_to_reloc:
                    move.l  (a1),a1
                    add.l   a1,a1
                    add.l   a1,a1
                    dbf     d1,.go_to_hunk_to_reloc
                    move.l  a1,d2
                    addq.l  #4,d2
.reloc_hunk_loop:
                    ; reloc offset
                    move.l  (a0)+,d1
                    ; from the start of the hunk
                    add.l   d2,4(a4,d1.l)
                    subq.l  #1,d0
                    bne     .reloc_hunk_loop
                    bra     .reloc_hunks
.no_relocs:
                    lea     STRUCT_LEN(a2),a2
                    ; next hunk
                    dbf     d6,.depack_hunks
                    lea     (a5),a1
                    jsr     _LVOCloseLibrary(a6)
                    move.l  (a7)+,a1
                    move.l  (a7)+,d0
                    jsr     _LVOFreeMem(a6)
                    cmp.w	#37,LIB_VERSION(a6)
                    blt     .clear_cache
                    jsr     _LVOCacheClearU(a6)
.clear_cache:
                    move.l  depacker_start-4(pc),a4
                    add.l   a4,a4
                    add.l   a4,a4
                    ; first depacked section address
                    move.l  (a4),a4
                    add.l   a4,a4
                    add.l   a4,a4
                    addq.l  #4,a4
                    movem.l (a7)+,d0-a3/a5/a6
                    ; run it
                    jmp     (a4)
                    ; read data from the disk
.read_data:
                    movem.l a0/a1,-(a7)
                    move.l  file_handle(pc),d1
                    move.l  a0,d2
                    jsr     -42(a5)
                    movem.l (a7)+,a0/a1
                    cmp.l   d3,d0
                    rts

                    include "../explode_68000.s"

; ============================================================
                    cnop    0,4
depacker_end:
