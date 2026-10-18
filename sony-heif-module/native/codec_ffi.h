#pragma once
#include <cstddef>
#include <cstdint>
struct HifInfo {uint32_t width,height,bit_depth,chroma,hlg,grid_rows,grid_columns,tile_width,tile_height;};
extern "C" {
int sony_hif_probe(const uint8_t*,size_t,HifInfo*);
void* sony_hif_decode(const uint8_t*,size_t);
int sony_hif_row(const void*,uint32_t,uint32_t,uint8_t*,size_t);
void sony_hif_free(void*);
const char* sony_hif_last_error();
}
