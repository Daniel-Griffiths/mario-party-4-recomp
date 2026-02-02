#include "dolphin.h"

#ifdef TARGET_PC
static inline s32 _mess_bes32(const void *p) {
    const u8 *b = (const u8 *)p;
    return (s32)((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]);
}
static inline u16 _mess_beu16(const void *p) {
    const u8 *b = (const u8 *)p;
    return (u16)((b[0] << 8) | b[1]);
}
#define MESS_S32(p) _mess_bes32(p)
#define MESS_U16(p) _mess_beu16(p)
#else
#define MESS_S32(p) (*(s32*)(p))
#define MESS_U16(p) (*(u16*)(p))
#endif

static void *MessData_MesDataGet(void *messdata, u32 id)
{
    s32 i;
    s32 max_bank;
    u16 *banks;
    u16 bank;
    s32 *data;
    bank = id >> 16;
    data = messdata;
    max_bank = MESS_S32(data);
    data++;
    banks = (u16 *)(((u8 *)messdata)+MESS_S32(data));
    for(i=max_bank; i != 0; i--, banks += 2) {
        if(MESS_U16(banks) == bank) {
            break;
        }
    }
    if(i == 0) {
        return NULL;
    } else {
        data += MESS_U16(&banks[1]);
        return (((u8 *)messdata)+MESS_S32(data));
    }
}

static void *_MessData_MesPtrGet(void *messbank, u32 id)
{
    u16 index;
    s32 max_index;
    s32 *data;

    index = id & 0xFFFF;
    data = messbank;
    max_index = MESS_S32(data);
    data++;
    if(max_index <= index) {
        return NULL;
    } else {
        data =  data+index;
        return (((u8 *)messbank)+MESS_S32(data));
    }
}

void *MessData_MesPtrGet(void *messdata, u32 id)
{
    void *bank = MessData_MesDataGet(messdata, id);
    if(bank) {
        return _MessData_MesPtrGet(bank, id);
    }
    return NULL;
}