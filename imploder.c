// =================================================================
// Imploder data cruncher/decruncher
// =================================================================

// =================================================================
#define LITTLE_ENDIAN
#define OP_IMPLODE 0
#define OP_EXPLODE 1

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <strings.h>

// =================================================================
static unsigned int MAX_SIZES[12] =
{
    0x80, 0x100, 0x200, 0x400, 0x700, 0xD00, 0x1500, 0x2500, 0x5100, 0x9200, 0x10900, 0x10900
};

static unsigned char MODE_INIT[12][12] =
{
    { 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06 }, // mode 0
	{ 0x05, 0x06, 0x07, 0x07, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x06, 0x06 }, // mode 1
	{ 0x05, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x08 }, // mode 2
	{ 0x05, 0x06, 0x07, 0x08, 0x07, 0x07, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09 }, // mode 3
    { 0x06, 0x07, 0x07, 0x08, 0x07, 0x08, 0x09, 0x09, 0x08, 0x09, 0x0A, 0x0A }, // mode 4
	{ 0x06, 0x07, 0x07, 0x08, 0x07, 0x09, 0x09, 0x0A, 0x08, 0x0A, 0x0B, 0x0B }, // mode 5
	{ 0x06, 0x07, 0x08, 0x08, 0x07, 0x09, 0x09, 0x0A, 0x08, 0x0A, 0x0B, 0x0C }, // mode 6
	{ 0x06, 0x07, 0x08, 0x08, 0x07, 0x09, 0x09, 0x0A, 0x09, 0x0A, 0x0C, 0x0D }, // mode 7
    { 0x06, 0x07, 0x07, 0x08, 0x07, 0x09, 0x09, 0x0C, 0x09, 0x0A, 0x0C, 0x0E }, // mode 8
	{ 0x06, 0x07, 0x08, 0x09, 0x07, 0x09, 0x0A, 0x0C, 0x09, 0x0B, 0x0D, 0x0F }, // mode 9
	{ 0x06, 0x07, 0x08, 0x08, 0x07, 0x0A, 0x0B, 0x0B, 0x09, 0x0C, 0x0D, 0x10 }, // mode A
	{ 0x06, 0x08, 0x08, 0x09, 0x07, 0x0B, 0x0C, 0x0C, 0x09, 0x0D, 0x0E, 0x11 }, // mode B
};

static unsigned short static_token_lens[12] =
{
    2, 2, 2, 2, 6, 0x0A, 0x0A, 0x12, 0x16, 0x2A, 0x8A, 0x4012
};
static unsigned char static_reps[4] =
{
    0, 2, 6, 14
};
static unsigned char static_rep_bits_cnts[4] =
{
    1, 2, 3, 4
};
static unsigned char static_token_extra_bits[12] =
{
    1,  1,  1,  1,  2,  3,  3,  4,  4,  5,  7, 14
};
static unsigned char explode_token_base[4] =
{
    6, 10, 10, 18
};

unsigned char *src;

unsigned int read_pos, src_size, max_encoded_size, end_offset, token_run_len;
unsigned char write_byte;

unsigned short last_reps, encoded_reps, last_token_len, encoded_token_len;
unsigned char last_reps_bits_cnt, encoded_reps_bits_cnt, last_token_len_bits_cnt;
unsigned char encoded_token_len_bits_cnt, last_offset_bits_cnt, encoded_offset_bits_cnt;
unsigned int last_offset, encoded_offset;

char write_bits_cnt;

unsigned short counts[8];
unsigned int offsets[8];

unsigned int run_base_offs[12];
unsigned char run_extra_bits[12];

unsigned char *src, *cmpr_data;
unsigned int write_pos, src_end;
int cmpr_pos;

unsigned char token;

// =================================================================
static unsigned short word_get_bits_right(unsigned short value, unsigned char count)
{
	return (unsigned short) (((1 << count) - 1) & value);
}

static unsigned int long_get_bits_right(unsigned int value, unsigned char count)
{
	return (unsigned int) (((1 << count) - 1) & value);
}

static void copy_bytes(unsigned char *dst, unsigned char *src, int count)
{
	for (int i = 0; i < count; i++)
    {
		dst[i] = src[i];
	}
}

static unsigned short read_word(unsigned char *buf)
{
	return ((buf[0] << 8) | buf[1]);
}

static unsigned int read_dword(unsigned char *buf)
{
	return ((read_word(&buf[0]) << 16) | read_word(&buf[2]));
}

