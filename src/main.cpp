/*  This a simple C++14 program that uses raylib to load and display textures and sprites
    from a GameMaker Studio 2 data/game.win (unx, ios, droid, etc.) file. It includes
    optimized scanning for texture formats within the TXTR chunk, and supports loading 
    individual sprites by their index.

    It's being optimized as most as possible to reduce memory usage and improve performance
    when dealing with large game data files. It will be used for a future GameMaker Studio 2
    Raylib Recreation Engine, aimed for older computers and consoles like the SEGA Dreamcast,
    GameCube / Wii, Nintendo 3DS, PlayStation 2, PlayStation Portable, etc.

    The main purpose of the engine is to port Feltree Forestk's "SIN_GAL: Pre-Lucid" Project,
    mostly for lore reasons. Main piority is getting this to run on Dreamcast with good performance,
    low memory usage and high compatibility with most (if not all) games, all under 16 MB of RAM.

    For this program to compile correctly, install these libraries:
        - raylib (https://www.raylib.com/)
        - bzip2 (https://sourceware.org/bzip2/):
        On Windows with MINGW64 (in MSYS2), you can install them via:
        pacman -S mingw-w64-x86_64-bzip2
        (Already included with KOS-Ports)

        - zlib (https://zlib.net/): pacman -S mingw-w64-x86_64-zlib
        (Already included with KOS-Ports)
    
        - libpng (https://libpng.sourceforge.io/): pacman -S mingw-w64-x86_64-libpng
        (Already included with KOS-Ports)
    Compile with C++14 standard.
*/

#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "sprNodes.h"
#ifdef _WIN32
    #include <raylib_win32.h>
#endif
#include <bzip2-1.0.8/bzlib.h>

FILE* currentFile = NULL;

size_t ramMetaUsage = 0;
size_t ramTexUsage = 0;

struct TexturePage texturePages[256];
int textureCount = 0;

//############### TEXTURE PAGES ONLY

