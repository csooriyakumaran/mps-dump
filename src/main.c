#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "version.h"

#include "scanivalve/mps-protocol.h"
#include "scanivalve/mps-protocol-version.h"

#define AETHER_IMPLEMENTATION
#include "aether/aether.h"

#define WORD_SIZE 4
#define WORDS_PER_LINE 4
#define BYTES_PER_LINE (WORD_SIZE * WORDS_PER_LINE)
#define ANNOTATION_SLOT_WIDTH 20

#define BIG_ENDIAN 1
#define LITTLE_ENDIAN 0

typedef struct Packet
{
    u8  type;
    u64 size;
    u8* data;     /* view into read-only memory */
} Packet;

static char g_hex[256][2];

static void hex_table_init(void)
{
    static const char digits[] = "0123456789ABCDEF";
    for (int i = 0; i < 256; ++i)
    {
        g_hex[i][0] = digits[(i >> 4) & 0xF];
        g_hex[i][1] = digits[ i & 0xF];
    }
}

u32 read_be32(const u8* p)
{
    return ( (u32)p[0] << 24 ) |
           ( (u32)p[1] << 16 ) |
           ( (u32)p[2] <<  8 ) |
           ( (u32)p[3] <<  0 );
}

u32 read_le32(const u8* p)
{
    return ( (u32)p[3] << 24 ) |
           ( (u32)p[2] << 16 ) |
           ( (u32)p[1] <<  8 ) |
           ( (u32)p[0] <<  0 );
}

f32 bits_to_f32(u32 bits)
{
    f32 value; 
    memcpy(&value, &bits, sizeof(value));
    return value;
}

b8   create_packet_view(str8 bytes, u64 offset, Packet* pkt);
void dump_packet(const Packet* pkt, size_t* p_offset, FILE* fout);
void println_buffered(size_t offset, const u8* data, size_t size, const char* annotation, FILE* fout);
size_t build_words(const u8* data, size_t size_bytes, u32* out_words, size_t max_words, int big_endian);

void append_slot(char* dst, size_t dst_size, const char* field_text, int slot_width);
void annotate_line_legacy(const u32* words, size_t word_count, size_t line_index, char* out, size_t out_size);
void annotate_line_eu(const u32* words, size_t word_count, size_t line_index, char* out, size_t out_size);
void annotate_line_raw(const u32* words, size_t word_count, size_t line_index, char* out, size_t out_size);
void annotate_line_default(const u32* words, size_t word_count, size_t line_index, char* out, size_t out_size);
void annotate_line(u8 pkt_type, const u32* words, size_t word_count, size_t line_index, char* out, size_t out_size);

int main(int argc, char** argv)
{
    hex_table_init();

    u64 start = time_mark();
    if (argc > 1 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0 ))
    {
        fprintf(stdout, "%s\n", MPS_DUMP_VERSION_STRING);
        fprintf(stdout, "  - Scanivalve Protocol Version %s\n  - Scanivalve Firmware Version %s\n", MPS_PROTOCOL_VERSION_STRING, MPS_FIRMWARE_VERSION_STRING);
        fprintf(stdout, "  -              Aether Version %s\n", AETHER_VERSION_STRING);
        return 0;
    }

    const char* path = (argc > 1) ? argv[1] : "test.dat";

    // map the contents of the file into read-only memory
    str8 bytes = map_file(path);

    if (!bytes.data || bytes.size == 0)
    {
        fprintf(stderr, "Error mapping to file `%s`\n", path);
        return 1;
    }

    Packet pkt = {0};
    size_t offset = 0;

    while (create_packet_view(bytes, offset, &pkt))
    {
        // dump packet and advance offset
        dump_packet(&pkt, &offset, stdout);
    }

    u64 end = time_mark();
    f64 elapsed_s = time_elapsed_sec(start, end);
    printf("--- %.4f ms", elapsed_s*1000.0);

    unmap_file(bytes);
    return 0;
}

b8  create_packet_view(str8 bytes, u64 offset, Packet* pkt)
{

    if (offset >= bytes.size) return false;
    u8* base = bytes.data + offset;

    pkt->type = base[0];
    const MpsBinaryPacketInfo* info = mps_get_binary_packet_info_by_type(pkt->type);

    if (!info)
    {
        pkt->size = 1;
        pkt->data = base;
        return true;
    }

    u64 available = bytes.size - offset;
    pkt->size = info->size_bytes;
    if (pkt->size > available)
    {
        fprintf(stderr, "Warning: Truncated packet at offset %llu (expected %u, have %llu)\n", (unsigned long long)offset, info->size_bytes, (unsigned long long)available);
        pkt->size = available;
    }

    pkt->data = base;
    return true;
}

