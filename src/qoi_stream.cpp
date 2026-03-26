#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "sprNodes.h"

void QoiWriteRowToSprite(void* user, int y, const uint8_t* row, int width) {
    SpriteRowWriter* w = (SpriteRowWriter*)user;
    if (w->done) return;

    if (y < w->srcY) return;
    if (y >= w->srcY + w->srcH) { w->done = 1; return; }

    int srcRow = y - w->srcY;
    int dstY   = w->dstY + srcRow;
    if (dstY < 0 || dstY >= w->potH) return;

    int srcX = w->srcX;
    int srcW = w->srcW;
    int dstX = w->dstX;

    // clamp once
    if (srcX < 0) { dstX -= srcX; srcW += srcX; srcX = 0; }
    if (srcX + srcW > width) srcW = width - srcX;
    if (dstX < 0) { srcW += dstX; srcX -= dstX; dstX = 0; }
    if (dstX + srcW > w->potW) srcW = w->potW - dstX;
    if (srcW <= 0) return;

    const uint32_t* src32 = (const uint32_t*)(row + srcX*4);
    uint32_t* dst32 = (uint32_t*)(w->canvas + (dstY*w->potW + dstX)*4);

    // small spans - unrolled copy
    if (srcW <= 4) {
        for (int i=0; i<srcW; ++i) dst32[i] = src32[i];
    } else {
        memcpy(dst32, src32, srcW*4);
    }
}

// Streamed QOI decode that writes rows directly to canvas via callback
void DecodeQoiToSprite(const uint8_t* data, size_t size, void* user,
                       void (*RowWriter)(void* user, int y, const uint8_t* row, int width)) {
    if (size < 12) { fprintf(stderr, "Error: QOI too short\n"); exit(1); }
    if (!(data[0]=='f' && data[1]=='i' && data[2]=='o' && data[3]=='q')) {
        fprintf(stderr, "Error: Invalid QOI magic\n"); exit(1);
    }

    int width  = data[4] | (data[5]<<8);
    int height = data[6] | (data[7]<<8);
    int length = data[8] | (data[9]<<8) | (data[10]<<16) | (data[11]<<24);
    if ((12ull + length) > size) { fprintf(stderr, "Error: Truncated QOI\n"); exit(1); }

    const uint8_t* pixelData = data + 12;

    // decoder state
    uint8_t r=0,g=0,b=0,a=255;
    int run=0;
    uint8_t index[64*4];
    memset(index,0,sizeof(index));
    size_t pos=0;

    // allocate one row buffer
    uint8_t* rowBuf = (uint8_t*)AllocateMeta(width*4);

    for (int y=0; y<height; ++y) {
        for (int x=0; x<width; ++x) {
            if (run>0) {
                run--;
            } else if ((int)pos < length) {
                uint8_t b1 = pixelData[pos++];
                if ((b1 & QOI_MASK_2) == QOI_INDEX) {
                    int idx = (b1 ^ QOI_INDEX) << 2;
                    r=index[idx]; g=index[idx+1]; b=index[idx+2]; a=index[idx+3];
                }
                else if ((b1 & QOI_MASK_3) == QOI_RUN_8) {
                    run = b1 & 0x1f;
                }
                else if ((b1 & QOI_MASK_3) == QOI_RUN_16) {
                    int b2 = pixelData[pos++];
                    run = (((b1 & 0x1f)<<8)|b2)+32;
                }
                else if ((b1 & QOI_MASK_2) == QOI_DIFF_8) {
                    r += (uint8_t)(((b1 & 48) << 26 >> 30) & 0xff);
                    g += (uint8_t)(((b1 & 12) << 28 >> 22 >> 8) & 0xff);
                    b += (uint8_t)(((b1 & 3) << 30 >> 14 >> 16) & 0xff);
                }
                else if ((b1 & QOI_MASK_3) == QOI_DIFF_16) {
                    int b2 = pixelData[pos++];
                    int merged = (b1<<8)|b2;
                    r += (uint8_t)(((merged & 7936) << 19 >> 27) & 0xff);
                    g += (uint8_t)(((merged & 240) << 24 >> 20 >> 8) & 0xff);
                    b += (uint8_t)(((merged & 15) << 28 >> 12 >> 16) & 0xff);
                }
                else if ((b1 & QOI_MASK_4) == QOI_DIFF_24) {
                    int b2 = pixelData[pos++];
                    int b3 = pixelData[pos++];
                    int merged = (b1<<16)|(b2<<8)|b3;
                    r += (uint8_t)(((merged & 1015808) << 12 >> 27) & 0xff);
                    g += (uint8_t)(((merged & 31744) << 17 >> 19 >> 8) & 0xff);
                    b += (uint8_t)(((merged & 992) << 22 >> 11 >> 16) & 0xff);
                    a += (uint8_t)(((merged & 31) << 27 >> 3 >> 24) & 0xff);
                }
                else if ((b1 & QOI_MASK_4) == QOI_COLOR) {
                    if (b1 & 8) r = pixelData[pos++];
                    if (b1 & 4) g = pixelData[pos++];
                    if (b1 & 2) b = pixelData[pos++];
                    if (b1 & 1) a = pixelData[pos++];
                }
                int idx2 = ((r^g^b^a)&63)<<2;
                index[idx2]=r; index[idx2+1]=g; index[idx2+2]=b; index[idx2+3]=a;
            }
            // write pixel into row buffer
            rowBuf[x*4+0]=r;
            rowBuf[x*4+1]=g;
            rowBuf[x*4+2]=b;
            rowBuf[x*4+3]=a;
        }
        // flush row to sprite
        RowWriter(user,y,rowBuf,width);
    }

    FreeMeta(rowBuf,width*4);
}

