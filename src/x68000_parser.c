// =================================================================
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// =================================================================
#include "x68000_parser.h"
#include "endian.h"
#include "x68000_depacker.h"

// =================================================================
extern int packed_dest_size;

// =================================================================
unsigned int implode(unsigned char *input, unsigned int size, char mode);

// =================================================================
static uint32_t get_u32_inc(const void *t, int *index)
{
    uint32_t *mem = (uint32_t *)(((uint8_t *) t) + *index);
    *index += 4;
    return swap_uint32(*mem);
}

// =================================================================
static uint16_t get_u16_inc(const void *t, int *index)
{
    uint16_t *mem = (uint16_t *)(((uint8_t *) t) + *index);
    *index += 2;
    return swap_uint16(*mem);
}

// =================================================================
static uint8_t get_u8_inc(const void *t, int *index)
{
    uint8_t *mem = (uint8_t *)(((uint8_t *) t) + *index);
    *index += 1;
    return *mem;
}

// =================================================================
static void skip(int *index, int skip_bytes)
{
    *index += skip_bytes;
}

// =================================================================
static void *loadToMemory(const char *filename, int *size)
{
    FILE *f = fopen(filename, "rb");
    void *data = 0;
    int s = 0, t = 0;

    *size = 0;

    if(!f)
    {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long ts = ftell(f);

    if(ts < 0)
    {
        goto end;
    }
    s = (int) ts;
    data = malloc(s);
    if (!data)
    {
        goto end;
    }
    fseek(f, 0, SEEK_SET);

    t = (int) fread(data, s, 1, f);
    (void) t;

    *size = s;
end:
    fclose(f);
    return data;
}

// =================================================================
static void *alloc_zero(int size)
{
	void *t = malloc(size);
	memset(t, 0, size); 
	return t;
}

// =================================================================
#define xalloc_zero(type, count) (type *) alloc_zero(sizeof(type) * count);
#define xalloc(type, count) (type *) malloc(sizeof(type) * count);

// =================================================================
X68000Info *x68000_parse_file(const char *filename)
{
    int size = 0;
    int index = 0;
    uint16_t header = 0;
    void *data = loadToMemory(filename, &size);

    if(!data)
    {
        printf("\n\nUnable to open %s.", filename);
        return 0;
    }

    X68000Info *info = xalloc_zero(X68000Info, 1);

    info->fileData = data;
    info->source_size = size;

    header = get_u16_inc(data, &index);
    if(header != X68000_MAGIC)
    {
        fprintf(stderr, "\n\nError: Header magic is incorrect (should be 0x%04x but is 0x%04x).", 0x4555, header);
        x68000_free(info);
        return 0;
    }

    // skip reserved byte
    skip(&index, 1);
    
    // malloc2 memory allocation type
    info->malloc_hi = 0;
    if(get_u8_inc(data, &index) == 2)
    {
        info->malloc_hi = 2;
    }

    info->baseAddress = get_u32_inc(data, &index);
    info->entryPoint = get_u32_inc(data, &index) - info->baseAddress;

    info->section_code.memSize = get_u32_inc(data, &index);
    info->section_data.memSize = get_u32_inc(data, &index);
    info->section_bss.memSize = get_u32_inc(data, &index);
    info->relocRealSize = get_u32_inc(data, &index);

    // we aren't interested in those
    skip(&index, 36);

    info->section_code.dataStart = index;

    return info;
}

// =================================================================
static int write_byte(unsigned char data, FILE *output_file)
{
    if(fwrite(&data, 1, sizeof(data), output_file) != sizeof(data))
    {
        fprintf(stderr, "\n\nError: can't write to file.");
        fclose(output_file);
        return 0;
    }
    packed_dest_size += sizeof(data);
    return 1;
}

// =================================================================
static int write_zero_bytes(unsigned int amount, FILE *output_file)
{
    int i;
    unsigned char data = 0;
    
    for(i = 0; i < amount; i++)
    {
        if(fwrite(&data, 1, sizeof(data), output_file) != sizeof(data))
        {
            fprintf(stderr, "\n\nError: can't write to file.");
            fclose(output_file);
            return 0;
        }
        packed_dest_size += sizeof(data);
    }
    return 1;
}

// =================================================================
static int write_word(unsigned short data, FILE *output_file)
{
    unsigned short fixed_data;

    fixed_data = swap_uint16(data);

    if(fwrite(&fixed_data, 1, sizeof(fixed_data), output_file) != sizeof(fixed_data))
    {
        fprintf(stderr, "\n\nError: can't write to file.");
        fclose(output_file);
        return 0;
    }
    packed_dest_size += sizeof(fixed_data);
    return 1;
}

// =================================================================
static int write_longword(unsigned int data, FILE *output_file)
{
    unsigned int fixed_data;

    fixed_data = swap_uint32(data);

    if(fwrite(&fixed_data, 1, sizeof(fixed_data), output_file) != sizeof(fixed_data))
    {
        fprintf(stderr, "\n\nError: can't write to file.");
        fclose(output_file);
        return 0;
    }
    packed_dest_size += sizeof(fixed_data);
    return 1;
}

// =================================================================
int x68000_pack(X68000Info *info, char *dest_filename, int mode)
{
    int packed_size;
    FILE *output_file;

    packed_dest_size = 0;
	printf("\n\nX68000 executable.\n");

	printf("\n CODE size: %8d\n", info->section_code.memSize);
	printf(" DATA size: %8d\n", info->section_data.memSize);
	printf("  BSS size: %8d\n", info->section_bss.memSize);
	printf("RELOC size: %8d\n", info->relocRealSize);

    info->section_mem = malloc(info->section_code.memSize + info->section_data.memSize + info->relocRealSize);
    if(!info->section_mem)
    {
        fprintf(stderr, "\n\nError: not enough memory.");
        return 0;
    }

    printf("\nImploding exe file (using mode %d)...", mode);
#ifdef __AMIGA__
    fflush(stdout);
#endif

    // Copy the data
    memcpy(info->section_mem, info->fileData + info->section_code.dataStart, info->section_code.memSize + info->section_data.memSize + info->relocRealSize);

    info->section_code.unpackedSize = info->section_code.memSize + info->section_data.memSize + info->relocRealSize;

    packed_size = implode(info->section_mem, info->section_code.unpackedSize, mode);
    if(!packed_size)
    {
        fprintf(stderr, "\n\nError: can't implode.");
        return 0;
    }
    info->section_code.packedSize = packed_size;

	printf("\n\n CODE size: %8d\n", info->section_code.packedSize);

    // Save the packed file
    printf("\nWriting '%s'...", dest_filename);
#ifdef __AMIGA__
    fflush(stdout);
#endif

    output_file = fopen(dest_filename, "wb");
    if(output_file)
    {
        if(!write_word(X68000_MAGIC, output_file)) return 0;
        if(!write_byte(0, output_file)) return 0;

        // Memory allocation mode
        if(!write_byte(0, output_file)) return 0;
        // Base Address
        if(!write_longword(0, output_file)) return 0;
        // Execution start position
        if(!write_longword(0, output_file)) return 0;
        // Text size (+(5 * 4) + 2 for extra infos bytes)
        if(!write_longword(size_x68000_depacker + (5 * 4) + 2, output_file)) return 0;
        // Data size
        if(!write_longword(0, output_file)) return 0;
        // Bss+comm+stack size
        if(!write_longword(0, output_file)) return 0;
        // Relocations table size
        if(!write_longword(0, output_file)) return 0;
        // Skip those
        if(!write_zero_bytes(36, output_file)) return 0;
        
        // Write the depacker
        if(fwrite(x68000_depacker, 1, size_x68000_depacker, output_file) != size_x68000_depacker)
        {
            fprintf(stderr, "\n\nError: can't write to file.");
            fclose(output_file);
            return 0;
        }
        packed_dest_size += size_x68000_depacker;

        // Bytes to read
        if(!write_longword(info->section_code.packedSize, output_file)) return 0;
        // Memory to alloc
        if(!write_longword(info->section_code.unpackedSize + info->section_bss.memSize, output_file)) return 0;
        // Type of memory to alloc
        if(!write_word(info->malloc_hi, output_file)) return 0;
        // Relocations position
        if(!write_longword(info->section_code.memSize + info->section_data.memSize, output_file)) return 0;
        // Relocations size
        if(!write_longword(info->relocRealSize, output_file)) return 0;
        // Entry point
        if(!write_longword(info->entryPoint, output_file)) return 0;

        // Write the data
        if(fwrite(info->section_mem, 1, info->section_code.packedSize, output_file) != info->section_code.packedSize)
        {
            fprintf(stderr, "\n\nError: can't write to file.");
            fclose(output_file);
            return 0;
        }
        packed_dest_size += info->section_code.packedSize;
    }
    else
    {
        fprintf(stderr, "\n\nError: can't open '%s' for writing.", dest_filename);
        return 0;
    }

    if(packed_dest_size)
    {
        printf("\n\nOriginal size: %8d bytes.\n", info->source_size);
        printf("Imploded size: %8d bytes.\n", packed_dest_size);
        printf("          Won: %8d bytes.", info->source_size - packed_dest_size);
    }
}

// =================================================================
void x68000_free(X68000Info *info)
{
	if(info->section_mem) free(info->section_mem);
	info->section_mem = NULL;
    if(info->fileData) free(info->fileData);
	info->fileData = NULL;
	if(info) free(info);
    info = NULL;
}