void dump_packet(const Packet* pkt, size_t* p_offset, FILE* fout)
{
    size_t offset = *p_offset;
    size_t pos    = 0;
    size_t line_index = 0;

    u32 words[MPS_MAX_BINARY_PACKET_SIZE / 4];
    size_t word_count = build_words(pkt->data, pkt->size, words, sizeof(words)/sizeof(words[0]), LITTLE_ENDIAN);

    while (pos < pkt->size)
    {
        size_t remaining = pkt->size - pos;
        size_t line_bytes = remaining < BYTES_PER_LINE ? remaining : BYTES_PER_LINE;

        const u8* line_data = pkt->data + pos;

        // - Generate Annotations
        char annotation[256];
        annotate_line(pkt->type, words, word_count, line_index, annotation, sizeof(annotation));
        println_buffered(offset, line_data, line_bytes, annotation, fout);

        offset  += line_bytes;
        pos     += line_bytes;
        ++line_index;

    }
    *p_offset = offset;
}

static inline size_t append_clamped(int written, size_t n, size_t cap)
{
    if (n >= cap || written < 0) return cap;
    size_t w = (size_t)written;
    return (w > cap - n) ? cap : n + w;
}

void println_buffered(size_t offset, const u8* data, size_t size, const char* annotation, FILE* fout)
{
    char line[512];
    size_t n = append_clamped(snprintf(line, sizeof(line), "%08zx  ", offset), 0, sizeof(line));

    for (size_t j = 0; j < BYTES_PER_LINE; ++j)
    {
        if (j < size) { memcpy(line + n, g_hex[data[j]], 2); n+=2; }
        else          { line[n++] = ' '; line[n++] = ' '; }

        line[n++] = ' ';
        if ((j+1) % WORD_SIZE == 0) line[n++] = ' ';
    }

    memcpy(line + n, " | ", 3); n += 3;

    for (size_t j = 0; j < size; ++j)
    {
        unsigned char ch = data[j];
        line[n++] = (ch >= 0x20 && ch <= 0x7E) ? (char)ch : '.';
    }

    n = append_clamped(
        snprintf(line + n, sizeof(line) - n, " | %s\n", annotation ? annotation : ""),
        n, sizeof(line)
    );

    fwrite(line, 1, n, fout);

}

size_t build_words(const u8* data, size_t size_bytes, u32* out_words, size_t max_words, int big_endian)
{
    // - ignore any trailing partial words
    size_t word_count = size_bytes / 4;

    if (word_count > max_words)
        word_count = max_words;

    for (size_t i = 0; i < word_count; ++i)
    {
        const u8* p = data + 4 * i;
        out_words[i] = big_endian ? read_be32(p) : read_le32(p);
    }

    return word_count;
}

void append_slot(char* dst, size_t dst_size, const char* field_text, int slot_width)
{
    size_t dst_len = strlen(dst);
    if (dst_len >= dst_size - 1)
        return; // no space left

    size_t avail = dst_size - 1 - dst_len;

    size_t field_len = strlen(field_text);
    if (field_len > (size_t)slot_width)
        field_len = slot_width;

    // - how many spaces to pad
    int pad = slot_width - (int)field_len;
    if (pad < 0)
        pad = 0;

    size_t total = (size_t)pad + field_len;
    if (total > avail)
    {
        // not enough room; shrink pad/field accordingly
        if ((size_t)pad > avail)
        {
            pad = (int)avail;
            field_len = 0;
        }
        else
        {
            field_len = avail - (size_t)pad;
        }
        total = (size_t)pad + field_len;
    }

    if (pad > 0)
    {

        memset(dst + dst_len, ' ', (size_t)pad);
        dst_len += (size_t)pad;
    }
        
    if (field_len > 0)
    {
        memcpy(dst + dst_len, field_text, field_len);
        dst_len += field_len;
    }

    dst[dst_len] = '\0';
}

