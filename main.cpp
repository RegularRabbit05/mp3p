#include <pspkernel.h>
#include <pspfpu.h>
#include <pspsysmem_kernel.h>

#define ATTR_PSP_WIDTH 480
#define ATTR_PSP_HEIGHT 272

PSP_MODULE_INFO("(M)P3P", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(20*1024);
PSP_MAIN_THREAD_STACK_SIZE_KB(1024);

#include <raylib.h>
#include <math.h>
#include <cstdio>
#include <malloc.h>

#include "include/state.h"
#include "include/utils.hpp"
#include "include/control.h"
#include "include/waves.h"

#include "include/views/menu.hpp"
#include "include/views/play.hpp"

void DrawDiagnostic(AppState &appState) {
    if constexpr (!DEBUG) return;
    DrawText(TextFormat("%d", GetFPS()), 465, 10, 10, GREEN);
    if (!appState.controller.t) return;
    struct mallinfo mi = mallinfo();
    int free_bytes = mi.fordblks;
    int allocated_bytes = mi.arena;
    const char* text = TextFormat("FM: %d MM: %d FH: %d UH: %d FPS: %d J: %c", sceKernelTotalFreeMemSize(), sceKernelMaxFreeMemSize(), free_bytes, allocated_bytes, GetFPS(), appState.jpegWorking ? 'Y' : 'N');
    DrawTextEx(appState.font, text, {10, 10}, 20, 0, GREEN);
    DrawLogData();
}

int main() {
    sceKernelSetCompiledSdkVersion(0x06060010);
    _DisableFPUExceptions();
    pspFpuSetEnable(0);
    AppState appState;
    appState.isRunning = 1;
    appState.bgColor = {20, 20, 20, 255};
    appState.isMenu = 1;
    appState.currentState = STATE_MENU;
    appState.toPlayFolder[0] = 0;
    appState.toPlayFile[0] = 0;
    SetupCallbacks(&appState);
    constexpr int screenWidth = ATTR_PSP_WIDTH;
    constexpr int screenHeight = ATTR_PSP_HEIGHT;
    InitWindow(screenWidth, screenHeight, nullptr);
    SetTraceLogCallback(CustomLog);
    appState.font = LoadFontEx("assets/opensans.ttf", 20, 0, 270);
    SetTextureFilter(appState.font.texture, TEXTURE_FILTER_BILINEAR);
  
    menu_init(appState);

    while (appState.isRunning) {
        int retVal = RETURN_STATE_CONTINUE;
        BeginDrawing();
        UpdateControls(&appState.controller);
        ClearBackground(appState.bgColor);
        switch (appState.currentState) {
            case STATE_MENU: {
                retVal = menu_handle(appState);
            } break;
            case STATE_PLAY: {
                retVal = play_handle(appState);
            } break;
            default: retVal = RETURN_STATE_EXIT;
        }
        DrawDiagnostic(appState);
        EndDrawing();
        switch (retVal) {
            case RETURN_STATE_CONTINUE: break;
            case RETURN_STATE_EXIT: appState.isRunning = 0; break;
            case RETURN_STATE_PLAY: {
                menu_destroy(appState);
                appState.currentState = STATE_PLAY;
                play_init(appState);
            } break;
            case RETURN_STATE_HOME: {
                play_destroy(appState);
                appState.currentState = STATE_MENU;
                menu_init(appState);
                ((MenuState *) appState.currentViewData)->wasPressing = true;
            } break;
            default: break;
        }
    }

    ExitApp(appState);
    return 0;
}