Image LoadGMImage(FILE* file, unsigned char* bytes, size_t bytes_size, size_t txtSize, int gm2022_5) {
    if (bytes_size < 8) {
        fprintf(stderr, "Error: Too short for GMImage\n");
        exit(1);
    }

    // PNG
    if (bytes[0]==0x89 && bytes[1]==0x50 && bytes[2]==0x4E && bytes[3]==0x47) {
        return LoadImageFromMemory(".png", bytes, bytes_size);
    }

    // QOI
    if (bytes[0]=='f' && bytes[1]=='i' && bytes[2]=='o' && bytes[3]=='q') {
        struct QoiImage qoi = DecodeQoi(bytes, bytes_size);
        void* pixels = malloc(qoi.pixels_size);
        memcpy(pixels, qoi.pixels, qoi.pixels_size);
        free(qoi.pixels);
        Image img;
        img.data = pixels;
        img.width = qoi.width;
        img.height = qoi.height;
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        return img;
    }

    // BZ2+QOI
    size_t offset = (size_t)-1;
    for (size_t i = 0; i + 3 < bytes_size; i++) {
        if (bytes[i] == '2' && bytes[i+1] == 'z' && bytes[i+2] == 'o' && bytes[i+3] == 'q') {
            offset = i;
            break;
        }
    }

    if (offset != (size_t)-1) {
        /*int width  = bytes[offset+4] | (bytes[offset+5]<<8);
        int height = bytes[offset+6] | (bytes[offset+7]<<8);*/
        int uncompressedLen = -1;
        if (gm2022_5 && bytes_size >= 12) {
            memcpy(&uncompressedLen, &bytes[8], sizeof(int));
        }

        // 🔍 Scan for "BZh" after offset
        size_t bzipStart = (size_t)-1;
        for (size_t i = offset; i + 2 < bytes_size; i++) {
            if (bytes[i] == 'B' && bytes[i+1] == 'Z' && bytes[i+2] == 'h') {
                bzipStart = i;
                break;
            }
        }
        if (bzipStart == (size_t)-1) {
            fprintf(stderr, "Error: BZh header not found\n");
            exit(1);
        }

        // 🚀 Use TXTR chunk size to define the end
        size_t endOfStream = bytes_size;

        size_t compressed_size = endOfStream - bzipStart;
        unsigned char* compressed = (unsigned char*)malloc(compressed_size);
        if (!compressed) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            exit(1);
        }
        memcpy(compressed, bytes + bzipStart, compressed_size);

        unsigned int destLen = compressed_size * 10;
        unsigned char* output = NULL;
        int result = BZ_OUTBUFF_FULL;

        for (int attempt = 0; attempt < 5 && result == BZ_OUTBUFF_FULL; ++attempt) {
            if (output) free(output);
            output = (unsigned char*)malloc(destLen);
            if (!output) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                free(compressed);
                exit(1);
            }
            result = BZ2_bzBuffToBuffDecompress(
                (char*)output, &destLen,
                (char*)compressed, compressed_size,
                0, 0
            );
            if (result == BZ_OUTBUFF_FULL) {
                destLen *= 3; // triple the buffer and try again
            }
        }

        free(compressed);

        if (result != BZ_OK) {
            char errorMsg[256];
            sprintf(errorMsg, "BZ2 decompression failed: ");
            switch (result) {
                case BZ_OUTBUFF_FULL: strcat(errorMsg, "Output buffer too small"); break;
                case BZ_DATA_ERROR: strcat(errorMsg, "Data integrity error"); break;
                case BZ_DATA_ERROR_MAGIC: strcat(errorMsg, "Invalid BZip2 header"); break;
                case BZ_PARAM_ERROR: strcat(errorMsg, "Bad parameter"); break;
                case BZ_MEM_ERROR: strcat(errorMsg, "Memory allocation failed"); break;
                case BZ_UNEXPECTED_EOF: strcat(errorMsg, "Unexpected end of file"); break;
                default: sprintf(errorMsg + strlen(errorMsg), "Unknown error code %d", result); break;
            }
            fprintf(stderr, "Error: %s\n", errorMsg);
            free(output);
            exit(1);
        }

        struct QoiImage qoi = DecodeQoi(output, destLen);
        free(output);
        
        void* pixels = malloc(qoi.pixels_size);
        if (!pixels) {
            fprintf(stderr, "Error: QOI decode failed\n");
            free(qoi.pixels);
            exit(1);
        }

        memcpy(pixels, qoi.pixels, qoi.pixels_size);
        free(qoi.pixels);
        
        Image img;
        img.data = pixels;
        img.width = qoi.width;
        img.height = qoi.height;
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        return img;
    }

    fprintf(stderr, "Error: Unknown GMImage format\n");
    exit(1);
}

Image LoadTextureByOffset(FILE* file, int id) {
    if (id < 0 || id >= textureCount) {
        fprintf(stderr, "Error: Invalid texture ID\n");
        exit(1);
    }

    size_t offset = texturePages[id].offset;
    size_t end    = texturePages[id].endOffset;
    size_t size   = end - offset;

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    if (offset >= fileSize) {
        fprintf(stderr, "Error: Offset beyond file size\n");
        exit(1);
    }
    if (end > fileSize) end = fileSize;
    size = end - offset;

    unsigned char* buffer = (unsigned char*)malloc(size);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }
    fseek(file, offset, SEEK_SET);
    fread(buffer, 1, size, file);

    Image img = LoadGMImage(file, buffer, size, size, 1);
    free(buffer);
    return img;
}

//##################################

// --- PNG streaming (simplest: still needs full buffer for raylib) ---
void LoadPNGStream(FILE* f, size_t offset, size_t size, Image &img) {
    fseek(f, offset, SEEK_SET);
    //unsigned char* buf = (unsigned char*)malloc(size);
    unsigned char* buf = (unsigned char*)AllocateTex(size);
    if (!buf) { fprintf(stderr, "malloc failed\n"); exit(1); }
    fread(buf, 1, size, f);
    /*Image */img = LoadImageFromMemory(".png", buf, size);
    //free(buf);
    FreeTex(buf, size);
}

// Clear texturePages when rescanning
void ResetTexturePages() {
    for (int i = 0; i < 256; i++) {
        texturePages[i].id = -1;
        texturePages[i].offset = 0;
        texturePages[i].endOffset = 0;
    }
    textureCount = 0;
}