void annotate_line_legacy(const u32* words, size_t word_count, size_t line_index, char* out, size_t out_size)
{
    out[0] = '\0';

    size_t w_idx = line_index * WORDS_PER_LINE;

    if (line_index == 0 && word_count >= (line_index+1)*WORDS_PER_LINE)
    {
        
        i32 packet_type   = (i32)words[w_idx++];
        i32 packet_size   = (i32)words[w_idx++];
        i32 frame         = (i32)words[w_idx++];
        i32 serial_number = (i32)words[w_idx++];
        
        char field[ANNOTATION_SLOT_WIDTH];

        snprintf(field, sizeof(field), "PTYPE: 0x%08X;", packet_type);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "BYTES: %10d;", packet_size);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "FRAME: %010d;", frame);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "SN---: %10d;", serial_number);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        return;
    }
    if (line_index == 1 && word_count >= (line_index+1) * WORDS_PER_LINE)
    {
        f32 framerate    = bits_to_f32(words[w_idx++]);
        i32 valve_status = (i32)words[w_idx++];
        i32 unit_index   = (i32)words[w_idx++];
        f32 unit_conv    = bits_to_f32(words[w_idx++]);

        const char* units_str = mps_units_to_string((MpsUnits)unit_index);
        const char* valve_str = mps_valve_status_to_string((MpsValveStatus)valve_status);
        if (!units_str)
            units_str = "?";
        if (!valve_str)
            valve_str = "?";

        char field[64];

        snprintf(field, sizeof(field), "HERTZ: %+.3e;", framerate);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "VALVE: %10s;", valve_str);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "UNITS: %10s;", units_str);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "CONVR: %+.3e;", unit_conv);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        return;
    }

    if (line_index == 2 && word_count >= (line_index + 1)*WORDS_PER_LINE)
    {
        u32 ptp_start_s     = words[w_idx++];
        u32 ptp_start_ns    = words[w_idx++];
        u32 extern_trig_us  = words[w_idx++];
        f32 temp = bits_to_f32(words[w_idx++]);

        char field[ANNOTATION_SLOT_WIDTH];

        snprintf(field, sizeof(field), "PTP-S: %010d;", ptp_start_s);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "PTPNS:  %09d;", ptp_start_ns);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "XTRIG:  %09d;", extern_trig_us);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "T[01]: %+.3e;", temp);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        return;
    }

    // temperatures / pressures
    if ( 3 <= line_index && line_index < 20 && word_count >= (line_index+1) * WORDS_PER_LINE)
    {
        char field[ANNOTATION_SLOT_WIDTH];

        const MpsBinaryPacketInfo* info = mps_get_binary_packet_info_by_type(words[0]);
        if (!info) return;

        u8 tcount = info->temperature_channel_count;
        u32 units = words[6];

        for (size_t i = 0; i < 4; ++i)
        {
            u32 word  = words[w_idx++];
            if (w_idx - 11 <= tcount)
            {
                f32 t = bits_to_f32(word);
                snprintf(field, sizeof(field), "T[%02d]: %+.3e;", (int)w_idx - 11, t);

            }
            else
            {
                if (units == (u32)MPS_UNITS_RAW)
                {
                    i32 p = (i32)word;
                    snprintf(field, sizeof(field), "P[%02d]: %+010d;", (int)w_idx - 11 - tcount, p);

                } 
                else
                {
                    f32 p = bits_to_f32(word);
                    snprintf(field, sizeof(field), "P[%02d]: %+.3e;", (int)w_idx - 11 - tcount, p);

                }
            }

            append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);
        }
        return;
    }

    if (line_index == 20 && word_count >= (line_index+1) * WORDS_PER_LINE)
    {
        char field[ANNOTATION_SLOT_WIDTH];

        const MpsBinaryPacketInfo* info = mps_get_binary_packet_info_by_type(words[0]);
        if (!info) return;

        u8 tcount = info->temperature_channel_count;
        u32 units = words[6];

        for (size_t i = 0; i < 3; ++i)
        {
            u32 word  = words[w_idx++];
            if (units == (u32)MPS_UNITS_RAW)
            {
                i32 p = (i32)word;
                snprintf(field, sizeof(field), "P[%02d]: %+010d;", (int)w_idx - 11 - tcount, p);

            } 
            else
            {
                f32 p = bits_to_f32(word);
                snprintf(field, sizeof(field), "P[%02d]: %+.3e;", (int)w_idx - 11 - tcount, p);

            }

            append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);
        }

        u32 time = words[w_idx++];
        snprintf(field, sizeof(field), "t(S): %010d;",  time);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);
        return;
    }

    if (line_index == 21 && word_count >= line_index * WORDS_PER_LINE + 3)
    {
        char field[ANNOTATION_SLOT_WIDTH];
        u32 time    = words[w_idx++];
        u32 xtrg_s  = words[w_idx++];
        u32 xtrg_ns = words[w_idx++];
        
        append_slot(out, out_size, " ", 4);

        snprintf(field, sizeof(field), "t(NS):  %09d;",  time);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "TRG-S: %010d;",  xtrg_s);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "TRGNS:  %09d;",  xtrg_ns);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);
        return;
    }
}