static void write_word(unsigned char *dst, unsigned short value)
{
	dst[0] = (value >> 8) & 0xFF;
	dst[1] = (value >> 0) & 0xFF;
}

static void write_dword(unsigned char *dst, unsigned int value)
{
	write_word(&dst[0], (value >> 16) & 0xFFFF);
	write_word(&dst[2], (value >> 0) & 0xFFFF);
}

unsigned int read_bits(unsigned char count)
{
	unsigned int retn = 0;

	for (int i = 0; i < count; i++)
    {
		char bit = (token >> 7);
		token <<= 1;

		if (!token)
        {
			token = (cmpr_data[cmpr_pos - 1] << 1) | bit;
			bit = (cmpr_data[--cmpr_pos] >> 7);
		}

		retn <<= 1;
		retn |= bit;
	}
	return retn;
}

int check_imp(unsigned char *input)
{
	unsigned int id, end_off, out_len;

	if (!input)
    {
		return 0;
	}

	id = read_dword(&input[0]);
	out_len = read_dword(&input[4]);
	end_off = read_dword(&input[8]);

	// check for magic ID 'IMP!', or one of the other IDs used by Imploder
    // clones; ATN!, BDPI, CHFI, Dupa, EDAM, FLT!, M.H., PARA and RDC9
	if (id != 0x494d5021 && id != 0x41544e21 && id != 0x42445049 && id != 0x43484649 && id != 0x44757061 &&
        id != 0x4544414d && id != 0x464c5421 && id != 0x4d2e482e && id != 0x50415241 &&	id != 0x52444339)
    {
		return 0;
	}

	// sanity checks
	return !((end_off & 1) || (end_off < 14) || ((end_off + 0x26) > out_len));
}

int exploded_size(unsigned char *input)
{
	if(!check_imp(input)) return 0;

	return read_dword(&input[4]);
}

static unsigned int checksum(unsigned char *buf, unsigned int size)
{
	unsigned int sum = 0;

	size >>= 1;
	for (int i = 0; i < size; i++)
    {
		sum += read_word(&buf[i * 2]);
	}

	return (sum + 7);
}

void find_repeats()
{
	for (int i = 0; i < 8; i++)
    {
		counts[i] = 0;
	}

	unsigned int limit = read_pos + max_encoded_size;

	if (limit > src_size)
    {
		limit = src_size;
	}

	int min_reps = 1;
	unsigned int offset = read_pos;

	while (offset < limit)
    {
		while (src[++offset] != src[read_pos] && offset < limit);

		int reps = 1;

		while (src[read_pos + reps] == src[offset + reps] && reps < 0xFF && (offset + reps) < limit)
        {
			reps++;
		}

		if (reps > min_reps)
        {
			min_reps = reps;

			if (reps <= 8)
            {
				if (counts[reps - 2] == 0)
                {
					counts[reps - 2] = reps;
					offsets[reps - 2] = offset - read_pos - 1;
				}
			}
			else
            {
				counts[7] = reps;
				offsets[7] = offset - read_pos - 1;

				if (reps == 0xFF)
                {
					break;
				}
			}
		}
	}
}

int encode_match(unsigned char count, unsigned int offset)
{
	count -= 2;
	if (count <= 3)
    {
		last_reps = static_reps[count];
		last_reps_bits_cnt = static_rep_bits_cnts[count];
	}
	else
    {
		if (count <= 11)
        {
			last_reps = 0xF0 | (count - 4);
			last_reps_bits_cnt = 8;
		}
		else
        {
			last_reps = 0x1F00 | (count + 2);
			last_reps_bits_cnt = 13;
		}
		count = 3;
	}

	if (static_token_lens[count] > token_run_len)
    {
		last_token_len_bits_cnt = static_token_extra_bits[count] + 1;
		last_token_len = word_get_bits_right(token_run_len, static_token_extra_bits[count]);
	}
	else if (static_token_lens[count + 4] > token_run_len)
    {
		last_token_len_bits_cnt = static_token_extra_bits[count + 4] + 2;
		last_token_len = (2 << static_token_extra_bits[count + 4]) | 
                         word_get_bits_right(token_run_len - static_token_lens[count], static_token_extra_bits[count + 4]);
	}
	else if (static_token_lens[count + 8] > token_run_len)
    {
		last_token_len_bits_cnt = static_token_extra_bits[count + 8] + 2;
		last_token_len = (3 << static_token_extra_bits[count + 8]) |
                         word_get_bits_right(token_run_len - static_token_lens[count + 4], static_token_extra_bits[count + 8]);
	}
	else
    {
		return 0;
	}

	if (run_base_offs[count] > offset)
    {
		last_offset_bits_cnt = run_extra_bits[count] + 1;
		last_offset = long_get_bits_right(offset, run_extra_bits[count]);
	}
	else if (run_base_offs[count + 4] > offset)
    {
		last_offset_bits_cnt = run_extra_bits[count + 4] + 2;
		last_offset = (2 << run_extra_bits[count + 4]) |
                      long_get_bits_right(offset - run_base_offs[count], run_extra_bits[count + 4]);
	}
	else if (run_base_offs[count + 8] > offset)
    {
		last_offset_bits_cnt = run_extra_bits[count + 8] + 2;
		last_offset = (3 << run_extra_bits[count + 8]) |
                      long_get_bits_right(offset - run_base_offs[count + 4], run_extra_bits[count + 8]);
	}
	else
    {
		return 0;
	}
	return -1;
}

