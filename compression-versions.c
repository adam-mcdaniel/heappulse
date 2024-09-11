#include <stdio.h>

#include <zstd.h>
#include <zlib.h>
#include <lz4.h>
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>
#include <snappy-c.h>
#include <liblzf/lzf.h>
#include <lz4hc.h>

int main() {
    unsigned version = ZSTD_versionNumber();
    printf("Zstd version: %u.%u.%u\n", version / 10000, (version / 100) % 100, version % 100);
    printf("Zlib version: %s\n", zlibVersion());
    version = LZ4_versionNumber();
    printf("LZ4 version: %d\n", LZ4_versionNumber());
    printf("LZO version: %d.%d.%d\n", LZO_VERSION / 10000, (LZO_VERSION / 100) % 100, LZO_VERSION % 100);
    version = LZ4_versionNumber();
    printf("LZ4HC version: %d\n", LZ4_versionNumber());

    return 0;
}