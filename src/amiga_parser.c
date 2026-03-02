// =================================================================
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// =================================================================
#include "doshunks.h"
#include "amiga_parser.h"
#include "endian.h"
#include "amiga_depacker.h"

// =================================================================
#define HUNK_DEBUG_LINE 0x4C494E45

#define MEMF_ANY 0
#define MEMF_PUBLIC (1<<0)
#define MEMF_FAST (1<<2)
#define MEMF_CHIP (1<<1)
#define MEMF_CLEAR (1<<16)

// =================================================================
const char* hunktype[HUNK_ABSRELOC16 - HUNK_UNIT + 1] = 
{
    "UNIT", "NAME", "CODE", "DATA", "BSS ", "RELOC32", "RELOC16", "RELOC8",
    "EXT", "SYMBOL", "DEBUG", "END", "HEADER", "", "OVERLAY", "BREAK",
    "DREL32", "DREL16", "DREL8", "LIB", "INDEX",
    "RELOC32SHORT", "RELRELOC32", "ABSRELOC16"
};

extern int packed_dest_size;

// =================================================================
unsigned int implode(unsigned char *input, unsigned int size, char mode);

// =================================================================
static uint32_t get_u32(const void *t, int index)
{
    uint32_t *mem = (uint32_t *)(((uint8_t *) t) + index);
    return swap_uint32(*mem);
}

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
    if(t)
    {
        memset(t, 0, size); 
    }
	return t;
}

// =================================================================
#define xalloc_zero(type, count) (type *) alloc_zero(sizeof(type) * count);
#define xalloc(type, count) (type *) malloc(sizeof(type) * count);

// =================================================================
static void parseSymbols(AHPSection *section, const void *data, int *currIndex)
{
	int oldIndex, index, i = 0;
	int symCount = 0;

	index = oldIndex = *currIndex;

	// count symbols
	int symlen = get_u32_inc(data, &index) * 4;

	while(symlen > 0)
	{
		symCount++;
		index += symlen + 4;
		symlen = get_u32_inc(data, &index) * 4;
	}

	section->symbolCount = symCount;
	section->symbols = xalloc(AHPSymbolInfo, symCount);

	index = oldIndex;

	symlen = get_u32_inc(data, &index) * 4;

	while(symlen > 0)
	{
		AHPSymbolInfo *info = &section->symbols[i++];
		info->name = ((const char*) data) + index;
		index += symlen;
		info->address = get_u32_inc(data, &index) * 4;
		symlen = get_u32_inc(data, &index) * 4;
	}

	*currIndex = index;
}

// =================================================================
static void parseDebug(AHPSection *section, const void *data, int *currIndex)
{
	int index = *currIndex;
	AHPLineInfo *lineInfo = 0;

	const uint32_t hunkLength = get_u32_inc(data, &index) * 4;
	const uint32_t baseOffset = get_u32_inc(data, &index) * 4;
	const uint32_t debugId = get_u32_inc(data, &index);

	if(debugId != HUNK_DEBUG_LINE)
	{
		*currIndex += hunkLength;
		return;
	}

	if(!section->debugLines)
	{
		lineInfo = xalloc_zero(AHPLineInfo, 1);
		section->debugLines = lineInfo;
		section->debugLineCount = 1;
	}
	else
	{
		// Kinda sucky impl but should do
		int lineCount = section->debugLineCount++;
		section->debugLines = realloc(section->debugLines, lineCount * 2 * sizeof(AHPLineInfo));
		lineInfo = &section->debugLines[lineCount];
		memset(lineInfo, 0, sizeof(AHPLineInfo));
	}

	const uint32_t stringLength = get_u32_inc(data, &index) * 4;

	lineInfo->baseOffset = baseOffset;
	lineInfo->filename = ((const char *)data) + index;

	index += stringLength;

	// M = ((N - 3) - number_of_string_longwords) / 2

	const int lineCount = ((hunkLength - (3 * 4)) - stringLength) / 8;

	lineInfo->addresses = xalloc(uint32_t, lineCount); 
	lineInfo->lines = xalloc(int, lineCount); 

	for(int i = 0; i < lineCount; ++i)
	{
		lineInfo->lines[i] = (int) get_u32_inc(data, &index); 
		lineInfo->addresses[i] = (uint32_t) get_u32_inc(data, &index); 
	}

	lineInfo->count = lineCount;
	
	*currIndex += hunkLength + 4;
}

