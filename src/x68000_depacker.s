; ============================================================
; Impoder v1.4b overlay X68000 depacker
; Written by hitchhikr.
; ============================================================
                    opt     o+
                    opt     all+

; ============================================================
_EXIT               equ     $FF00
_PRINT              equ     $FF09
_OPEN               equ     $FF3D
_CLOSE              equ     $FF3E
_READ               equ     $FF3F
_SEEK               equ     $FF42
_SETBLOCK           equ     $FF4A
_GETPDB             equ     $FF81
_MALLOC2            equ     $FF88

DOS                 macro
                    dc.w	\1
                    endm

                    rsreset
PACKED_SIZE         rs.l    1
DEPACKED_SIZE       rs.l    1
MEM_TYPE            rs.w    1
RELOCATIONS         rs.l    1
RELOCATIONS_SIZE    rs.l    1
ENTRY_POINT         rs.l    1
STRUCT_LEN          rs.b    0

; ============================================================
begin:
                    movem.l d0-d6/a0-a3/a5/a6,-(a7)
                    lea     (16,a0),a0
                    sub.l   a0,a1
                    move.l  a1,-(sp)
                    move.l  a0,-(sp)
                    DOS     _SETBLOCK
                    addq.l  #8,a7
                    DOS     _GETPDB
                    move.l  d0,a0
                    ; reconstruct the complete filename
                    lea     $72(a0),a1
.end_of_path:
                    tst.b   (a1)+
                    bne     .end_of_path
                    subq.l  #1,a1
                    lea     $B4(a0),a2
.copy_filename:
                    move.b  (a2),(a1)+
                    tst.b   (a2)+
                    bne    .copy_filename
                    sf.b    (a2)
                    lea     $70(a0),a1
                    ; and re-open ourself
                    move.w  #1,-(a7)
                    move.l  a1,-(a7)
                    DOS     _OPEN
                    addq.l  #6,a7
                    move.l  d0,d7
                    ; move to the packed data
                    clr.w   -(a7)
                    move.l  #(depacker_end+(5*4)+2)+64-begin,-(a7)
                    move.w  d7,-(a7)
                    DOS     _SEEK
                    addq.l  #8,a7
                    ; allocate room
                    lea     depacker_end(pc),a2
                    move.l  DEPACKED_SIZE(a2),-(a7)
                    move.w  MEM_TYPE(a2),-(a7)
                    DOS     _MALLOC2
                    addq.l  #6,a7
                    lea     .not_enough_memory(pc),a0
                    tst.l   d0
                    bmi     .error
                    move.l  d0,a4
                    ; load the packed data
                    move.l  PACKED_SIZE(a2),-(a7)
                    move.l  a4,-(a7)
                    move.w  d7,-(a7)
                    DOS     _READ
                    lea     10(a7),a7
                    lea     .reading_error(pc),a0
                    cmp.l   PACKED_SIZE(a2),d0
                    bne     .error
                    ; we're done
                    move.w  d7,-(a7)
                    DOS     _CLOSE
                    addq.l  #2,a7
                    ; depack them
                    lea     (a4),a0
                    lea     (a4),a1
                    bsr     _explode
                    ; reloc the data
                    move.l  RELOCATIONS(a2),d0      ; relocs offset
                    lea     (a4,d0.l),a0            ; reloc section address
                    move.l  a4,d3                   ; base address
                    move.l  RELOCATIONS_SIZE(a2),d0 ; size of relocs
                    lea     (a0,d0.l),a3            ; end of relocs
.dorelocs:
                    moveq   #0,d2
                    move.w  (a0)+,d2
                    cmp.w   #1,d2
                    bne     .reloc_longjump
                    move.l  (a0)+,d2
.reloc_longjump:
                    add.l   d2,a1
                    add.l   d3,(a1)
                    cmp.l   a3,a0
                    bne     .dorelocs
                    move.l  ENTRY_POINT(a2),d7
                    movem.l (a7)+,d0-d6/a0-a3/a5/a6
                    jmp     (a4,d7.l)
.error:
                    move.l  a0,-(a7)
                    DOS     _PRINT
                    DOS     _EXIT
.not_enough_memory:
                    dc.b    "Not enough memory.",13,10,0
.reading_error:
                    dc.b    "Reading error.",13,10,0
                    even

                    include "../explode_68000.s"

; ============================================================
depacker_end:
