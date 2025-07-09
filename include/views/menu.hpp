#pragma once

#include <psppower.h>
#include <dirent.h>
#include <cstring>

#define MENU_USE_THREAD     false

#define DISK_MATERIAL_ID    3

#define MENU_FILE_GENERIC   0
#define MENU_FILE_MUSIC     1

const char* menu_tooltips[] = {
    "START: Play current folder",
    "SELECT: Exit",
    "X: Select",
    "O: Back",
};

typedef struct {
    bool isFolder;
    char name[257];
} MenuState_ListItem;

typedef struct {
    double nextWaveIn;
    Waves * waves;
    Model * folderModel;
    Model * fileModel;
    Model * diskModel;
    Texture * diskTexture;
    Texture * currentMusicTexture;
    Camera3D cam;
    bool wasPressing;
    bool imageDecodeRunning;
    bool cancelThread;
    bool hasToDeleteTexture;
    bool fadingOut;
    bool firstFrame;
    char * imageToDecode;
    float toNext;
    float rotation;
    float timeUntilLoad;
    int updateThreadId;

    char currentDir[256];
    int filesCount;
    int selectedFile;
    MenuState_ListItem * fileNames;
} MenuState;

int menu_compareMenuItems(const void *a, const void *b) {
    const MenuState_ListItem *itemA = (const MenuState_ListItem *)a;
    const MenuState_ListItem *itemB = (const MenuState_ListItem *)b;
    if (itemA->isFolder && !itemB->isFolder) return -1;
    if (!itemA->isFolder && itemB->isFolder) return 1;
    return strcmp(itemA->name, itemB->name);
}

void menu_loadCurrentFolder(AppState &appState, MenuState *menuState) {
    menuState->filesCount = 0;
    menuState->selectedFile = 0;
    if (menuState->fileNames != nullptr) free(menuState->fileNames);
    menuState->fileNames = nullptr;
    DIR * dir = opendir(menuState->currentDir);
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") == 0) continue;
        menuState->filesCount++;
    }
    closedir(dir);
    if (menuState->filesCount == 0) return;
    menuState->fileNames = (MenuState_ListItem*) malloc(sizeof(MenuState_ListItem) * menuState->filesCount);
    dir = opendir(menuState->currentDir);
    for (int i = 0; (entry = readdir(dir)) != nullptr && i < menuState->filesCount; i++) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0) {
                i--; 
                continue;
            }
            menuState->fileNames[i].isFolder = true;
        } else {
            menuState->fileNames[i].isFolder = false;
        }

        strncpy(menuState->fileNames[i].name, entry->d_name, 256);
        menuState->fileNames[i].name[255] = '\0';
    }
    closedir(dir);
    qsort(menuState->fileNames, menuState->filesCount, sizeof(MenuState_ListItem), menu_compareMenuItems);
}

int menu_pictureUpdateThread(SceSize args, void *argp) {
    AppState * appState = (AppState *) argp;
    MenuState * menuState = (MenuState *) appState->currentViewData;
    if (menuState->updateThreadId != 0) TraceLog(LOG_INFO, "Decoder thread started!");
    while (!menuState->cancelThread) {
        if (menuState->imageDecodeRunning) {
            if (menuState->updateThreadId != 0) TraceLog(LOG_INFO, "Decoder received new task!");
            Texture t = readPictureFromFileMP3(menuState->imageToDecode, appState);
            if (menuState->currentMusicTexture != nullptr) *menuState->currentMusicTexture = t;
            menuState->imageDecodeRunning = false;
        }
        if (menuState->updateThreadId == 0) break;
        sceKernelDelayThreadCB(10000);
    }
    menuState->imageDecodeRunning = false;

    if (menuState->updateThreadId != 0) sceKernelExitDeleteThread(0);
    return 0;
}