// Advance decode until reaching row `targetY`. Returns the cursor state (pos, RGBA, run, index) at that point.
QoiCursor QoiSeekToRow(const uint8_t* pixelData, size_t length,
                       int width, int height, int srcH, int targetY) {
    QoiCursor st;
    st.pos = 0;
    st.r=0; st.g=0; st.b=0; st.a=255;
    st.run=0;
    memset(st.index,0,sizeof(st.index));

    // temporary row buffer just to advance
    uint8_t* rowBuf = (uint8_t*)AllocateMeta(width*4);

    for (int y=0; y<targetY; ++y) {
        for (int x=0; x<width; ++x) {
            if (st.run>0) {
                st.run--;
            } else if ((int)st.pos < length) {
                uint8_t b1 = pixelData[st.pos++];
                if ((b1 & QOI_MASK_2) == QOI_INDEX) {
                    int idx = (b1 ^ QOI_INDEX) << 2;
                    st.r=st.index[idx]; st.g=st.index[idx+1];
                    st.b=st.index[idx+2]; st.a=st.index[idx+3];
                }
                else if ((b1 & QOI_MASK_3) == QOI_RUN_8) {
                    st.run = b1 & 0x1f;
                }
                else if ((b1 & QOI_MASK_3) == QOI_RUN_16) {
                    int b2 = pixelData[st.pos++];
                    st.run = (((b1 & 0x1f)<<8)|b2)+32;
                }
                else if ((b1 & QOI_MASK_2) == QOI_DIFF_8) {
                    st.r += (uint8_t)(((b1 & 48) << 26 >> 30) & 0xff);
                    st.g += (uint8_t)(((b1 & 12) << 28 >> 22 >> 8) & 0xff);
                    st.b += (uint8_t)(((b1 & 3) << 30 >> 14 >> 16) & 0xff);
                }
                else if ((b1 & QOI_MASK_3) == QOI_DIFF_16) {
                    int b2 = pixelData[st.pos++];
                    int merged = (b1<<8)|b2;
                    st.r += (uint8_t)(((merged & 7936) << 19 >> 27) & 0xff);
                    st.g += (uint8_t)(((merged & 240) << 24 >> 20 >> 8) & 0xff);
                    st.b += (uint8_t)(((merged & 15) << 28 >> 12 >> 16) & 0xff);
                }
                else if ((b1 & QOI_MASK_4) == QOI_DIFF_24) {
                    int b2 = pixelData[st.pos++];
                    int b3 = pixelData[st.pos++];
                    int merged = (b1<<16)|(b2<<8)|b3;
                    st.r += (uint8_t)(((merged & 1015808) << 12 >> 27) & 0xff);
                    st.g += (uint8_t)(((merged & 31744) << 17 >> 19 >> 8) & 0xff);
                    st.b += (uint8_t)(((merged & 992) << 22 >> 11 >> 16) & 0xff);
                    st.a += (uint8_t)(((merged & 31) << 27 >> 3 >> 24) & 0xff);
                }
                else if ((b1 & QOI_MASK_4) == QOI_COLOR) {
                    if (b1 & 8) st.r = pixelData[st.pos++];
                    if (b1 & 4) st.g = pixelData[st.pos++];
                    if (b1 & 2) st.b = pixelData[st.pos++];
                    if (b1 & 1) st.a = pixelData[st.pos++];
                }
                int idx2 = ((st.r^st.g^st.b^st.a)&63)<<2;
                st.index[idx2]=st.r; st.index[idx2+1]=st.g;
                st.index[idx2+2]=st.b; st.index[idx2+3]=st.a;
            }
            // discard pixel, we only advance
            *(uint32_t*)&rowBuf[x*4] = (st.r | (st.g<<8) | (st.b<<16) | (st.a<<24));
        }
    }

    FreeMeta(rowBuf,width*4);
    return st;
}