// BZip2 + QOI streamed decode into canvas
void LoadBZ2QOIStreamForQoiPos(
    FILE* f,
    size_t offset,
    int srcX, int srcY,
    int srcW, int srcH,
    int dstX, int dstY,
    int potW, int potH,
    SpriteSt& txtPage
) {
    fseek(f, offset, SEEK_SET);

    int bzerr;
    BZFILE* bzf = BZ2_bzReadOpen(&bzerr, f, 0, 0, NULL, 0);
    if (bzerr != BZ_OK) { fprintf(stderr, "bzReadOpen error\n"); exit(1); }

    // decompress into buffer
    size_t cap = 1024*1024;
    uint8_t* out = (uint8_t*)AllocateTex(cap);
    size_t total = 0;

    while (1) {
        if (total + 4096 > cap) {
            cap *= 2;
            out = ReallocateTex(out, cap/2, cap);
            if (!out) { fprintf(stderr, "realloc failed\n"); exit(1); }
        }
        int n = BZ2_bzRead(&bzerr, bzf, out + total, 4096);
        if (bzerr == BZ_OK || bzerr == BZ_STREAM_END) {
            total += n;
            if (bzerr == BZ_STREAM_END) break;
        } else {
            fprintf(stderr, "bzRead error %d\n", bzerr);
            FreeTex(out, cap);
            BZ2_bzReadClose(&bzerr, bzf);
            exit(1);
        }
    }
    BZ2_bzReadClose(&bzerr, bzf);

    //Parse QOI header to get width/height
    if (total < 12 || memcmp(out,"fioq",4)!=0) 
    { 
        fprintf(stderr,"Error: invalid QOI stream\n"); 
        FreeTex(out,cap); return; 
    } 
    int width = out[4] | (out[5]<<8);
    int height = out[6] | (out[7]<<8);
    int length = out[8] | (out[9]<<8) | (out[10]<<16) | (out[11]<<24);
    if ((12ull + length) > total) { fprintf(stderr, "Error: Truncated QOI\n"); exit(1); }

    const uint8_t* pixelData = out + 12;

    //seek to starting row
    txtPage.qoi = QoiFastSeek(pixelData, length, width, srcY);
    
    FreeTex(out, cap);
    printf("QOI cursor pos gained\n");
}

// BZip2 + QOI streamed decode into canvas
void LoadBZ2QOIStreamToCanvas(
    FILE* f,
    size_t offset,
    int srcX, int srcY,
    int srcW, int srcH,
    int dstX, int dstY,
    int potW, int potH,
    uint8_t* canvas,
    SpriteSt* txtPage
) {
    fseek(f, offset, SEEK_SET);

    int bzerr;
    BZFILE* bzf = BZ2_bzReadOpen(&bzerr, f, 0, 0, NULL, 0);
    if (bzerr != BZ_OK) { fprintf(stderr, "bzReadOpen error\n"); exit(1); }

    // decompress into buffer
    size_t cap = 1024*1024;
    uint8_t* out = (uint8_t*)AllocateTex(cap);
    size_t total = 0;

    while (1) {
        if (total + 4096 > cap) {
            cap *= 2;
            out = ReallocateTex(out, cap/2, cap);
            if (!out) { fprintf(stderr, "realloc failed\n"); exit(1); }
        }
        int n = BZ2_bzRead(&bzerr, bzf, out + total, 4096);
        if (bzerr == BZ_OK || bzerr == BZ_STREAM_END) {
            total += n;
            if (bzerr == BZ_STREAM_END) break;
        } else {
            fprintf(stderr, "bzRead error %d\n", bzerr);
            FreeTex(out, cap);
            BZ2_bzReadClose(&bzerr, bzf);
            exit(1);
        }
    }
    BZ2_bzReadClose(&bzerr, bzf);

    // Parse QOI header to get width/height
    if (total < 12 || memcmp(out,"fioq",4)!=0) 
    { 
        fprintf(stderr,"Error: invalid QOI stream\n"); 
        FreeTex(out,cap); return; 
    }

    // prepare row writer
    SpriteRowWriter writer = {
        .srcX = srcX, .srcY = srcY, .srcW = srcW, .srcH = srcH,
        .dstX = dstX, .dstY = dstY,
        .potW = potW, .potH = potH,
        .canvas = canvas,
        .done = 0,
        .direct = 0
    };

    // decode QOI directly into canvas row by row
    DecodeQoiToSpriteFast(out, total, &writer, txtPage->qoi, QoiWriteRowToSprite);

    FreeTex(out, cap);
}