unsigned short find_best_match_and_encode()
{
	short max = 0;
	unsigned short retn = 0;

	for (int i = 0; i < 8; i++)
    {
		if (counts[i] && encode_match(counts[i], offsets[i]))
        {
			short encoded = (counts[i] << 3) - (last_reps_bits_cnt + last_offset_bits_cnt + last_token_len_bits_cnt);
			if (encoded >= max)
            {
				encoded_reps = last_reps;
				encoded_reps_bits_cnt = last_reps_bits_cnt;
				encoded_token_len = last_token_len;
				encoded_token_len_bits_cnt = last_token_len_bits_cnt;
				encoded_offset = last_offset;
				encoded_offset_bits_cnt = last_offset_bits_cnt;
				max = encoded;
				retn = counts[i];
			}
		}
	}
	return retn;
}

void write_bits(unsigned char count, unsigned int value)
{
	for (int i = 0; i < count; i++)
    {
		write_byte >>= 1;
		write_byte |= ((value & 1) << 7);
		value >>= 1;

		write_bits_cnt--;
		if (write_bits_cnt < 0)
        {
			write_bits_cnt = 7;
			src[end_offset++] = write_byte;
			write_byte = 0;
		}
	}
}

int explode(unsigned char *input)
{
	if (!check_imp(input))
    {
		return 0;
	}

	write_pos = 0, src_size = 0, src_end = 0, token_run_len = 0;
	cmpr_pos = 0;
	token = 0;

	src = input;
	src_end = src_size = read_dword(&src[0x04]);
	cmpr_data = &src[read_dword(&src[0x08])];
	cmpr_pos = 0;

	write_dword(&src[0x08], read_dword(&cmpr_data[0x00]));
	write_dword(&src[0x04], read_dword(&cmpr_data[0x04]));
	write_dword(&src[0x00], read_dword(&cmpr_data[0x08]));

	token_run_len = read_dword(&cmpr_data[0x0C]);

	if (!(cmpr_data[0x10] & 0x80))
    {
		cmpr_pos--;
	}

	token = cmpr_data[0x11];

	for (int i = 0; i < 8; i++)
    {
		run_base_offs[i] = read_word(&cmpr_data[0x12 + i * 2]);
	}

	copy_bytes(&run_extra_bits[0], &cmpr_data[0x22], 12);

	while (1)
    {
		for (int i = 0; (i < token_run_len) && (src_end > 0); i++)
        {
			src[--src_end] = cmpr_data[--cmpr_pos];
		}

		if (src_end == 0)
        {
			break;
		}

		unsigned int match_len, selector;

		if (read_bits(1))
        {
			if (read_bits(1))
            {
				if (read_bits(1))
                {
					if (read_bits(1))
                    {
						if (read_bits(1))
                        {
							match_len = cmpr_data[--cmpr_pos];
							selector = 3;
						}
						else
                        {
							match_len = read_bits(3) + 6;
							selector = 3;
						}
					}
					else
                    {
						match_len = 5;
						selector = 3;
					}
				}
				else
                {
					match_len = 4;
					selector = 2;
				}
			}
			else
            {
				match_len = 3;
				selector = 1;
			}
		}
		else
        {
			match_len = 2;
			selector = 0;
		}

		if (read_bits(1))
        {
			if (read_bits(1))
            {
				token_run_len = read_bits(static_token_extra_bits[selector + 8]) + explode_token_base[selector];
			}
			else
            {
				token_run_len = read_bits(static_token_extra_bits[selector + 4]) + 2;
			}
		}
		else
        {
			token_run_len = read_bits(static_token_extra_bits[selector]);
		}

		unsigned char *match;

		if (read_bits(1))
        {
			if (read_bits(1))
            {
				match = &src[src_end + read_bits(run_extra_bits[8 + selector]) + run_base_offs[4 + selector] + 1];
			}
			else
            {
				match = &src[src_end + read_bits(run_extra_bits[4 + selector]) + run_base_offs[selector] + 1];
			}
		}
		else
        {
			match = &src[src_end + read_bits(run_extra_bits[selector]) + 1];
		}

		for (int i = 0; (i < match_len) && (src_end > 0); i++)
        {
			src[--src_end] = *--match;
		}
	}

	return src_size;
}

