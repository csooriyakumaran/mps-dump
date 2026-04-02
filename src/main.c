#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define _CRT_SECURE_NO_WARNINGS
#define WORD_SIZE 4
#define WORDS_PER_LINE 4
#define BYTES_PER_LINE (WORD_SIZE * WORDS_PER_LINE)

typedef float f32;
typedef double f64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;



void println(size_t offset, const u8* data, size_t size, FILE* fout)
{
    printf("%08zx  ", offset);

    // Hex words column
    for (size_t j = 0; j < BYTES_PER_LINE; ++j)
    {
        if (j < size)
            fprintf(fout, "%02X", data[j]);
        else
            fprintf(fout, "  ");
 
        if ( (j+1) % WORD_SIZE == 0)
            fprintf(fout, "  ");
        else
            fprintf(fout, " ");
    }

    fprintf(fout, " | ");
    // ASCII Column
    
    fprintf(fout, "\n");

}

static int dump_file(FILE* fin, FILE* fout)
{
    u8 buffer[BYTES_PER_LINE];
    size_t offset = 0;

    for (;;)
    {
        size_t nread = fread(buffer, 1, sizeof(buffer), fin);

        if (nread > 0)
        {
            println(offset, buffer, nread, fout);
            offset += nread;
        }
        
        if (nread < sizeof(buffer))
        {
            if (ferror(fin))
            {
                fprintf(stderr, "Error: read faild\n");
                return 0;
            }
            break;
        }

    }

    return 1;


}


int main(int argc, char** argv)
{

    FILE* fp = fopen("test.dat", "rb");

    if (fp == NULL)
    {
        fprintf(stderr, "Error opening file");
        return 1;
    }

    if (!dump_file(fp, stdout))
    {
        fclose(fp);
        return 1;
    }

    fclose(fp);
    return 0;
}