QoiCursor QoiFastSeek(const uint8_t* pixelData, size_t length,
                      int width, int targetY) {
    QoiCursor st;
    st.pos = 0;
    st.r = 0; st.g = 0; st.b = 0; st.a = 255;
    st.run = 0;
    memset(st.index, 0, sizeof(st.index));

    size_t pixelsToSkip = (size_t)targetY * width;

    while (pixelsToSkip > 0 && (int)st.pos < (int)length) {
        if (st.run > 0) {
            size_t skip = (pixelsToSkip < (size_t)st.run) ? pixelsToSkip : st.run;
            st.run -= (int)skip;
            pixelsToSkip -= skip;
        } else {
            uint8_t b1 = pixelData[st.pos++];

            if ((b1 & QOI_MASK_2) == QOI_INDEX) {
                int idx = (b1 ^ QOI_INDEX) << 2;
                st.r = st.index[idx];
                st.g = st.index[idx+1];
                st.b = st.index[idx+2];
                st.a = st.index[idx+3];
            }
            else if ((b1 & QOI_MASK_3) == QOI_RUN_8) {
                st.run = b1 & 0x1f;
            }
            else if ((b1 & QOI_MASK_3) == QOI_RUN_16) {
                int b2 = pixelData[st.pos++];
                st.run = (((b1 & 0x1f)<<8)|b2)+32;
            }
            else if ((b1 & QOI_MASK_2) == QOI_DIFF_8) {
                st.r += (uint8_t)(((b1 & 48) << 26 >> 30) & 0xff);
                st.g += (uint8_t)(((b1 & 12) << 28 >> 22 >> 8) & 0xff);
                st.b += (uint8_t)(((b1 & 3) << 30 >> 14 >> 16) & 0xff);
            }
            else if ((b1 & QOI_MASK_3) == QOI_DIFF_16) {
                int b2 = pixelData[st.pos++];
                int merged = (b1<<8)|b2;
                st.r += (uint8_t)(((merged & 7936) << 19 >> 27) & 0xff);
                st.g += (uint8_t)(((merged & 240) << 24 >> 20 >> 8) & 0xff);
                st.b += (uint8_t)(((merged & 15) << 28 >> 12 >> 16) & 0xff);
            }
            else if ((b1 & QOI_MASK_4) == QOI_DIFF_24) {
                int b2 = pixelData[st.pos++];
                int b3 = pixelData[st.pos++];
                int merged = (b1<<16)|(b2<<8)|b3;
                st.r += (uint8_t)(((merged & 1015808) << 12 >> 27) & 0xff);
                st.g += (uint8_t)(((merged & 31744) << 17 >> 19 >> 8) & 0xff);
                st.b += (uint8_t)(((merged & 992) << 22 >> 11 >> 16) & 0xff);
                st.a += (uint8_t)(((merged & 31) << 27 >> 3 >> 24) & 0xff);
            }
            else if ((b1 & QOI_MASK_4) == QOI_COLOR) {
                if (b1 & 8) st.r = pixelData[st.pos++];
                if (b1 & 4) st.g = pixelData[st.pos++];
                if (b1 & 2) st.b = pixelData[st.pos++];
                if (b1 & 1) st.a = pixelData[st.pos++];
            }

            int idx2 = ((st.r^st.g^st.b^st.a)&63)<<2;
            st.index[idx2]   = st.r;
            st.index[idx2+1] = st.g;
            st.index[idx2+2] = st.b;
            st.index[idx2+3] = st.a;

            pixelsToSkip--;
        }
    }

    return st;
}