unsigned int implode(unsigned char *input, unsigned int size, char mode)
{
	if (!input || size < 0x40)
    {
		return 0;
	}

	read_pos = src_size = max_encoded_size = end_offset = token_run_len = 0;
	write_byte = 0;
	last_reps = encoded_reps = last_token_len = encoded_token_len = 0;
	last_reps_bits_cnt = encoded_reps_bits_cnt = last_token_len_bits_cnt = 0;
	encoded_token_len_bits_cnt = last_offset_bits_cnt = encoded_offset_bits_cnt = 0;
	last_offset = encoded_offset = 0;
	write_bits_cnt = 0;

	src_size = size;
	read_pos = 0;
	src = input;
	end_offset = 0;

	write_byte = 0;
	write_bits_cnt = 0;

	if (mode >= 0x0C)
    {
		mode = 0;
	}

	max_encoded_size = MAX_SIZES[mode] + 1;

	if (max_encoded_size > src_size)
    {
		max_encoded_size = src_size - 1;
	}

	unsigned char size_mode = 0;
	while (max_encoded_size - 1 > MAX_SIZES[size_mode])
    {
		size_mode++;
	}

	for (int i = 0; i < 0x0C; i++)
    {
		run_extra_bits[i] = MODE_INIT[size_mode][i];
		run_base_offs[i] = (1 << MODE_INIT[size_mode][i]);
	}

	for (int i = 0; i < 8; i++)
    {
		run_base_offs[4 + i] += run_base_offs[i];
	}

	write_bits_cnt = 7;

	while (read_pos < src_size - 2)
    {
		find_repeats();

		unsigned short len = find_best_match_and_encode();
		if (!len)
        {
			src[end_offset++] = src[read_pos++];
			token_run_len++;

			if (token_run_len >= 0x4012)
            {
				break;
			}
		}
		else
        {
			token_run_len = 0;

			read_pos += len;
			write_bits(encoded_offset_bits_cnt, encoded_offset);
			write_bits(encoded_token_len_bits_cnt, encoded_token_len);

			if (encoded_reps_bits_cnt == 13)
            {
				src[end_offset++] = encoded_reps & 0xFF;
				write_bits(5, 0x1F);
			}
			else
            {
				write_bits(encoded_reps_bits_cnt, encoded_reps);
			}
		}
	}

	while (read_pos != src_size)
    {
		src[end_offset++] = src[read_pos++];
		token_run_len++;
	}

	if (end_offset >= 0x0C && (src_size - end_offset > 54))
    {
		if (end_offset & 1)
        {
			src[end_offset++] = 0;
			write_word(&src[end_offset + 0x10], (write_byte & 0xFE) | (1 << write_bits_cnt));
		}
		else
        {
			write_word(&src[end_offset + 0x10], 0xFF00 | (write_byte & 0xFE) | (1 << write_bits_cnt));
		}

		unsigned int cmp_dword_1 = read_dword(&src[0x00]);
		unsigned int cmp_dword_2 = read_dword(&src[0x04]);
		unsigned int cmp_dword_3 = read_dword(&src[0x08]);
		// HEADER
		// +0x00, MAGIC DWORD
        write_dword(&src[0x00], 0x494d5021); // "IMP!"
		// +0x04, UNCOMPRESSED SIZE
        write_dword(&src[0x04], src_size);
		// +0x08, CMPR DATA OFFSET
        write_dword(&src[0x08], end_offset);
		// +0x0C, COMPRESSED DWORD AS IS: SRC[0x0C], NO NEED TO REPLACE

		// 0x00, CMPR DATA DWORD 3
        write_dword(&src[end_offset + 0x00], cmp_dword_3);
		// +0x04, CMPR DATA DWORD 2
        write_dword(&src[end_offset + 0x04], cmp_dword_2);
		// +0x08, CMPR DATA DWORD 1
        write_dword(&src[end_offset + 0x08], cmp_dword_1);
		// +0x0C, LITERAL RUN LEN
        write_dword(&src[end_offset + 0x0C], token_run_len);

		// +0x12, BASE OFFSETS TABLE
		for (int i = 0; i < 8; i++)
        {
			write_word(&src[end_offset + 0x12 + i * 2], run_base_offs[i] & 0xFFFF);
		}

		// +0x22, EXTRA BITS TABLE
		copy_bytes(&src[end_offset + 0x22], &run_extra_bits[0], 12);

		// +0x2E, CHECKSUM OF DATA
        write_dword(&src[end_offset + 0x2E], checksum(&src[0], end_offset + 0x2E));
		return (end_offset + 0x2E + 4);
	}
	return 0;
}