long currentFileSize = 0;

// Load a specific frame from a sprite by sprite index and frame index
void LoadSpriteFrameByIndex(FILE* file, int spriteIndex, int frameIndex, Image& imgOut) {
    if (spriteIndex < 0 || spriteIndex >= (int)sprtCount) {
        fprintf(stderr, "Error: Invalid sprite index %d\n", spriteIndex);
        exit(1);
    }

    if (spriteIndex != currentSprite.sprite.id) {
        parseSPRT_single(file, spriteIndex, SPRT_SAME);
    }

    SpriteSt* spr = &currentSprite.sprite;
    if (frameIndex < 0 || frameIndex >= spr->textureCount) {
        fprintf(stderr, "Error: Invalid frame index %d\n", frameIndex);
        exit(1);
    }

    uint32_t tpagAddr = spr->tpagAddrs[frameIndex];
    if (tpagAddr == 0 || (int)tpagAddr + 22 > currentFileSize) { 
        fprintf(stderr, "Error: TPAG address out of bounds (0x%08X)\n", tpagAddr); exit(1); 
    }

    if (spr->qoiCursors && frameIndex < spr->textureCount)
        spr->qoi = spr->qoiCursors[frameIndex];

    bool streamed = false;

    fseek(file, tpagAddr, SEEK_SET);
    uint16_t sourceX = readU16(file);
    uint16_t sourceY = readU16(file);
    uint16_t sourceW = readU16(file);
    uint16_t sourceH = readU16(file);
    uint16_t targetX = readU16(file);
    uint16_t targetY = readU16(file);
    uint16_t targetW = readU16(file);
    uint16_t targetH = readU16(file);
    uint16_t boundW  = readU16(file);
    uint16_t boundH  = readU16(file);
    int16_t  texPage = readI16(file);

    // Print TPAG info for debugging
    /*printf("TPAG for sprite %d frame %d: texPage=%d sourceX=%d sourceY=%d sourceW=%d sourceH=%d targetX=%d targetY=%d targetW=%d targetH=%d boundW=%d boundH=%d\n",
        spriteIndex, frameIndex, texPage, sourceX, sourceY, sourceW, sourceH, targetX, targetY, targetW, targetH, boundW, boundH);*/

    int originalW = spr->width;
    int originalH = spr->height;
    int potW = originalW;
    int potH = originalH;


    uint8_t* canvas = (uint8_t*)CallocTex(potW * potH, 4);
    if (!canvas) {
        fprintf(stderr, "Error: Canvas allocation failed\n");
        exit(1);
    }

    if (texPage >= 0 && texPage < textureCount) {
        size_t offset = texturePages[texPage].offset;
        size_t end    = texturePages[texPage].endOffset;
        size_t size   = end - offset;

        fseek(file, 0, SEEK_END);
        size_t fileSize = ftell(file);
        if (offset >= fileSize) {
            fprintf(stderr, "Error: Offset beyond file size\n");
            exit(1);
        }
        if (end > fileSize) end = fileSize;
        size = end - offset;

        //STREAMING LOAD
        fseek(file, offset, SEEK_SET);
        unsigned char header[4];
        fread(header, 1, 4, file);
        fseek(file, offset, SEEK_SET);

        //Image fullImg;
        if (header[0]==0x89 && header[1]==0x50 && header[2]==0x4E && header[3]==0x47) {
            // PNG
            /*imgOut = */LoadPNGStream(file, offset, size, imgOut);
        } else if (header[0]=='f' && header[1]=='i' && header[2]=='o' && header[3]=='q') {
            // QOI (still needs buffer because DecodeQoi expects memory)
            //unsigned char* buf = (unsigned char*)malloc(size);
            unsigned char* buf = (unsigned char*)AllocateTex(size);
            fread(buf, 1, size, file);
            struct QoiImage qoi = DecodeQoi(buf, size);
            //free(buf);
            FreeTex(buf, size);
            imgOut.data = qoi.pixels;
            imgOut.width = qoi.width;
            imgOut.height = qoi.height;
            imgOut.mipmaps = 1;
            imgOut.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        } else {
            streamed = true;
            /*LoadBZ2QOIStreamForQoiPos(
                file, offset, sourceX, sourceY, sourceW, sourceH, targetX, targetY, potW, potH, *spr
            );*/
            LoadBZ2QOIStreamToCanvas( 
                file, offset, sourceX, sourceY, sourceW, sourceH, targetX, targetY, potW, potH, canvas, spr );
        }

        if (!streamed) {
            // Crop region
            //uint8_t* croppedData = (uint8_t*)malloc(sourceW * sourceH * 4);
            uint8_t* croppedData = (uint8_t*)AllocateTex(sourceW * sourceH * 4);
            for (int y = 0; y < sourceH; ++y) {
                for (int x = 0; x < sourceW; ++x) {
                    int srcIdx = ((sourceY + y) * imgOut.width + (sourceX + x)) * 4;
                    int dstIdx = (y * sourceW + x) * 4;
                    memcpy(&croppedData[dstIdx], &((uint8_t*)imgOut.data)[srcIdx], 4);
                }
            }

            // Logical canvas dimensions
            const int logicalW = (boundW > 0) ? boundW : spr->width;
            const int logicalH = (boundH > 0) ? boundH : spr->height;

            for (int yy = 0; yy < sourceH; ++yy) {
                int dstRow = targetY + yy;
                if (dstRow < 0 || dstRow >= logicalH) continue;

                int copyW = sourceW;
                int srcStart = 0;
                int dstStart = targetX;

                if (dstStart < 0) { srcStart += -dstStart; copyW -= -dstStart; dstStart = 0; }
                if (dstStart + copyW > logicalW) { copyW = logicalW - dstStart; }
                if (copyW <= 0) continue;

                const uint8_t* src = croppedData + (yy * sourceW + srcStart) * 4;
                uint8_t* dst = canvas + (dstRow * potW + dstStart) * 4;

                memcpy(dst, src, copyW * 4);
            }

            SafeUnloadImage(imgOut);
        }
        
    } else {
        fprintf(stderr, "Error: Invalid texture ID\n");
        exit(1);
    }
    
    imgOut.data = canvas;
    imgOut.width = potW;
    imgOut.height = potH;
    imgOut.mipmaps = 1;
    imgOut.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    
    #ifdef __DC__
        ImageToPOT(&imgOut, (Color){0, 0, 0, 0});
    #endif
}