// Faster QOI streamed decode: direct 32bit writes, run expansion, packed index, vertical skip
void DecodeQoiToSpriteFast(const uint8_t* data, size_t size, void* user,
    QoiCursor cur, void (*RowWriter)(void* user, int y, const uint8_t* row, int width)) {
    if (size < 12) { fprintf(stderr, "Error: QOI too short\n"); exit(1); }
    if (!(data[0]=='f' && data[1]=='i' && data[2]=='o' && data[3]=='q')) {
        fprintf(stderr, "Error: Invalid QOI magic\n"); exit(1);
    }

    int width  = data[4] | (data[5]<<8);
    int height = data[6] | (data[7]<<8);
    int length = data[8] | (data[9]<<8) | (data[10]<<16) | (data[11]<<24);
    if ((12ull + length) > size) { fprintf(stderr, "Error: Truncated QOI\n"); exit(1); }

    const uint8_t* pixelData = data + 12;

    SpriteRowWriter* w = (SpriteRowWriter*)user;

    // seek to starting row
    //QoiCursor cur = QoiSeekToRow(pixelData, length, width, w->srcY);

    // copy state into local variables
    size_t pos = cur.pos;
    uint8_t r=cur.r, g=cur.g, b=cur.b, a=cur.a;
    int run=cur.run;
    uint8_t index[64*20];
    memcpy(index, cur.index, sizeof(index));

    uint8_t* rowBuf = (uint8_t*)AllocateMeta(width*4);

    for (int y=w->srcY; y<w->srcY+w->srcH && y<height; ++y) {
        // decode row into buffer
        for (int x=0; x<width; ++x) {
            if (run>0) {
                run--;
            } else if ((int)pos < length) {
                uint8_t b1 = pixelData[pos++];
                if ((b1 & QOI_MASK_2) == QOI_INDEX) {
                    int idx = (b1 ^ QOI_INDEX) << 2;
                    r=index[idx]; g=index[idx+1]; b=index[idx+2]; a=index[idx+3];
                }
                else if ((b1 & QOI_MASK_3) == QOI_RUN_8) {
                    run = b1 & 0x1f;
                }
                else if ((b1 & QOI_MASK_3) == QOI_RUN_16) {
                    int b2 = pixelData[pos++];
                    run = (((b1 & 0x1f)<<8)|b2)+32;
                }
                else if ((b1 & QOI_MASK_2) == QOI_DIFF_8) {
                    r += (uint8_t)(((b1 & 48) << 26 >> 30) & 0xff);
                    g += (uint8_t)(((b1 & 12) << 28 >> 22 >> 8) & 0xff);
                    b += (uint8_t)(((b1 & 3) << 30 >> 14 >> 16) & 0xff);
                }
                else if ((b1 & QOI_MASK_3) == QOI_DIFF_16) {
                    int b2 = pixelData[pos++];
                    int merged = (b1<<8)|b2;
                    r += (uint8_t)(((merged & 7936) << 19 >> 27) & 0xff);
                    g += (uint8_t)(((merged & 240) << 24 >> 20 >> 8) & 0xff);
                    b += (uint8_t)(((merged & 15) << 28 >> 12 >> 16) & 0xff);
                }
                else if ((b1 & QOI_MASK_4) == QOI_DIFF_24) {
                    int b2 = pixelData[pos++];
                    int b3 = pixelData[pos++];
                    int merged = (b1<<16)|(b2<<8)|b3;
                    r += (uint8_t)(((merged & 1015808) << 12 >> 27) & 0xff);
                    g += (uint8_t)(((merged & 31744) << 17 >> 19 >> 8) & 0xff);
                    b += (uint8_t)(((merged & 992) << 22 >> 11 >> 16) & 0xff);
                    a += (uint8_t)(((merged & 31) << 27 >> 3 >> 24) & 0xff);
                }
                else if ((b1 & QOI_MASK_4) == QOI_COLOR) {
                    if (b1 & 8) r = pixelData[pos++];
                    if (b1 & 4) g = pixelData[pos++];
                    if (b1 & 2) b = pixelData[pos++];
                    if (b1 & 1) a = pixelData[pos++];
                }
                int idx2 = ((r^g^b^a)&63)<<2;
                index[idx2]=r; index[idx2+1]=g; index[idx2+2]=b; index[idx2+3]=a;
            }
            *(uint32_t*)&rowBuf[x*4] = (r | (g<<8) | (b<<16) | (a<<24));
        }

        //printf("pos=%zu y=%d\n", pos, y);

        // vertical skip: only flush rows inside region
        if (y >= w->srcY && y < w->srcY + w->srcH) {
            RowWriter(user,y,rowBuf,width);
        }
    }

    FreeMeta(rowBuf,width*4);
    printf("Decoded!\n");
}

