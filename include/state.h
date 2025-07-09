#pragma once

#include <stdint.h>
#include <psputility_modules.h>
#include "control.h"

typedef struct {
    uint8_t isRunning;
    uint8_t currentState;
    uint8_t isMenu;
    Controller controller;
    Font font;
    bool jpegWorking;
    void * currentViewData;
    Color bgColor;
    char toPlayFile[257];
    char toPlayFolder[257];
} AppState;

bool StartJpegPsp();
void StopJpegPsp();
void menu_destroy(AppState &appState);
void ExitApp(AppState &appState) {
    appState.isRunning = false;
    menu_destroy(appState);
    if (appState.jpegWorking) StopJpegPsp();
    CloseWindow();
    sceKernelExitGame();
}

#define DEBUG_LOG_SIZE 24
static char CustomLogBuffer[DEBUG_LOG_SIZE][128] = { 0 };
void CustomLog(int msgType, const char *text, va_list args) {
    char* customLogLine = CustomLogBuffer[0];
    for (int i = DEBUG_LOG_SIZE-1; i > 0 ; i--) strcpy(CustomLogBuffer[i], CustomLogBuffer[i-1]);
    switch (msgType) {
        case LOG_INFO: sprintf(customLogLine,    "[INFO] : "); break;
        case LOG_ERROR: sprintf(customLogLine,   "[ERROR]: "); break;
        case LOG_WARNING: sprintf(customLogLine, "[WARN] : "); break;
        case LOG_DEBUG: sprintf(customLogLine,   "[DEBUG]: "); break;
        default: break;
    }
    const int cur = strlen(customLogLine);
    vsnprintf(customLogLine+cur, 127-cur, text, args);
    sceIoWrite(1, customLogLine, strlen(customLogLine));
    sceIoWrite(1, "\n", 1);
}

void DrawLogData() {
    for (int i = 0; i < DEBUG_LOG_SIZE; i++) DrawText(CustomLogBuffer[i], 10, i * 10 + 30, 10, GRAY);
}

int exit_callback(int arg1, int arg2, void* common) {
    ExitApp(*(AppState *) common);
	return 0;
}

int CallbackThread(SceSize args, void* argp) {
	int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, argp);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();
	return 0;
}

int SetupCallbacks(AppState * appState) {
    sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    pspAudioInit();
    appState->jpegWorking = StartJpegPsp();
    sceUtilityLoadModule(PSP_MODULE_AV_MP3);
	int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
	if (thid >= 0) sceKernelStartThread(thid, sizeof(AppState), appState);
	return thid;
}

#define STATE_MENU              0
#define STATE_PLAY              1

#define RETURN_STATE_CONTINUE   0
#define RETURN_STATE_EXIT       1
#define RETURN_STATE_PLAY       2
#define RETURN_STATE_HOME       3