const float leftStickDeadzoneX = 0.3f;
const float leftStickDeadzoneY = 0.3f;

bool spriteMode = true;
int spriteId = 1;
int currentFrame = 0;
float frameTimer = 0.0f;
float frameSpeed = 0.1f;  // Time per frame in seconds

int cnt = 0;
int id = 0;
int x = 60;
int y = 60;

SpriteFrameCache spriteCache = { NULL, 0, -1 };

void FreeSpriteFrames(void) {
    if (spriteCache.frames) {
        for (int i = 0; i < spriteCache.frameCount; ++i) {
            if (spriteCache.frames[i].id != 0) {
                SafeUnloadTexture(spriteCache.frames[i]);
            }
        }
        FreeTex(spriteCache.frames, sizeof(Texture2D) * spriteCache.frameCount);
        spriteCache.frames = NULL;
    }
    spriteCache.frameCount = 0;
    spriteCache.spriteId = -1;
}

// Load every frame for a sprite (decode once, create textures, free images)
void LoadSpriteFramesForSprite(FILE* file, int spriteIndex) {
    if (spriteCache.spriteId == spriteIndex && spriteCache.frames) return;

    printf("Loading frames for sprite %d\n", spriteIndex);

    FreeSpriteFrames();

    SpriteSt* spr = &currentSprite.sprite;
    int frames = spr->textureCount;
    if (frames <= 0) {
        // create single blank frame
        spriteCache.frames = (Texture2D*)AllocateTex(sizeof(Texture2D));
        Image blank = GenImageColor(1, 1, (Color){0,0,0,0});
        spriteCache.frames[0] = LoadTextureFromImage(blank);
        SafeUnloadImage(blank);
        spriteCache.frameCount = 1;
        spriteCache.spriteId = spriteIndex;
        return;
    }

    // allocate array
    spriteCache.frames = (Texture2D*)AllocateTex(sizeof(Texture2D) * frames);
    if (!spriteCache.frames) {
        fprintf(stderr, "Error: Could not allocate frame textures array\n");
        spriteCache.frameCount = 0;
        spriteCache.spriteId = -1;
        return;
    }

    Image img;

    // load each frame
    for (int f = 0; f < frames; ++f) {
        LoadSpriteFrameByIndex(file, spriteIndex, f, img);
        Texture2D t = LoadTextureFromImage(img);
        SafeUnloadImage(img);
        spriteCache.frames[f] = t;
    }

    spriteCache.frameCount = frames;
    spriteCache.spriteId = spriteIndex;
}

