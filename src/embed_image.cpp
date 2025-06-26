#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "stb_image.h"
#include "stb_image_write.h"

void write_callback(void *context, void *dvoid, int size)
{
    std::ofstream &fout = *reinterpret_cast<std::ofstream *>(context);
    uint8_t *data = reinterpret_cast<uint8_t *>(dvoid);
    fout << std::hex;
    for (int i = 0; i < size; ++i) {
        fout << "0x" << static_cast<int>(data[i]);
        if (i + 1 < size) {
            fout << ",";
        }
    }
}
