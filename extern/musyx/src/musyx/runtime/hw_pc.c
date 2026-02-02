#include "musyx/platform.h"

#if MUSY_TARGET == MUSY_TARGET_PC
#include <pthread.h>
#include <SDL.h>
#include "musyx/assert.h"
#include "musyx/hardware.h"
#include "musyx/sal.h"
#include <string.h>

static volatile u32 oldState = 0;
static volatile u16 hwIrqLevel = 0;
static volatile u32 salDspInitIsDone = 0;
static volatile u64 salLastTick = 0;
static volatile u32 salLogicActive = 0;
static volatile u32 salLogicIsWaiting = 0;
static volatile u32 salDspIsDone = 0;
void* salAIBufferBase = NULL;
static u8 salAIBufferIndex = 0;
static SND_SOME_CALLBACK userCallback = NULL;

#define DMA_BUFFER_LEN 0x280
pthread_mutex_t globalMutex;
pthread_mutex_t globalInterrupt;

/* SDL2 audio device handle */
static SDL_AudioDeviceID sdlAudioDev = 0;

u32 salGetStartDelay();
static void callUserCallback() {
  if (salLogicActive) {
    return;
  }
  salLogicActive = 1;
  userCallback();
  salLogicActive = 0;
}

void salCallback() {
  salAIBufferIndex = (salAIBufferIndex + 1) % 4;
  salLastTick = 0;
  if (salDspIsDone) {
    callUserCallback();
  } else {
    salLogicIsWaiting = 1;
  }
}

void dspInitCallback() {
  salDspIsDone = TRUE;
  salDspInitIsDone = TRUE;
}

void dspResumeCallback() {
  salDspIsDone = TRUE;
  if (salLogicIsWaiting) {
    salLogicIsWaiting = FALSE;
    callUserCallback();
  }
}

/*
 * SDL2 audio callback — runs on a dedicated audio thread.
 * Each invocation simulates what the GC AI DMA interrupt would do:
 * advance the ring buffer, run the synthesis chain, then copy
 * the mixed PCM into SDL's output buffer.
 */
static void sdlAudioCallback(void* userdata, Uint8* stream, int len) {
  (void)userdata;

  /* Drive the synthesis callback (equivalent to the AI DMA interrupt) */
  salCallback();

  /* The synth writes into the ring buffer slot at (index+2)%4.
   * After salCallback advances the index, the *previous* dest slot
   * (now at (index+1)%4) contains the freshly mixed audio. */
  u8 readIdx = (salAIBufferIndex + 1) % 4;
  void* src = (u8*)salAIBufferBase + readIdx * DMA_BUFFER_LEN;

  if (len <= (int)DMA_BUFFER_LEN) {
    memcpy(stream, src, len);
  } else {
    memcpy(stream, src, DMA_BUFFER_LEN);
    memset(stream + DMA_BUFFER_LEN, 0, len - DMA_BUFFER_LEN);
  }
}

bool salInitAi(SND_SOME_CALLBACK callback, u32 unk, u32* outFreq) {
  if ((salAIBufferBase = salMalloc(DMA_BUFFER_LEN * 4)) != NULL) {
    memset(salAIBufferBase, 0, DMA_BUFFER_LEN * 4);
    salAIBufferIndex = 0;
    salLogicIsWaiting = FALSE;
    salDspIsDone = TRUE;
    salLogicActive = FALSE;
    userCallback = callback;

    synthInfo.numSamples = 0x20;
    *outFreq = 32000;

    /* Open SDL2 audio device: 32 kHz, signed 16-bit stereo */
    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = 32000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    /* DMA_BUFFER_LEN = 0x280 = 640 bytes = 160 stereo samples */
    want.samples = DMA_BUFFER_LEN / (2 * sizeof(s16));
    want.callback = sdlAudioCallback;
    want.userdata = NULL;

    sdlAudioDev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (sdlAudioDev == 0) {
      MUSY_DEBUG("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
      /* Non-fatal: game runs without sound */
    } else {
      MUSY_DEBUG("MusyX AI interface initialized (SDL2 audio: %dHz, %dch, %d samples).\n",
                 have.freq, have.channels, have.samples);
    }
    return TRUE;
  }

  return FALSE;
}

bool salStartAi() {
  if (sdlAudioDev != 0) {
    SDL_PauseAudioDevice(sdlAudioDev, 0);
  }
  return TRUE;
}

bool salExitAi() {
  if (sdlAudioDev != 0) {
    SDL_CloseAudioDevice(sdlAudioDev);
    sdlAudioDev = 0;
  }
  salFree(salAIBufferBase);
  return TRUE;
}

void* salAiGetDest() {
  u8 index = (salAIBufferIndex + 2) % 4;
  return (u8*)salAIBufferBase + index * DMA_BUFFER_LEN;
}

bool salInitDsp(u32 a) {
  (void)a;
  /* No real DSP on PC — software synthesis handles everything */
  salDspIsDone = TRUE;
  salDspInitIsDone = TRUE;
  return TRUE;
}

bool salExitDsp() { return TRUE; }

void salStartDsp(u16* cmdList) {
  (void)cmdList;
  /* On PC we process the command list synchronously in software.
   * Signal completion immediately so the callback chain proceeds. */
  dspResumeCallback();
}

void salCtrlDsp(s16* dest) {
  salBuildCommandList(dest, salGetStartDelay());
  salStartDsp(dspCmdList);
}

u32 salGetStartDelay() { return 0; }

void hwInitIrq() {
  hwIrqLevel = 1;
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&globalMutex, &attr);
}

void hwExitIrq() {}

void hwEnableIrq() {
  if (--hwIrqLevel == 0) {
    pthread_mutex_unlock(&globalMutex);
  }
}

void hwDisableIrq() {
  if ((hwIrqLevel++) == 0) {
    pthread_mutex_lock(&globalMutex);
  }
}

void hwIRQEnterCritical() {
  pthread_mutex_lock(&globalMutex);
}

void hwIRQLeaveCritical() {
  pthread_mutex_unlock(&globalMutex);
}
#endif