Texture2D tex;
bool CDMode = false;
DataWinIndex indexDW;
int currentSpriteIndex = 0;
SprNode currentSprite = { {0}, {0} };

// Load new data.win (close previous one)
int LoadDataWin(const char* filename) {
    if (currentFile) {
        freeSpriteList();
        FreeSpriteFrames();
        ResetTexturePages();
        fclose(currentFile);
        currentFileSize = 0;
    }

    currentFile = fopen(filename, "rb");
    if (!currentFile) {
        fprintf(stderr, "Error: cannot open %s\n", filename);
        return -1;
    }

    fseek(currentFile, 0, SEEK_END);
    long size = ftell(currentFile);
    fseek(currentFile, 0, SEEK_SET);

    memset(&indexDW, 0, sizeof(indexDW));
    listChunksIndex(currentFile, size, 0, &indexDW);

    currentFileSize = size;
    currentSpriteIndex = spriteId;

    scanSPRT(currentFile, indexDW.sprtOffset, indexDW.sprtSize);
    parseSPRT_single(currentFile, currentSpriteIndex, SPRT_SAME);
    parseTXTR(currentFile, indexDW.txtrOffset, indexDW.txtrSize);
    InitAllSpriteQoiCursors(currentFile, indexDW.sprtOffset, indexDW.sprtSize);

    sprCounter = (int)sprtCount;

    return 0;
}