// =================================================================
static void parseCodeDataBss(AHPSection *section, int type, const void *data, int *currIndex)
{
	int index = *currIndex;

	switch (type)
	{
		case HUNK_CODE: section->type = AHPSectionType_Code; break;
		case HUNK_DATA: section->type = AHPSectionType_Data; break;
		case HUNK_BSS: section->type = AHPSectionType_Bss; break;
	}

	section->dataSize = get_u32_inc(data, &index) * 4;

	if (type != HUNK_BSS)
	{
		section->dataStart = index;
		index += section->dataSize;

		if (section->dataSize > 0)
		{
			int sum = 0;

			for (unsigned int pos = section->dataStart; pos < section->dataStart + section->dataSize; pos += 4)
				sum += get_u32(data, pos);
			(void) sum;
		}
	}

	*currIndex = index;
}

// =================================================================
static void parseReloc32(AHPSection *section, const void *data, int *currIndex)
{
	int index = *currIndex;

	section->relocStart = index;
	int n, tot = 0;

	while((n = get_u32_inc(data, &index)) != 0)
	{
		uint32_t t = get_u32_inc(data, &index);
		(void) t;

		tot += n;
		while(n--)
		{
			if((get_u32_inc(data, &index)) > (unsigned int) section->memSize - 4)
			{
				fprintf(stderr, "\n\nError: Error in reloc table.");
				exit(EXIT_FAILURE);
			}
		}
	}

    section->relocCount = tot;
	section->relocRealSize = index - section->relocStart;
	*currIndex = index;
}

// =================================================================
static int parseSection(AHPSection *section, const void *data, int hunkId, int size, int *currIndex)
{
	int type;
	int index = *currIndex;

	for(;;)
	{
		if(index >= size)
		{
			fprintf(stderr, "\n\nError: Unexpected end of file.");
			return 0;
		}

		type = get_u32_inc(data, &index) & 0x0fffffff;

		if(index >= size && type != HUNK_END)
		{
			fprintf(stderr, "\n\nError: Unexpected end of file.");
			return 0;
		}

		switch(type)
		{
			case HUNK_DEBUG: parseDebug(section, data, &index); break;
			case HUNK_SYMBOL: parseSymbols(section, data, &index); break;

			case HUNK_CODE:
			case HUNK_DATA:
			case HUNK_BSS: parseCodeDataBss(section, type, data, &index); break;
			case HUNK_RELOC32: parseReloc32(section, data, &index); break;

			case HUNK_DREL32:
			case HUNK_RELOC32SHORT:
			case HUNK_UNIT:
			case HUNK_NAME:
			case HUNK_RELOC16:
			case HUNK_RELOC8:
			case HUNK_EXT:
			case HUNK_HEADER:
			case HUNK_OVERLAY:
			case HUNK_BREAK:
			case HUNK_DREL16:
			case HUNK_DREL8:
			case HUNK_LIB:
			case HUNK_INDEX:
			case HUNK_RELRELOC32:
			case HUNK_ABSRELOC16:
			{
				fprintf(stderr, "\n\nError: %s (unsupported) at %d.", hunktype[type - HUNK_UNIT], index);
				return 0;
			}

			case HUNK_END: 
			{
				*currIndex = index;
				return 1; 
			}

			default:
			{
				fprintf(stderr, "\n\nError: Unknown (%08X).", type);
				return 0;
			}
		}
	}

	return 1;
}

