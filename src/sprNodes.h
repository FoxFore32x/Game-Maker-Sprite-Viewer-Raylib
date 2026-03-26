#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <raylib.h>
#include "qoi_stream.h"

// RAM usage counters (bytes)
extern size_t ramMetaUsage;
extern size_t ramTexUsage;

#define DREAMCAST_MAIN_RAM (16 * 1024 * 1024)   // 16 MB
#define DREAMCAST_VRAM     (8 * 1024 * 1024)    // 8 MB

void* AllocateMeta(size_t size);

void FreeMeta(void* p, size_t size);

void* AllocateTex(size_t size);

void FreeTex(void* p, size_t size);

unsigned char* ReallocateTex(unsigned char* p, size_t oldSize, size_t newSize);

void* CallocTex(size_t count, size_t size);

void SafeUnloadImage(Image img);

void SafeUnloadTexture(Texture2D tex);

void DrawRamBar(int x, int y, int width, int height);

struct sprTexPage{
    int id;
    int pageItem;
    int sourceX;
    int sourceY;
    int sourceW;
    int sourceH;
    int targetX;
    int targetY;
    int targetW;
    int targetH;
    int boundW;
    int boundH;
};

struct texNode{
    sprTexPage texture;
    texNode* next;
};

struct SpriteSt{
    int id;
    char* name;
    int width;
    int height;
    int marginLeft;
    int marginRight;
    int marginBottom;
    int marginTop;
    bool transparent;
    bool smooth;
    bool preload;
    int bboxMode;
    int sepMasks;
    int originX;
    int originY;
    bool specialType;
    int sVersion;
    int spriteType;
    float playbackSpeed;
    int playbackSpeedType;
    int sequenceOffset;
    int nineSliceOffset;
    texNode* textures;
    int textureCount = 0;
    QoiCursor qoi;
    QoiCursor* qoiCursors = NULL;

    uint32_t* tpagAddrs = NULL;
};

struct SprNode{
    SpriteSt sprite;
    SprNode* next;
};

extern int sprCounter;
extern SprNode* spriteList;
extern SprNode currentSprite;

struct TexturePage {
    int id;
    size_t offset;
    size_t size;
    size_t endOffset;
};

extern struct TexturePage texturePages[256];
extern int textureCount;

// Sprite frame cache (load all frames once per sprite)
struct SpriteFrameCache {
    Texture2D *frames;   // dynamically allocated array of textures
    int frameCount;      // number of frames allocated
    int spriteId;        // which sprite is cached (-1 = none)
    int chunkStart;      // index of first frame in current chunk
    int chunkSize;       // how many frames per chunk (e.g. 4)
};

void listChunks(FILE* file, long endPos, int depth);

void freeSpriteList(void);

int match(const unsigned char* buf, const char* sig, size_t len);

int isTextureSignature(const unsigned char* p);

uint16_t readU16(FILE* file);
int16_t readI16(FILE* file);

struct DataWinIndex {
    long sprtOffset;
    uint32_t sprtSize;
    long txtrOffset;
    uint32_t txtrSize;
    long roomOffset;
    uint32_t roomSize;
    long tpagOffset;
    uint32_t tpagSize;
};

extern DataWinIndex indexDW;

void listChunksIndex(FILE* file, long endPos, int depth, DataWinIndex* index);
void parseSPRT(FILE* file, long chunkStart, uint32_t chunkSize);
void parseTXTR(FILE* file, long chunkStart, uint32_t chunkSize);

void indexSPRT(FILE* file, long chunkStart, uint32_t chunkSize);

typedef struct {
    uint32_t offset;
    uint32_t size;
} SpriteOffset;

#define SPRT_PREV -1
#define SPRT_NEXT  1
#define SPRT_SAME  0

void scanSPRT(FILE* f, uint32_t sprtChunkOffset, uint32_t sprtChunkSize);
int parseSPRT_single(FILE* file, int currentIndex, int direction);
void clearSprites();
extern uint32_t sprtCount;

void LoadBZ2QOIStreamForQoiPos(
    FILE* f,
    size_t offset,
    int srcX, int srcY,
    int srcW, int srcH,
    int dstX, int dstY,
    int potW, int potH,
    SpriteSt& txtPage
);

int NextPOT2(int size);

void InitAllSpriteQoiCursors(FILE *f, long sprtChunkOffset, uint32_t sprtChunkSize);