unsigned char *load_input_file(char *filename, unsigned int *size, int *op)
{
    unsigned char *memory;
    FILE *input = fopen(filename, "rb");
    int exp_size;

    if(!input)
    {
        fprintf(stderr, "Error: opening input file.");
        return(NULL);
    }
    printf("Reading '%s'...", filename);
    // get the filesize
    fseek(input, 0, SEEK_END);
    *size = ftell(input);
    fseek(input, 0, SEEK_SET);
    memory = (unsigned char *) malloc(*size);
    if(!memory)
    {
        fprintf(stderr, "\nError: can't allocate memory.");
        fclose(input);
        return(NULL);
    }
    if(fread(memory, 1, *size, input) != (size_t) *size)
    {
        fprintf(stderr, "\nError: reading input file.");
        fclose(input);
        free(memory);
        return(NULL);
    }
    // check if input is packed
    exp_size = exploded_size(memory);
    if(exp_size)
    {
        *op = OP_EXPLODE;
        memory = realloc(memory, exp_size);
        if(!memory)
        {
            fprintf(stderr, "\nError: can't allocate memory.");
            fclose(input);
            return(NULL);
        }
    }
    fclose(input);
    return(memory);
}

int main(int argc, char *argv[])
{
	unsigned int size;
	unsigned char *src_mem;
	FILE *output_file;
	unsigned int dest_size;
    char auto_name[512];
    int ret_value = 0;
    // default operation
    int op = OP_IMPLODE;
    int arg_pos = 1;

    printf("Imploder data cruncher/decruncher v1.0\n");
    printf("Reversed by lab313ru + small shell by hitchhikr.\n");
    if(argc < 2 || argc > 3)
    {
        printf("Usage: imploder <input file> [output file]\n");
        return ret_value;
    }

	if(src_mem = load_input_file(argv[arg_pos], &size, &op))
	{
        if(op == OP_IMPLODE)
        {
            printf("\nImploding...");
#ifdef __AMIGA__
            fflush(stdout);
#endif
            dest_size = implode(src_mem, size, 0xB);
            if(dest_size)
            {
                printf("\nOriginal size: %d bytes.\n", size);
                printf("Imploded size: %d bytes.\n", dest_size);
                printf("Won: %d bytes.", size - dest_size);
            }
        }
        else
        {
            printf("\nExploding...");
#ifdef __AMIGA__
            fflush(stdout);
#endif
            dest_size = explode(src_mem);
            if(dest_size)
            {
                printf("\nOriginal size: %d bytes.\n", size);
                printf("Exploded size: %d bytes.", dest_size);
            }
        }
        if(dest_size)
        {
            // user didn't supply an output name
            if(argc != 3)
            {
                strcpy(auto_name, argv[arg_pos]);
                if(op == OP_IMPLODE)
                {
                    strcat(auto_name, ".imp");
                }
                else
                {
                    strcat(auto_name, ".exp");
                }     
            }
            else
            {
                arg_pos++;
                strcpy(auto_name, argv[arg_pos]);
            }
            printf("\nWriting '%s'...", auto_name);
            output_file = fopen(auto_name, "wb");
            if(output_file)
            {
                fwrite(src_mem, 1, dest_size, output_file);
                fclose(output_file);
            }
            else
            {
                fprintf(stderr, "\nError: can't open '%s' for writing.", auto_name);
                ret_value = 1;
            }
        }
        else
        {
            if(op == OP_IMPLODE)
            {
                fprintf(stderr, "\nError: can't implode.");
            }
            else
            {
                fprintf(stderr, "\nError: can't explode.");
            }
            ret_value = 1;
        }
		free(src_mem);
	}
    else
    {
        ret_value = 1;
    }
#ifdef __AMIGA__
    printf("\n");
#endif
	return ret_value;
}