// =================================================================
AHPInfo *amiga_parse_file(const char *filename)
{
    int size = 0;
    void *data = loadToMemory(filename, &size);
    uint32_t header = 0;
    int h, index = 0;
    int sectionCount = 0;
    AHPSection* sections = 0;

    if(!data)
    {
        fprintf(stderr, "\nError: Unable to open %s.", filename);
        return 0;
    }

    AHPInfo *info = xalloc_zero(AHPInfo, 1);

    if(!info)
    {
        fprintf(stderr, "\n\nError: Not enough memory.");
        amiga_free(info);
        return 0;
    }

    info->fileData = data;
    info->source_size = size;

    header = get_u32_inc(data, &index);
    if(header != HUNK_HEADER)
    {
        fprintf(stderr, "\n\nError: HunkHeader is incorrect (should be 0x%08x but is 0x%08x).", HUNK_HEADER, header);
        amiga_free(info);
        return 0;
    }

    while(get_u32_inc(data, &index))
    {
        index += get_u32(data, index) * 4;
        if(index >= size)
        {
            fprintf(stderr, "\n\nError: Bad hunk header.");
        	amiga_free(info);
            return 0;
        }
    }

    sectionCount = get_u32_inc(data, &index);

    if(sectionCount == 0)
    {
        fprintf(stderr, "\n\nError: No sections.");
		amiga_free(info);
        return 0;
    }

	info->sections = sections = xalloc_zero(AHPSection, sectionCount); 
	info->sectionCount = sectionCount;

    if(get_u32_inc(data, &index) != 0 || get_u32_inc(data, &index) != sectionCount - 1)
    {
        fprintf(stderr, "\n\nError: Unsupported hunk load limits.");
        amiga_free(info);
        return 0;
    }

    // read hunk sizes and target
    for(h = 0; h < sectionCount; ++h)
    {
    	AHPSectionTarget target = AHPSectionTarget_Any;

        sections[h].memSize = (get_u32(data, index) & 0x0fffffff) * 4;
        uint32_t flags = get_u32(data, index) & 0xf0000000;

        switch(flags)
		{
			case 0: target = AHPSectionTarget_Any; break; 
			case HUNKF_CHIP: target = AHPSectionTarget_Chip; break; 
			case HUNKF_FAST: target = AHPSectionTarget_Fast; break; 
		}

		sections[h].target = target;
        index += 4;
    }

    for(h = 0; h < sectionCount; ++h)
    {
    	if(!parseSection(&sections[h], data, h, size, &index)) 
		{
			amiga_free(info);
    		return 0; 
		}
    }

    if(index < size)
    {
        fprintf(stderr, "\n\nWarning: %d bytes of extra data at the end of the file.\n", (int) (size - index) * 4);
    }

    return info;
}

// =================================================================
static const char *getTypeName(AHPSectionType type)
{
	switch(type)
	{
		case AHPSectionType_Code: return "CODE";
		case AHPSectionType_Data: return "DATA";
		case AHPSectionType_Bss: return  " BSS";
	}
	return "UNKN";
}