int main(int argc, char* argv[]) {

    InitWindow(640, 480, "Game Maker Image Loader");

    SetTargetFPS(60);

    #ifdef __DC__
        LoadDataWin("/cd/data.win");
    #else
        LoadDataWin("data.win");
    #endif
    
    // Load initial sprite frame
    LoadSpriteFramesForSprite(currentFile, spriteId);
    if (spriteCache.frames && spriteCache.frameCount > 0) {
        // set initial texture to first frame
        if (tex.id != 0) { SafeUnloadTexture(tex); }
        tex = spriteCache.frames[currentFrame];
    }

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GREEN);

        // Switch data.win
        if (IsKeyPressed(KEY_SPACE) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
            
            if(CDMode == false){
                #ifdef __DC__
                    LoadDataWin("/cd/data.win");
                #else
                    LoadDataWin("dataSG.win");
                #endif
                CDMode = true;
            } else {
                #ifdef __DC__
                    LoadDataWin("/rd/data.win");
                #else
                    LoadDataWin("data.win");
                #endif
                CDMode = false;
            }
            
            spriteMode = true;
            spriteId = 0;
            currentFrame = 0;
            frameTimer = 0.0f;
            
            // Load initial sprite frame after switch
            LoadSpriteFramesForSprite(currentFile, spriteId);
            if (spriteCache.frames && spriteCache.frameCount > 0) {
                if (tex.id != 0) { SafeUnloadTexture(tex); }
                tex = spriteCache.frames[currentFrame];
            }
        }

        // Read gamepad axes
        float leftStickX = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float leftStickY = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);

        // Deadzone
        if (fabsf(leftStickX) < leftStickDeadzoneX) leftStickX = 0.0f;
        if (fabsf(leftStickY) < leftStickDeadzoneY) leftStickY = 0.0f;

        // Movement speed
        float moveSpeed = 10.0f;

        // Keyboard
        if (IsKeyDown(KEY_W)) y += moveSpeed;
        if (IsKeyDown(KEY_S)) y -= moveSpeed;
        if (IsKeyDown(KEY_A)) x += moveSpeed;
        if (IsKeyDown(KEY_D)) x -= moveSpeed;

        // Gamepad
        x += -leftStickX * moveSpeed;
        y += -leftStickY * moveSpeed;

        if (IsKeyPressed(KEY_TAB) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) {
            spriteMode = !spriteMode;
            currentFrame = 0;
            frameTimer = 0.0f;
            if (spriteMode && spriteId < sprCounter) {
                LoadSpriteFramesForSprite(currentFile, spriteId);
                if (spriteCache.frames && spriteCache.frameCount > 0) {
                    tex = spriteCache.frames[currentFrame];
                }
            }
        }

        if (spriteMode) {
            if (IsKeyPressed(KEY_LEFT) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) {
                spriteId--;
                if (spriteId < 0) spriteId = sprCounter - 1;
                currentFrame = 0; frameTimer = 0.0f;

                currentSpriteIndex = parseSPRT_single(currentFile, currentSpriteIndex, SPRT_PREV);
                
                LoadSpriteFramesForSprite(currentFile, spriteId);
                if (spriteCache.frames && spriteCache.frameCount > 0) {
                    tex = spriteCache.frames[currentFrame];
                }
            }
            if (IsKeyPressed(KEY_RIGHT) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) {
                spriteId++;
                if (spriteId >= sprCounter) spriteId = 0;
                currentFrame = 0; frameTimer = 0.0f;

                currentSpriteIndex = parseSPRT_single(currentFile, currentSpriteIndex, SPRT_NEXT);

                LoadSpriteFramesForSprite(currentFile, spriteId);
                if (spriteCache.frames && spriteCache.frameCount > 0) {
                    tex = spriteCache.frames[currentFrame];
                }
            }

            // Animation frame update (load one frame at a time)
            int frameCount = currentSprite.sprite.textureCount;
            if (frameCount > 0) {
                frameTimer += GetFrameTime();
                if (frameTimer >= frameSpeed) {
                    frameTimer -= frameSpeed;
                    currentFrame++;
                    if (currentFrame >= frameCount) {
                        currentFrame = 0;  // Loop animation
                    }
                        
                    // Load the new frame, fast swap to cached texture
                    if (spriteCache.frames && spriteCache.frameCount > 0) {
                        tex = spriteCache.frames[currentFrame];
                    }
                }
            }
        } else {
            // Texture mode controls
            if (IsKeyPressed(KEY_LEFT) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) {
                id--;
                if (id < 0) id = textureCount - 1;
                UnloadTexture(tex);
                Image img = LoadTextureByOffset(currentFile, id);
                tex = LoadTextureFromImage(img);
                ramTexUsage += img.width * img.height * 4; // approximate VRAM
                UnloadImage(img);
            }
            if (IsKeyPressed(KEY_RIGHT) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) {
                id++;
                if (id >= textureCount) id = 0;
                UnloadTexture(tex);
                Image img = LoadTextureByOffset(currentFile, id);
                tex = LoadTextureFromImage(img);
                ramTexUsage += img.width * img.height * 4; // approximate VRAM
                UnloadImage(img);
            }
        }

        DrawTexture(tex, x, y, WHITE);

        DrawFPS(40, 40);
        if (spriteMode) {
            SprNode* cur = spriteList;
            for (int i = 0; i < spriteId && cur; ++i) cur = cur->next;
            int frameCount = (cur) ? cur->sprite.textureCount : 0;
            
            DrawText(TextFormat("Sprite: %d | Frame: %d/%d", currentSpriteIndex, currentFrame, frameCount), 
                     100, 450, 20, RED);
        } else {
            DrawText(TextFormat("Texture ID: %i", id), 100, 450, 20, RED);
        }
        
        DrawText("TAB / UP: Toggle Mode", 100, 400, 20, BLUE);

        EndDrawing();
    }

    FreeSpriteFrames();
    SafeUnloadTexture(tex);
    if (currentFile) fclose(currentFile);
    CloseWindow();
    return 0;
}