void annotate_line_eu(const u32* words, size_t word_count, size_t line_index, char* out, size_t out_size)
{
    out[0] = '\0';
    size_t w_idx = line_index * WORDS_PER_LINE;

    if (line_index == 0 && word_count >= (line_index+1)*WORDS_PER_LINE)
    {
        i32 packet_type   = (i32)words[w_idx++];
        u32 frame         = words[w_idx++];
        u32 frame_time_s  = words[w_idx++];
        u32 frame_time_ns = words[w_idx++];
        f64 frame_time = (f64) frame_time_s + (f64) frame_time_ns / 1e9;

        char field[ANNOTATION_SLOT_WIDTH];

        snprintf(field, sizeof(field), "TYP:   0x%08X;", packet_type);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "NO.:   %010u;", frame);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "SEC:   %010u;", frame_time_s);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "+NS:    %09u;", frame_time_ns);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);
    }

    if (line_index > 0 && word_count >= (line_index + 1) * WORDS_PER_LINE)
    {
        const MpsBinaryPacketInfo* info = mps_get_binary_packet_info_by_type(words[0]);
        if (!info) return;

        u8 tcount = info->temperature_channel_count;

        for (size_t i = 0; i < 4; ++i)
        {
            f32 v = bits_to_f32(words[w_idx++]);
            char field[ANNOTATION_SLOT_WIDTH];
            if (w_idx - 4 <= tcount)
                snprintf(field, sizeof(field), "T%02d:  %+.4e;", (int)w_idx - 4, v);
            else 
                snprintf(field, sizeof(field), "P%02d:  %+.4e;",(int)w_idx - 4 - tcount, v);
            append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);
        }
    }
}

void annotate_line_raw(const u32* words, size_t word_count, size_t line_index, char* out, size_t out_size)
{
    out[0] = '\0';
    size_t w_idx = line_index * WORDS_PER_LINE;

    if (line_index == 0 && word_count >= (line_index+1)*WORDS_PER_LINE)
    {
        i32 packet_type   = (i32)words[w_idx++];
        u32 frame         = words[w_idx++];
        u32 frame_time_s  = words[w_idx++];
        u32 frame_time_ns = words[w_idx++];

        char field[ANNOTATION_SLOT_WIDTH];

        snprintf(field, sizeof(field), "TYP:   0x%08X;", packet_type);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "NO.:   %010u;", frame);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "SEC:   %010u;", frame_time_s);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);

        snprintf(field, sizeof(field), "+NS:    %09u;", frame_time_ns);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);
    }

    if (line_index > 0 && word_count >= (line_index + 1) * WORDS_PER_LINE)
    {
        const MpsBinaryPacketInfo* info = mps_get_binary_packet_info_by_type(words[0]);
        if (!info) return;

        u8 tcount = info->temperature_channel_count;

        for (size_t i = 0; i < 4; ++i)
        {
            char field[ANNOTATION_SLOT_WIDTH];
            u32 v = words[w_idx++];
            if (w_idx - 4 <= tcount)
            {
                f32 val = bits_to_f32(v);
                snprintf(field, sizeof(field), "T%02d:  %+.4e;", (int)w_idx - 4, val);
            }
            else 
            {
                i32  val = (i32)v;
                snprintf(field, sizeof(field), "P%02d:  %+011d;", (int)w_idx - 4 - tcount, val);

            }
            append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);
        }
    }
}

void annotate_line_default(const u32* words, size_t word_count, size_t line_index, char* out, size_t out_size)
{
    out[0] = '\0';
    size_t w_idx = line_index * WORDS_PER_LINE;
    for (size_t i = 0; i < 4 && w_idx < word_count; ++i)
    {
        f32 v = bits_to_f32(words[w_idx++]);
        char field[ANNOTATION_SLOT_WIDTH];
        snprintf(field, sizeof(field), "%+.8e;", v);
        append_slot(out, out_size, field, ANNOTATION_SLOT_WIDTH);
    }
}

void annotate_line(const u8 pkt_type, const u32* words, size_t word_count, size_t line_index, char* out, size_t out_size)
{
    out[0] = '\0';

    switch (pkt_type)
    {
    case MPS_PKT_LEGACY_TYPE:
        annotate_line_legacy(words, word_count, line_index, out, out_size);
        break;

    case MPS_PKT_16_TYPE:
    case MPS_PKT_32_TYPE:
    case MPS_PKT_64_TYPE:
        annotate_line_eu(words, word_count, line_index, out, out_size);
        break;

    case MPS_PKT_16_RAW_TYPE:
    case MPS_PKT_32_RAW_TYPE:
    case MPS_PKT_64_RAW_TYPE:
        annotate_line_raw(words, word_count, line_index, out, out_size);
        break;

    default:
        // treat all as floats (likely labview) print a header for the first line
        if (line_index == 0)
        {
            const MpsBinaryPacketInfo* info = mps_get_binary_packet_info_by_type(pkt_type);
            if (info)
                snprintf(out, out_size, "type: 0x%02X, size: %u", (unsigned)pkt_type, (unsigned)info->size_bytes);
            else
                snprintf(out, out_size, "unknown type: 0x%02X", (unsigned)pkt_type);
        }
        else 
        {
            annotate_line_default(words, word_count, line_index, out, out_size);
        }
        break;
    }
}

