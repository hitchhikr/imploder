// =================================================================
#ifndef _X68000_PARSER_
#define _X68000_PARSER_

#include <stdint.h>

#define TRUE 1
#define FALSE 0

#define X68000_MAGIC 0x4855

// =================================================================
typedef enum X68000SectionType
{
	X68000SectionType_Code,
	X68000SectionType_Data,
	X68000SectionType_Bss,
} X68000SectionType;

// =================================================================
typedef struct X68000Section
{
    X68000SectionType type;
    int memSize;
    int packedSize;
    int dataSize;
    int unpackedSize;
    uint32_t dataStart;
    uint32_t relocStart;
    int relocCount;
} X68000Section;

// =================================================================
typedef struct X68000Info
{
	X68000Section section_code;
	X68000Section section_data;
	X68000Section section_bss;
    unsigned char *section_mem;
    int relocRealSize;
    int source_size;
    short malloc_hi;
    unsigned int baseAddress;
    unsigned int entryPoint;
	unsigned char *fileData;
} X68000Info;

// =================================================================
X68000Info *x68000_parse_file(const char *filename);
int x68000_pack(X68000Info *info, char *dest_filename, int mode);
void x68000_free(X68000Info *info);

#endif
