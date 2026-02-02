/*
 * PAD (Controller) replacement using SDL2 GameController
 */
#include <SDL.h>
#include <stdio.h>
#include <string.h>

#include "dolphin/types.h"
#include "dolphin/pad.h"

static SDL_GameController *g_controllers[4] = { NULL, NULL, NULL, NULL };
static int g_pad_initialized = 0;

BOOL PADInit(void) {
    printf("[PAD] PADInit()\n");
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    }
    /* Open up to 4 controllers */
    int num_joy = SDL_NumJoysticks();
    int ctrl_idx = 0;
    for (int i = 0; i < num_joy && ctrl_idx < 4; i++) {
        if (SDL_IsGameController(i)) {
            g_controllers[ctrl_idx] = SDL_GameControllerOpen(i);
            if (g_controllers[ctrl_idx]) {
                printf("[PAD] Controller %d: %s\n", ctrl_idx,
                       SDL_GameControllerName(g_controllers[ctrl_idx]));
                ctrl_idx++;
            }
        }
    }
    g_pad_initialized = 1;
    return TRUE;
}

u32 PADRead(PADStatus *status) {
    if (!status) return 0;
    memset(status, 0, sizeof(PADStatus) * 4);

    u32 connected = 0;

    /* Poll SDL events to update controller state */
    SDL_GameControllerUpdate();

    /* Read from game controllers */
    for (int i = 0; i < 4; i++) {
        if (g_controllers[i] && SDL_GameControllerGetAttached(g_controllers[i])) {
            connected |= (1 << (31 - i)); /* PAD_CHAN*_BIT */
            status[i].err = 0; /* PAD_ERR_NONE */

            /* Sticks */
            status[i].stickX = (s8)(SDL_GameControllerGetAxis(g_controllers[i], SDL_CONTROLLER_AXIS_LEFTX) / 256);
            status[i].stickY = (s8)(-SDL_GameControllerGetAxis(g_controllers[i], SDL_CONTROLLER_AXIS_LEFTY) / 256);
            status[i].substickX = (s8)(SDL_GameControllerGetAxis(g_controllers[i], SDL_CONTROLLER_AXIS_RIGHTX) / 256);
            status[i].substickY = (s8)(-SDL_GameControllerGetAxis(g_controllers[i], SDL_CONTROLLER_AXIS_RIGHTY) / 256);

            /* Triggers */
            status[i].triggerL = (u8)(SDL_GameControllerGetAxis(g_controllers[i], SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 128);
            status[i].triggerR = (u8)(SDL_GameControllerGetAxis(g_controllers[i], SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 128);

            /* Buttons */
            u16 btn = 0;
            if (SDL_GameControllerGetButton(g_controllers[i], SDL_CONTROLLER_BUTTON_A)) btn |= PAD_BUTTON_A;
            if (SDL_GameControllerGetButton(g_controllers[i], SDL_CONTROLLER_BUTTON_B)) btn |= PAD_BUTTON_B;
            if (SDL_GameControllerGetButton(g_controllers[i], SDL_CONTROLLER_BUTTON_X)) btn |= PAD_BUTTON_X;
            if (SDL_GameControllerGetButton(g_controllers[i], SDL_CONTROLLER_BUTTON_Y)) btn |= PAD_BUTTON_Y;
            if (SDL_GameControllerGetButton(g_controllers[i], SDL_CONTROLLER_BUTTON_START)) btn |= PAD_BUTTON_START;
            if (SDL_GameControllerGetButton(g_controllers[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT)) btn |= PAD_BUTTON_LEFT;
            if (SDL_GameControllerGetButton(g_controllers[i], SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) btn |= PAD_BUTTON_RIGHT;
            if (SDL_GameControllerGetButton(g_controllers[i], SDL_CONTROLLER_BUTTON_DPAD_UP)) btn |= PAD_BUTTON_UP;
            if (SDL_GameControllerGetButton(g_controllers[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN)) btn |= PAD_BUTTON_DOWN;
            if (status[i].triggerL > 128) btn |= PAD_TRIGGER_L;
            if (status[i].triggerR > 128) btn |= PAD_TRIGGER_R;
            if (SDL_GameControllerGetButton(g_controllers[i], SDL_CONTROLLER_BUTTON_LEFTSTICK)) btn |= PAD_TRIGGER_Z;
            status[i].button = btn;
        } else {
            status[i].err = -1; /* PAD_ERR_NO_CONTROLLER */
        }
    }

    /* Keyboard fallback for player 1 */
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if (!(connected & (1 << 31))) {
        connected |= (1 << 31);
        status[0].err = 0;
    }

    u16 kbtn = status[0].button;
    if (keys[SDL_SCANCODE_Z]) kbtn |= PAD_BUTTON_A;
    if (keys[SDL_SCANCODE_X]) kbtn |= PAD_BUTTON_B;
    if (keys[SDL_SCANCODE_C]) kbtn |= PAD_BUTTON_X;
    if (keys[SDL_SCANCODE_V]) kbtn |= PAD_BUTTON_Y;
    if (keys[SDL_SCANCODE_RETURN]) kbtn |= PAD_BUTTON_START;
    if (keys[SDL_SCANCODE_LEFT])  kbtn |= PAD_BUTTON_LEFT;
    if (keys[SDL_SCANCODE_RIGHT]) kbtn |= PAD_BUTTON_RIGHT;
    if (keys[SDL_SCANCODE_UP])    kbtn |= PAD_BUTTON_UP;
    if (keys[SDL_SCANCODE_DOWN])  kbtn |= PAD_BUTTON_DOWN;
    if (keys[SDL_SCANCODE_Q]) kbtn |= PAD_TRIGGER_L;
    if (keys[SDL_SCANCODE_E]) kbtn |= PAD_TRIGGER_R;
    if (keys[SDL_SCANCODE_TAB]) kbtn |= PAD_TRIGGER_Z;
    status[0].button = kbtn;

    /* WASD → left stick */
    s8 sx = status[0].stickX, sy = status[0].stickY;
    if (keys[SDL_SCANCODE_A]) sx = -80;
    if (keys[SDL_SCANCODE_D]) sx = 80;
    if (keys[SDL_SCANCODE_W]) sy = 80;
    if (keys[SDL_SCANCODE_S]) sy = -80;
    status[0].stickX = sx;
    status[0].stickY = sy;

    return connected;
}

BOOL PADReset(u32 mask) { (void)mask; return TRUE; }
BOOL PADRecalibrate(u32 mask) { (void)mask; return TRUE; }

void PADClamp(PADStatus *status) {
    for (int i = 0; i < 4; i++) {
        if (status[i].stickX > 76) status[i].stickX = 76;
        if (status[i].stickX < -76) status[i].stickX = -76;
        if (status[i].stickY > 76) status[i].stickY = 76;
        if (status[i].stickY < -76) status[i].stickY = -76;
    }
}

void PADClampCircle(PADStatus *status) { PADClamp(status); }
void PADControlMotor(s32 chan, u32 cmd) { (void)chan; (void)cmd; }
void PADControlAllMotors(const u32 *cmds) { (void)cmds; }
void PADSetSpec(u32 spec) { (void)spec; }
PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback) { (void)callback; return NULL; }
void PADSetAnalogMode(u32 mode) { (void)mode; }