void menu_init(AppState &appState) {
    appState.isMenu = 1;
    MenuState * menuState = (MenuState *) malloc(sizeof(MenuState));
    menuState->fileNames = nullptr;
    menuState->waves = (Waves *) malloc(sizeof(Waves));
    *menuState->waves = Waves{};
    menuState->nextWaveIn = 0;
    appState.currentViewData = menuState;
    menuState->toNext = 0;
    menuState->fadingOut = false;
    menuState->firstFrame = true;
    scePowerSetClockFrequency(333, 333, 166);
    menuState->folderModel = (Model*) malloc(sizeof(Model));
    menuState->fileModel = (Model*) malloc(sizeof(Model));
    menuState->diskModel = (Model*) malloc(sizeof(Model));
    *menuState->folderModel = LoadModel("assets/folder.glb");
    *menuState->fileModel = LoadModel("assets/file.glb");
    *menuState->diskModel = LoadModel("assets/disk.glb");
    menuState->diskTexture = (Texture*) malloc(sizeof(Texture));
    *menuState->diskTexture = menuState->diskModel->materials[DISK_MATERIAL_ID].maps[MATERIAL_MAP_DIFFUSE].texture;
    menuState->currentMusicTexture = nullptr;
    menuState->filesCount = 0;
    menuState->timeUntilLoad = 0;
    menuState->imageDecodeRunning = false;
    menuState->cancelThread = false;
    menuState->hasToDeleteTexture = false;
    menuState->imageToDecode = (char *) malloc(257*2);
    if constexpr (MENU_USE_THREAD) menuState->updateThreadId = sceKernelCreateThread("picture_thread", menu_pictureUpdateThread, sceKernelGetThreadCurrentPriority() - 1, 1024*256, 0, 0); else menuState->updateThreadId = 0;
    if (appState.toPlayFolder[0] != 0) {
        strcpy(menuState->currentDir, appState.toPlayFolder);
    } else {
        strcpy(menuState->currentDir, "ms0:/MUSIC/");
        DIR* dir = opendir(menuState->currentDir);
        if (dir == nullptr) {
            mkdir(menuState->currentDir, 0777);
        } else {
            closedir(dir);
        }
    }
    menu_loadCurrentFolder(appState, menuState);
    menuState->cam = {
        {0.0f, 0.0f, 4.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        45,
    };
    if (appState.toPlayFolder[0] != 0 && appState.toPlayFile[0] != 0) {
        for (int i = 0; i < menuState->filesCount; i++) {
            if (strcmp(menuState->fileNames[i].name, appState.toPlayFile) == 0) {
                menuState->selectedFile = i;
                menuState->timeUntilLoad = 0.5f;
                break;
            }
        }
    }
    if (menuState->updateThreadId != 0) sceKernelStartThread(menuState->updateThreadId, sizeof(AppState), &appState);
    menuState->wasPressing = true;
    Waves_setAlphaPercent(menuState->waves, 0);
}

void menu_destroy(AppState &appState) {
    if (appState.isMenu != 1) return;
    MenuState * menuState = ((MenuState *) appState.currentViewData);
    menuState->cancelThread = true;
    menuState->diskModel->materials[DISK_MATERIAL_ID].maps[MATERIAL_MAP_DIFFUSE].texture = *menuState->diskTexture;
    free(menuState->diskTexture);
    UnloadModelFull(*menuState->folderModel);
    UnloadModelFull(*menuState->fileModel);
    UnloadModelFull(*menuState->diskModel);
    while (menuState->imageDecodeRunning) {}
    if (menuState->updateThreadId != 0) sceKernelWaitThreadEndCB(menuState->updateThreadId, nullptr);
    if (menuState->currentMusicTexture != nullptr) {
        if (menuState->currentMusicTexture->id != 0) UnloadTexture(*menuState->currentMusicTexture);
        free(menuState->currentMusicTexture);
    }
    free(menuState->imageToDecode);

    free(menuState->folderModel);
    free(menuState->fileModel);
    free(menuState->fileNames);
    free(menuState->waves);
    free(appState.currentViewData);
    scePowerSetClockFrequency(222, 222, 111);
    appState.isMenu = 0;
}

#define CLAMP255(val) ((val) < 0 ? 0 : ((val) > 255 ? 255 : (val)))
int menu_handle(AppState &appState) {
    if (appState.isMenu == 0) return RETURN_STATE_EXIT;
    int rVal = RETURN_STATE_CONTINUE;
    MenuState * menuState = ((MenuState *) appState.currentViewData);
    if (menuState->nextWaveIn <= 0) {
        Waves_lineWorker(menuState->waves);
        menuState->nextWaveIn = 100;
    }
    Waves_draw(menuState->waves);
    menuState->nextWaveIn -= GetFrameTime() * 1000;
    bool hasSelectedFile = false;
    if (menuState->fadingOut) goto skipInput;
    if (appState.controller.x) {
        if (!menuState->wasPressing && menuState->toNext == 0) {
            if (menuState->fileNames[menuState->selectedFile].isFolder) {
                if (strcmp(menuState->fileNames[menuState->selectedFile].name, "..") == 0) {
                    if (strcmp(menuState->currentDir, "ms0:/") != 0) {
                        char * lastSlash = strrchr(menuState->currentDir, '/');
                        *lastSlash = '\0';
                        char * lastSlash2 = strrchr(menuState->currentDir, '/');
                        lastSlash2[1] = '\0';
                    }
                } else {
                    strcat(menuState->currentDir, menuState->fileNames[menuState->selectedFile].name);
                    strcat(menuState->currentDir, "/");
                }
                menu_loadCurrentFolder(appState, menuState);
            } else hasSelectedFile = true;
        }
        menuState->wasPressing = true;
    } else if (appState.controller.o) {
        if (!menuState->wasPressing && menuState->toNext == 0) {
            if (strcmp(menuState->currentDir, "ms0:/") != 0) {
                char * lastSlash = strrchr(menuState->currentDir, '/');
                *lastSlash = '\0';
                char * lastSlash2 = strrchr(menuState->currentDir, '/');
                lastSlash2[1] = '\0';
            }
            menu_loadCurrentFolder(appState, menuState);
        }
        menuState->wasPressing = true;
    } else if (appState.controller.up) {
        if (!menuState->wasPressing && menuState->toNext == 0) {
            if (menuState->filesCount > 1) menuState->toNext = 1.0f;
        }
        menuState->wasPressing = true;
    } else if (appState.controller.down) {
        if (!menuState->wasPressing && menuState->toNext == 0) {
            if (menuState->filesCount > 1) menuState->toNext = -1.0f;
        }
        menuState->wasPressing = true;
    } else if (appState.controller.select) {
        if (!menuState->wasPressing) {
            rVal = RETURN_STATE_EXIT;
        }
        menuState->wasPressing = true;
    } else menuState->wasPressing = false;

    skipInput:
    if (menuState->fadingOut) {
        const float currentFade = Waves_getAlphaPercent(menuState->waves);
        if (currentFade <= 0) {
            rVal = RETURN_STATE_PLAY;
        }
        Waves_setAlphaPercent(menuState->waves, currentFade-GetFrameTime()*2.f);
    } else {
        const float currentFade = Waves_getAlphaPercent(menuState->waves)+GetFrameTime()*(menuState->firstFrame ? 0 : 2.f);
        Waves_setAlphaPercent(menuState->waves, currentFade);
        const float alpha = currentFade * 255;
        Color copy = appState.bgColor;
        copy.a = 255 - (uint8_t) ((float) CLAMP255(alpha));
        DrawRectangle(0, 0, ATTR_PSP_WIDTH, ATTR_PSP_HEIGHT, copy);
    }

    if (menuState->toNext != 0) {
        bool goingDown = menuState->toNext < 0;
        bool del = false;;
        if (goingDown) {
            menuState->toNext += GetFrameTime() * 10;
            if (menuState->toNext > 0) {
                menuState->toNext = 0;

                menuState->selectedFile = (menuState->selectedFile+1) % menuState->filesCount;
                del = true;
            }
        } else {
            menuState->toNext -= GetFrameTime() * 10;
            if (menuState->toNext < 0) {
                menuState->toNext = 0;

                menuState->selectedFile--;
                if (menuState->selectedFile < 0) menuState->selectedFile = menuState->filesCount - 1;
                del = true;
            }
        }
        if (del) {
            menuState->rotation = -200;
            menuState->hasToDeleteTexture = true;
        }
    }

    if (menuState->hasToDeleteTexture && !menuState->imageDecodeRunning) {
        menuState->hasToDeleteTexture = false;
        if (menuState->currentMusicTexture != nullptr) {
            if (menuState->currentMusicTexture->id != 0) UnloadTexture(*menuState->currentMusicTexture);
            free(menuState->currentMusicTexture);
            menuState->currentMusicTexture = nullptr;
        }
        menuState->timeUntilLoad = 0.5f;
    }

    bool currentIsPlayable = false;
    menuState->rotation += GetFrameTime() * 50;
    if (menuState->rotation > 360) menuState->rotation -= 360;
    menuState->cam.position.y = -menuState->selectedFile - (menuState->toNext == 0 ? 0 : (menuState->toNext < 0 ? menuState->toNext + 1 : menuState->toNext - 1));
    menuState->cam.target.y = menuState->cam.position.y;
    BeginMode3D(menuState->cam);
    for (int i = 0; i < menuState->filesCount; i++) {
        if (abs(i - menuState->selectedFile) > 3) continue;
        Model * model = menuState->folderModel;
        const bool isFolder = menuState->fileNames[i].isFolder;
        if (!isFolder) {
            model = menuState->fileModel;
            if (isFilePlayable(menuState->fileNames[i].name)) {
                if (i == menuState->selectedFile) currentIsPlayable = true;
                model = menuState->diskModel;
            }
        }
        float scaleMod = 1;
        menuState->diskModel->materials[DISK_MATERIAL_ID].maps[MATERIAL_MAP_DIFFUSE].texture = *menuState->diskTexture;
        if (i == menuState->selectedFile) {
            if (!menuState->imageDecodeRunning && menuState->currentMusicTexture != nullptr && menuState->currentMusicTexture->id != 0) {
                SetTextureFilter(*menuState->currentMusicTexture, TEXTURE_FILTER_BILINEAR);
                menuState->diskModel->materials[DISK_MATERIAL_ID].maps[MATERIAL_MAP_DIFFUSE].texture = *menuState->currentMusicTexture;
            }
            if (menuState->fadingOut) {
                scaleMod += 1.0f-Waves_getAlphaPercent(menuState->waves);
            }
        }
        DrawModelEx(*model, {-2, -(float) i, 0}, {0, 1, 0}, ((isFolder || i != menuState->selectedFile) ? 0 : menuState->rotation < 0 ? 0 : menuState->rotation)+30, {scaleMod, scaleMod, scaleMod}, WHITE);
    }
    EndMode3D();
    if (menuState->filesCount != 0) {
        const bool hasRoot = strcmp(menuState->fileNames[0].name, "..") == 0;
        DrawTextEx(appState.font, TextFormat("%d", menuState->selectedFile + (hasRoot ? 0 : 1)), {7, 272/2-10}, 20, 0, DARKGRAY);
        const char* name = menuState->fileNames[menuState->selectedFile].name;
        char* ext = nullptr;
        if (currentIsPlayable) {
            ext = strrchr(name, '.');
            if (ext != nullptr) *ext = 0;
        }
        DrawTextEx(appState.font, name, {140, 272/2-10}, 20, 0, LIGHTGRAY);
        if (ext != nullptr) {
            *ext = '.';
        }
    }

    DrawTextEx(appState.font, menuState->currentDir, {470-MeasureTextEx(appState.font, menuState->currentDir, 20, 0).x, 170}, 20, 0, DARKGRAY);
    for (int i = sizeof(menu_tooltips)/sizeof(menu_tooltips[0])-1; i >= 0; i--) {
        DrawTextEx(appState.font, menu_tooltips[i], {470-MeasureTextEx(appState.font, menu_tooltips[i], 20, 0).x, 250-((float)i*20)}, 20, 0, LIGHTGRAY);
    }

    if (!menuState->imageDecodeRunning && currentIsPlayable && menuState->currentMusicTexture == nullptr) {
        if (menuState->timeUntilLoad == 0) {
            menuState->currentMusicTexture = (Texture *) malloc(sizeof(Texture));
            menuState->currentMusicTexture->id = 0;
            strcpy(menuState->imageToDecode, menuState->currentDir);
            strcat(menuState->imageToDecode, menuState->fileNames[menuState->selectedFile].name);
            if (menuState->updateThreadId != 0) {
                menuState->imageDecodeRunning = true;
            } else {
                if constexpr (!MENU_USE_THREAD) {
                    menuState->imageDecodeRunning = true;
                    menu_pictureUpdateThread(0, &appState);
                } else TraceLog(LOG_ERROR, "Decoder thread is not running!");
            }
        } else {
            menuState->timeUntilLoad -= GetFrameTime();
            if (menuState->timeUntilLoad < 0) {
                menuState->timeUntilLoad = 0;
            }
        }
    }

    if (menuState->fadingOut) {
        float alpha = Waves_getAlphaPercent(menuState->waves) * 255;
        Color copy = appState.bgColor;
        copy.a = 255 - (uint8_t) ((float) CLAMP255(alpha));
        DrawRectangle(0, 0, ATTR_PSP_WIDTH, ATTR_PSP_HEIGHT, copy);
    }

    if ((hasSelectedFile && currentIsPlayable) && Waves_getAlphaPercent(menuState->waves) == 1) {
        menuState->fadingOut = true;
        if (hasSelectedFile) {
            strcpy(appState.toPlayFile, menuState->fileNames[menuState->selectedFile].name);
            strcpy(appState.toPlayFolder, menuState->currentDir);
        }
    }

    menuState->firstFrame = false;
    return rVal;
}