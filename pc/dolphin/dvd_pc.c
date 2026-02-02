/*
 * DVD (Filesystem) replacement - reads from extracted game files
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dolphin/types.h"
#include "dolphin/dvd.h"
#include "pc_config.h"

/* Simple FST for entrynum lookups */
#define MAX_FST_ENTRIES 4096
typedef struct {
    char path[256];
    s32 entrynum;
} FSTEntry;

static FSTEntry g_fst[MAX_FST_ENTRIES];
static int g_fst_count = 0;

static char g_data_path[512] = MP4_DATA_PATH;

static void build_path(char *out, size_t out_size, const char *game_path) {
    if (game_path[0] == '/') game_path++;
    snprintf(out, out_size, "%s%s", g_data_path, game_path);
}

void DVDInit(void) {
    printf("[DVD] DVDInit() - data path: %s\n", g_data_path);
    g_fst_count = 0;
}

BOOL DVDOpen(char *path, DVDFileInfo *fileInfo) {
    char full_path[512];
    build_path(full_path, sizeof(full_path), path);

    FILE *f = fopen(full_path, "rb");
    if (!f) {
        printf("[DVD] DVDOpen failed: %s\n", full_path);
        memset(fileInfo, 0, sizeof(*fileInfo));
        return FALSE;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    fclose(f);

    memset(fileInfo, 0, sizeof(*fileInfo));
    fileInfo->startAddr = 0;
    fileInfo->length = (u32)size;

    /* Store the full path in the command block area for later reads */
    strncpy((char *)&fileInfo->cb, full_path, sizeof(DVDCommandBlock));

    /* Register in FST */
    if (g_fst_count < MAX_FST_ENTRIES) {
        strncpy(g_fst[g_fst_count].path, path, 255);
        g_fst[g_fst_count].entrynum = g_fst_count;
        g_fst_count++;
    }

    return TRUE;
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo *fileInfo) {
    if (entrynum < 0 || entrynum >= g_fst_count) {
        return FALSE;
    }
    return DVDOpen(g_fst[entrynum].path, fileInfo);
}

BOOL DVDClose(DVDFileInfo *fileInfo) {
    (void)fileInfo;
    return TRUE;
}

BOOL DVDReadPrio(DVDFileInfo *fileInfo, void *addr, s32 length, s32 offset, s32 prio) {
    (void)prio;
    char *path = (char *)&fileInfo->cb;

    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("[DVD] DVDRead: failed to open %s\n", path);
        return FALSE;
    }

    fseek(f, offset, SEEK_SET);
    size_t bytes_read = fread(addr, 1, (size_t)length, f);
    fclose(f);
    return (bytes_read > 0);
}

/* DVDRead and DVDReadAsync are macros in dvd.h that call DVDReadPrio/DVDReadAsyncPrio */

BOOL DVDReadAsyncPrio(DVDFileInfo *fileInfo, void *addr, s32 length, s32 offset,
                      DVDCallback callback, s32 prio) {
    BOOL result = DVDReadPrio(fileInfo, addr, length, offset, prio);
    if (callback) {
        callback(result ? 0 : -1, fileInfo);
    }
    return result;
}

s32 DVDConvertPathToEntrynum(char *path) {
    for (int i = 0; i < g_fst_count; i++) {
        if (strcmp(g_fst[i].path, path) == 0) {
            return g_fst[i].entrynum;
        }
    }

    /* Try opening the file to register it */
    DVDFileInfo fi;
    if (DVDOpen(path, &fi)) {
        return g_fst_count - 1;
    }

    return -1;
}

s32 DVDCancel(DVDCommandBlock *block) { (void)block; return 0; }
BOOL DVDCancelAsync(DVDCommandBlock *block, DVDCBCallback callback) { (void)block; (void)callback; return TRUE; }
s32 DVDCancelAll(void) { return 0; }
BOOL DVDCancelAllAsync(DVDCBCallback callback) { (void)callback; return TRUE; }
s32 DVDGetCommandBlockStatus(const DVDCommandBlock *block) { (void)block; return DVD_STATE_END; }
s32 DVDGetDriveStatus(void) { return DVD_STATE_END; }
BOOL DVDSetAutoFatalMessaging(BOOL enable) { (void)enable; return TRUE; }
void DVDReset(void) {}
int DVDSetAutoInvalidation(int autoInval) { (void)autoInval; return 0; }

/* Stream stubs */
BOOL DVDPrepareStreamAsync(DVDFileInfo *fInfo, u32 length, u32 offset, DVDCallback callback) {
    (void)fInfo; (void)length; (void)offset; (void)callback; return TRUE;
}
s32 DVDPrepareStream(DVDFileInfo *fInfo, u32 length, u32 offset) {
    (void)fInfo; (void)length; (void)offset; return 0;
}
BOOL DVDCancelStreamAsync(DVDCommandBlock *block, DVDCBCallback callback) {
    (void)block; (void)callback; return TRUE;
}
s32 DVDCancelStream(DVDCommandBlock *block) { (void)block; return 0; }
BOOL DVDStopStreamAtEndAsync(DVDCommandBlock *block, DVDCBCallback callback) {
    (void)block; (void)callback; return TRUE;
}
s32 DVDStopStreamAtEnd(DVDCommandBlock *block) { (void)block; return 0; }
BOOL DVDGetStreamErrorStatusAsync(DVDCommandBlock *block, DVDCBCallback callback) {
    (void)block; (void)callback; return TRUE;
}
s32 DVDGetStreamErrorStatus(DVDCommandBlock *block) { (void)block; return 0; }
BOOL DVDGetStreamPlayAddrAsync(DVDCommandBlock *block, DVDCBCallback callback) {
    (void)block; (void)callback; return TRUE;
}
s32 DVDGetStreamPlayAddr(DVDCommandBlock *block) { (void)block; return 0; }
u32 DVDGetTransferredSize(DVDFileInfo *fInfo) { (void)fInfo; return 0; }

/* Dir stubs */
BOOL DVDOpenDir(const char *path, DVDDir *dir) { (void)path; (void)dir; return FALSE; }
BOOL DVDReadDir(DVDDir *dir, DVDDirEntry *entry) { (void)dir; (void)entry; return FALSE; }
BOOL DVDCloseDir(DVDDir *dir) { (void)dir; return TRUE; }

void DVDResume(void) {}
void DVDPause(void) {}
