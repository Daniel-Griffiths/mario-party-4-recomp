/*
 * Memory Card stubs - all return "no card" / error
 */
#include <stdio.h>
#include "dolphin/card.h"

void CARDInit(void) {
    printf("[CARD] CARDInit() - memory card stubbed\n");
}

BOOL CARDGetFastMode(void) { return FALSE; }
BOOL CARDSetFastMode(BOOL enable) { (void)enable; return FALSE; }

s32 CARDCheck(s32 chan) { (void)chan; return CARD_RESULT_NOCARD; }
s32 CARDCheckAsync(s32 chan, CARDCallback callback) { (void)chan; (void)callback; return CARD_RESULT_NOCARD; }
s32 CARDCheckEx(s32 chan, s32 *xferBytes) { (void)chan; (void)xferBytes; return CARD_RESULT_NOCARD; }
s32 CARDCheckExAsync(s32 chan, s32 *xferBytes, CARDCallback callback) { (void)chan; (void)xferBytes; (void)callback; return CARD_RESULT_NOCARD; }

s32 CARDCreate(s32 chan, const char *fileName, u32 size, CARDFileInfo *fileInfo) {
    (void)chan; (void)fileName; (void)size; (void)fileInfo; return CARD_RESULT_NOCARD;
}
s32 CARDCreateAsync(s32 chan, const char *fileName, u32 size, CARDFileInfo *fileInfo, CARDCallback callback) {
    (void)chan; (void)fileName; (void)size; (void)fileInfo; (void)callback; return CARD_RESULT_NOCARD;
}

s32 CARDDelete(s32 chan, const char *fileName) { (void)chan; (void)fileName; return CARD_RESULT_NOCARD; }
s32 CARDDeleteAsync(s32 chan, const char *fileName, CARDCallback callback) { (void)chan; (void)fileName; (void)callback; return CARD_RESULT_NOCARD; }
s32 CARDFastDelete(s32 chan, s32 fileNo) { (void)chan; (void)fileNo; return CARD_RESULT_NOCARD; }
s32 CARDFastDeleteAsync(s32 chan, s32 fileNo, CARDCallback callback) { (void)chan; (void)fileNo; (void)callback; return CARD_RESULT_NOCARD; }
s32 CARDFastOpen(s32 chan, s32 fileNo, CARDFileInfo *fileInfo) { (void)chan; (void)fileNo; (void)fileInfo; return CARD_RESULT_NOCARD; }

s32 CARDFormat(s32 chan) { (void)chan; return CARD_RESULT_NOCARD; }
s32 CARDFormatAsync(s32 chan, CARDCallback callback) { (void)chan; (void)callback; return CARD_RESULT_NOCARD; }
s32 CARDFreeBlocks(s32 chan, s32 *byteNotUsed, s32 *filesNotUsed) { (void)chan; (void)byteNotUsed; (void)filesNotUsed; return CARD_RESULT_NOCARD; }

s32 CARDGetAttributes(s32 chan, s32 fileNo, u8 *attr) { (void)chan; (void)fileNo; (void)attr; return CARD_RESULT_NOCARD; }
s32 CARDGetEncoding(s32 chan, u16 *encode) { (void)chan; (void)encode; return CARD_RESULT_NOCARD; }
s32 CARDGetMemSize(s32 chan, u16 *size) { (void)chan; (void)size; return CARD_RESULT_NOCARD; }
s32 CARDGetResultCode(s32 chan) { (void)chan; return CARD_RESULT_NOCARD; }
s32 CARDGetSectorSize(s32 chan, u32 *size) { (void)chan; (void)size; return CARD_RESULT_NOCARD; }
s32 CARDGetSerialNo(s32 chan, u64 *serialNo) { (void)chan; (void)serialNo; return CARD_RESULT_NOCARD; }
s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat *stat) { (void)chan; (void)fileNo; (void)stat; return CARD_RESULT_NOCARD; }
s32 CARDGetXferredBytes(s32 chan) { (void)chan; return 0; }

s32 CARDMount(s32 chan, void *workArea, CARDCallback detachCallback) { (void)chan; (void)workArea; (void)detachCallback; return CARD_RESULT_NOCARD; }
s32 CARDMountAsync(s32 chan, void *workArea, CARDCallback detachCallback, CARDCallback attachCallback) {
    (void)chan; (void)workArea; (void)detachCallback; (void)attachCallback; return CARD_RESULT_NOCARD;
}

s32 CARDOpen(s32 chan, const char *fileName, CARDFileInfo *fileInfo) { (void)chan; (void)fileName; (void)fileInfo; return CARD_RESULT_NOCARD; }
BOOL CARDProbe(s32 chan) { (void)chan; return FALSE; }
s32 CARDProbeEx(s32 chan, s32 *memSize, s32 *sectorSize) { (void)chan; (void)memSize; (void)sectorSize; return CARD_RESULT_NOCARD; }

s32 CARDRename(s32 chan, const char *oldName, const char *newName) { (void)chan; (void)oldName; (void)newName; return CARD_RESULT_NOCARD; }
s32 CARDRenameAsync(s32 chan, const char *oldName, const char *newName, CARDCallback callback) { (void)chan; (void)oldName; (void)newName; (void)callback; return CARD_RESULT_NOCARD; }

s32 CARDSetAttributesAsync(s32 chan, s32 fileNo, u8 attr, CARDCallback callback) { (void)chan; (void)fileNo; (void)attr; (void)callback; return CARD_RESULT_NOCARD; }
s32 CARDSetAttributes(s32 chan, s32 fileNo, u8 attr) { (void)chan; (void)fileNo; (void)attr; return CARD_RESULT_NOCARD; }
s32 CARDSetStatus(s32 chan, s32 fileNo, CARDStat *stat) { (void)chan; (void)fileNo; (void)stat; return CARD_RESULT_NOCARD; }
s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat *stat, CARDCallback callback) { (void)chan; (void)fileNo; (void)stat; (void)callback; return CARD_RESULT_NOCARD; }
s32 CARDUnmount(s32 chan) { (void)chan; return CARD_RESULT_NOCARD; }
s32 CARDGetCurrentMode(s32 chan, u32 *mode) { (void)chan; (void)mode; return CARD_RESULT_NOCARD; }
s32 CARDCancel(CARDFileInfo *fileInfo) { (void)fileInfo; return CARD_RESULT_NOCARD; }
s32 CARDClose(CARDFileInfo *fileInfo) { (void)fileInfo; return CARD_RESULT_NOCARD; }

s32 CARDRead(CARDFileInfo *fileInfo, void *addr, s32 length, s32 offset) { (void)fileInfo; (void)addr; (void)length; (void)offset; return CARD_RESULT_NOCARD; }
s32 CARDReadAsync(CARDFileInfo *fileInfo, void *addr, s32 length, s32 offset, CARDCallback callback) { (void)fileInfo; (void)addr; (void)length; (void)offset; (void)callback; return CARD_RESULT_NOCARD; }
s32 CARDWrite(CARDFileInfo *fileInfo, const void *addr, s32 length, s32 offset) { (void)fileInfo; (void)addr; (void)length; (void)offset; return CARD_RESULT_NOCARD; }
s32 CARDWriteAsync(CARDFileInfo *fileInfo, const void *addr, s32 length, s32 offset, CARDCallback callback) { (void)fileInfo; (void)addr; (void)length; (void)offset; (void)callback; return CARD_RESULT_NOCARD; }