// =================================================================
static const char *getTargetName(AHPSectionTarget target)
{
	switch(target)
	{
		case AHPSectionTarget_Any: return  " ANY";
		case AHPSectionTarget_Fast: return "FAST";
		case AHPSectionTarget_Chip: return "CHIP";
	}
	return "UNKN";
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
int amiga_pack(AHPInfo *info, char *dest_filename, int mode)
{	
	int i;
    int packed_size;
    FILE *output_file;
    unsigned int bss_phony_relocs = -1;
    unsigned int allocmem_flags = 0;

    packed_dest_size = 0;
	printf("\n\nAmiga executable.\n");

	printf("\nSec Type Target     Size   Relocs\n");

    info->sections_mem = malloc(info->sectionCount * sizeof(unsigned char *));

	for(i = 0; i < info->sectionCount; ++i)
	{
		AHPSection *section = &info->sections[i];

		printf(" %02d %s   %s %8d %8d\n", i, 
				getTypeName(section->type),
				getTargetName(section->target),
				section->memSize,
                section->relocRealSize);
        info->sections_mem[i] = malloc(section->memSize + section->relocRealSize);
        if(!info->sections_mem[i])
        {
            fprintf(stderr, "\n\nError: not enough memory.");
            return 0;
        }
        memset(info->sections_mem[i], 0, section->memSize + section->relocRealSize);
	}

    printf("\nImploding exe file (using mode %d)...", mode);
#ifdef __AMIGA__
    fflush(stdout);
#endif

    // Pack them
	for (i = 0; i < info->sectionCount; ++i)
	{
		AHPSection *section = &info->sections[i];

        switch(section->type)
        {
            case AHPSectionType_Code:
            case AHPSectionType_Data:
                // Add the data
                memcpy(info->sections_mem[i], info->fileData + section->dataStart, section->memSize);
                // Add the relocs
                memcpy(info->sections_mem[i] + section->memSize, info->fileData + section->relocStart, section->relocRealSize);

                section->unpackedSize = section->relocRealSize + section->memSize;

                packed_size = implode(info->sections_mem[i], section->unpackedSize, mode);
                if(!packed_size)
                {
                    fprintf(stderr, "\n\nError: can't implode.");
                    return 0;
                }
                section->packedSize = ((packed_size + 3) >> 2) << 2;
                break;
            case AHPSectionType_Bss:
                break;
        }
    }

    // Print results
	printf("\n\nSec Type Target     Size\n");

	for (i = 0; i < info->sectionCount; ++i)
	{
		AHPSection* section = &info->sections[i];
		printf(" %02d %s   %s %8d\n", i,
				getTypeName(section->type),
				getTargetName(section->target),
				section->packedSize
              );
	}

    // Save the packed file
    printf("\nWriting '%s'...", dest_filename);
#ifdef __AMIGA__
    fflush(stdout);
#endif

    output_file = fopen(dest_filename, "wb");
    if(output_file)
    {
        // Write the file header
        if(!write_longword(HUNK_HEADER, output_file)) return 0;
        // private_dummy
        if(!write_longword(0, output_file)) return 0;
        // TABLE_SIZE
        if(!write_longword(2, output_file)) return 0;
        // FIRST_HUNK
        if(!write_longword(0, output_file)) return 0;
        // LAST_HUNK
        if(!write_longword(1, output_file)) return 0;
        // OVERLAY SIZE
        if(!write_longword(size_amiga_depacker >> 2, output_file)) return 0;
        // Phony size
        if(!write_longword(0, output_file)) return 0;
        if(!write_longword(HUNK_CODE, output_file)) return 0;
        if(!write_longword(size_amiga_depacker >> 2, output_file)) return 0;

        // Write the depacker
        if(fwrite(amiga_depacker, 1, size_amiga_depacker, output_file) != size_amiga_depacker)
        {
            fprintf(stderr, "\n\nError: can't write to file.");
            fclose(output_file);
            return 0;
        }
        packed_dest_size += size_amiga_depacker;

        // Close the hunk
        if(!write_longword(HUNK_END, output_file)) return 0;
        if(!write_longword(HUNK_OVERLAY, output_file)) return 0;
        if(!write_longword(0, output_file)) return 0;
        if(!write_longword(HUNK_BREAK, output_file)) return 0;

        // Number of sections to depack
        if(!write_longword(info->sectionCount - 1, output_file)) return 0;

        // Write the packed sections infos
        for (i = 0; i < info->sectionCount; ++i)
        {
            AHPSection *section = &info->sections[i];
            switch(section->target)
            {
                case AHPSectionTarget_Fast:
                    allocmem_flags = MEMF_FAST | MEMF_CLEAR;
                    break;
                case AHPSectionTarget_Chip:
                    allocmem_flags = MEMF_CHIP | MEMF_CLEAR;
                    break;
                default:
                case AHPSectionTarget_Any:
                    allocmem_flags = MEMF_PUBLIC | MEMF_CLEAR;
                    break;
            }
            if(section->type != AHPSectionType_Bss)
            {
                // reloc pos
                if(!write_longword(section->memSize, output_file)) return 0;
                // depacked size for allocation
                // +8 for DOS memlist
                // +4 for empty relocations
                if(!write_longword(section->memSize + section->relocRealSize + 4 + 8, output_file)) return 0;
                // memory type
                if(!write_longword(allocmem_flags, output_file)) return 0;
                // packed size for reading
                if(!write_longword(section->packedSize, output_file)) return 0;
            }
            else
            {
                // a bss section have no relocs
                if(!write_longword(-1, output_file)) return 0;
                // memory to alloc
                if(!write_longword(section->memSize + 4 + 8, output_file)) return 0;
                // memory type
                if(!write_longword(allocmem_flags, output_file)) return 0;
                // no reading
                if(!write_longword(-1, output_file)) return 0;
            }
        }

        // Write the packed sections data
        for (i = 0; i < info->sectionCount; ++i)
        {
            AHPSection *section = &info->sections[i];
            if(section->type != AHPSectionType_Bss)
            {
                if(fwrite(info->sections_mem[i], 1, section->packedSize, output_file) != section->packedSize)
                {
                    fprintf(stderr, "\n\nError: can't write to file.");
                    fclose(output_file);
                    return 0;
                }
                packed_dest_size += section->packedSize;
            }
        }
        fclose(output_file);
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
    return 1;
}

// =================================================================
void amiga_free(AHPInfo *info)
{
    int i;

	for (i = 0; i < info->sectionCount; ++i)
	{
        if(info->sections_mem[i]) free(info->sections_mem[i]);
        info->sections_mem[i] = NULL;
	}
    if(info->sections_mem) free(info->sections_mem);
    info->sections_mem = NULL;
	if(info->sections) free(info->sections);
	info->sections = NULL;
    if(info->fileData) free(info->fileData);
	info->fileData = NULL;
	if(info) free(info);
    info = NULL;
}