// QOI Decoder
QoiImage DecodeQoi(const uint8_t* data, size_t size) {
    QoiImage img;
    img.pixels = NULL;
    img.pixels_size = 0;
    
    if (size < 12) {
        fprintf(stderr, "Error: QOI too short\n");
        exit(1);
    }

    // Check magic: "fioq"
    if (!(data[0]=='f' && data[1]=='i' && data[2]=='o' && data[3]=='q')) {
        fprintf(stderr, "Error: Invalid QOI magic\n");
        exit(1);
    }

    int width  = data[4] | (data[5]<<8);
    int height = data[6] | (data[7]<<8);
    int length = data[8] | (data[9]<<8) | (data[10]<<16) | (data[11]<<24);

    if ((12ull + length) > size) {
        fprintf(stderr, "Error: Truncated QOI\n");
        exit(1);
    }

    const uint8_t* pixelData = data + 12;

    img.width = (int)width;
    img.height = (int)height;
    img.pixels_size = width * height * 4;
    //img.pixels = (uint8_t*)malloc(img.pixels_size);
    img.pixels = (uint8_t*)AllocateTex(img.pixels_size);

    if (!img.pixels) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    uint8_t r=0, g=0, b=0, a=255;
    int run=0;
    uint8_t index[64*4];
    memset(index, 0, 64*4);

    size_t pos=0;
    for (size_t rawPos=0; rawPos<img.pixels_size; rawPos+=4) {
        if (run > 0) {
            run--;
        } else if ((int)pos < length) {
            uint8_t b1 = pixelData[pos++];
            if ((b1 & QOI_MASK_2) == QOI_INDEX)
            {
                int indexPos = (b1 ^ QOI_INDEX) << 2;
                r = index[indexPos];
                g = index[indexPos + 1];
                b = index[indexPos + 2];
                a = index[indexPos + 3];
            }
            else if ((b1 & QOI_MASK_3) == QOI_RUN_8)
            {
                run = b1 & 0x1f;
            }
            else if ((b1 & QOI_MASK_3) == QOI_RUN_16)
            {
                int b2 = pixelData[pos++];
                run = (((b1 & 0x1f) << 8) | b2) + 32;
            }
            else if ((b1 & QOI_MASK_2) == QOI_DIFF_8)
            {
                r += (uint8_t)(((b1 & 48) << 26 >> 30) & 0xff);
                g += (uint8_t)(((b1 & 12) << 28 >> 22 >> 8) & 0xff);
                b += (uint8_t)(((b1 & 3) << 30 >> 14 >> 16) & 0xff);
            }
            else if ((b1 & QOI_MASK_3) == QOI_DIFF_16)
            {
                int b2 = pixelData[pos++];
                int merged = (b1 << 8) | b2;
                r += (uint8_t)(((merged & 7936) << 19 >> 27) & 0xff);
                g += (uint8_t)(((merged & 240) << 24 >> 20 >> 8) & 0xff);
                b += (uint8_t)(((merged & 15) << 28 >> 12 >> 16) & 0xff);
            }
            else if ((b1 & QOI_MASK_4) == QOI_DIFF_24)
            {
                int b2 = pixelData[pos++];
                int b3 = pixelData[pos++];
                int merged = (b1 << 16) | (b2 << 8) | b3;
                r += (uint8_t)(((merged & 1015808) << 12 >> 27) & 0xff);
                g += (uint8_t)(((merged & 31744) << 17 >> 19 >> 8) & 0xff);
                b += (uint8_t)(((merged & 992) << 22 >> 11 >> 16) & 0xff);
                a += (uint8_t)(((merged & 31) << 27 >> 3 >> 24) & 0xff);
            }
            else if ((b1 & QOI_MASK_4) == QOI_COLOR)
            {
                if ((b1 & 8) != 0)
                    r = pixelData[pos++];
                if ((b1 & 4) != 0)
                    g = pixelData[pos++];
                if ((b1 & 2) != 0)
                    b = pixelData[pos++];
                if ((b1 & 1) != 0)
                    a = pixelData[pos++];
            }

            int indexPos2 = ((r ^ g ^ b ^ a) & 63) << 2;
            index[indexPos2] = r;
            index[indexPos2 + 1] = g;
            index[indexPos2 + 2] = b;
            index[indexPos2 + 3] = a;
        }

        // Write RGBA
        img.pixels[rawPos+0] = r;
        img.pixels[rawPos+1] = g;
        img.pixels[rawPos+2] = b;
        img.pixels[rawPos+3] = a;
    }

    return img;
}