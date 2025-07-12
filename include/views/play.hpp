#pragma once

#include "../mp3.h"

#define PLAY_TEXTURE_SIZE   128

const char* play_tooltips[] = { // ¶ = square | ¹ = triangle
    "O: Back",
    "X: Pause",
    "¶: Loop",
    "R: Next",
    "L: Rewind/Previous",
};

struct PlayState_PlaylistTrack {
    char fileName[257];
    PlayState_PlaylistTrack * next;
    PlayState_PlaylistTrack * prev;
};

typedef struct {
    char *currentLabel;
    char *currentPath;
    Texture currentTexture;
    Texture currentSpecularTexture;
    MP3Instance * mp3Instance;

    PlayState_PlaylistTrack * first;
    PlayState_PlaylistTrack * last;
    PlayState_PlaylistTrack * current;
    int count;

    bool wasPressing;
    bool firstFrame;
    bool looping;
    bool showLoopingTooltip;
    double rotationTimer;
    float disappearing;
    float rotation;
    float loopingY;
    Model playModel;
    Texture playTexture[2];
    Texture noCover[2];
    AppState * parent;
} PlayState;

void play_unloadPlaylist(AppState &appState) {
    PlayState * playState = (PlayState *) appState.currentViewData;
    if (playState->first == nullptr) return;
    PlayState_PlaylistTrack * track = playState->first;
    while (track != nullptr) {
        PlayState_PlaylistTrack * next = track->next;
        free(track);
        track = next;
    }
    playState->first = nullptr;
    playState->last = nullptr;
    playState->current = nullptr;
    playState->count = 0;
}

void play_loadPlaylist(AppState &appState) {
    PlayState * playState = (PlayState *) appState.currentViewData;
    play_unloadPlaylist(appState);
    char * basePath = appState.toPlayFolder;
    DIR * dir = opendir(basePath);
    struct dirent *entry;
    playState->count = 0;
    PlayState_PlaylistTrack * lastAdded = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) continue;
        if (!isFilePlayable(entry->d_name)) continue;
        PlayState_PlaylistTrack * track = (PlayState_PlaylistTrack *) malloc(sizeof(PlayState_PlaylistTrack));
        memset(track, 0, sizeof(PlayState_PlaylistTrack));
        strncpy(track->fileName, entry->d_name, 256);
        track->fileName[256] = '\0';
        if (playState->first == nullptr) {
            playState->first = track;
        } else {
            lastAdded->next = track;
        }

        track->prev = lastAdded;
        lastAdded = track;
        playState->count++;
        TraceLog(LOG_INFO, "]%s", track->fileName);
    }
    closedir(dir);
    playState->last = lastAdded;

    if (appState.toPlayFile[0] != 0) {
        PlayState_PlaylistTrack * currentNode = playState->first;
        for (int i = 0; i < playState->count; i++) {
            if (strcmp(currentNode->fileName, appState.toPlayFile) == 0) {
                strcpy(currentNode->fileName, playState->first->fileName);
                strcpy(playState->first->fileName, appState.toPlayFile);
                break;
            }
            currentNode = currentNode->next;
        }
    }
    playState->current = playState->first;

    TraceLog(LOG_INFO, "Loaded %d tracks:", playState->count);
}

void play_makeTexture(PlayState * playState, const char* fileName) {
    int8_t c = 4;
    uint8_t specBuffer[PLAY_TEXTURE_SIZE*PLAY_TEXTURE_SIZE*4];
    playState->currentTexture = readPictureFromFileMP3(fileName, playState->parent, PLAY_TEXTURE_SIZE, specBuffer, &c);
    if (playState->currentTexture.id == 0 || c <= 2 || c > 4) {
        return;
    }
    double start = GetTime();
    flip_fade_texture(specBuffer, PLAY_TEXTURE_SIZE, PLAY_TEXTURE_SIZE, c, playState->parent);
    Image img;
    img.data = specBuffer;
    img.width = PLAY_TEXTURE_SIZE;
    img.height = PLAY_TEXTURE_SIZE;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    playState->currentSpecularTexture = LoadTextureFromImage(img);
    TraceLog(LOG_INFO, "Made specular in %f", (GetTime()-start)*1000);
}

void play_beginPlayback(PlayState * playState) {
    if (playState->currentTexture.id != 0) {
        UnloadTexture(playState->currentTexture);
        playState->currentTexture.id = 0;
    }
    if (playState->currentSpecularTexture.id != 0) {
        UnloadTexture(playState->currentSpecularTexture);
        playState->currentSpecularTexture.id = 0;
    }
    strcpy(playState->currentPath, playState->parent->toPlayFolder);
    strcat(playState->currentPath, playState->current->fileName);
    TraceLog(LOG_INFO, "Begin %s", playState->currentPath);
    strcpy(playState->currentLabel, playState->current->fileName);
    if (char* ptr = strrchr(playState->currentLabel, '.'); ptr != nullptr) *ptr = 0;
    play_makeTexture(playState, playState->currentPath);
    playState->rotationTimer = 0;
    playState->firstFrame = true;
    mp3_stopPlayback(playState->mp3Instance);
    playState->mp3Instance = mp3_beginPlayback(playState->currentPath);
}

void play_init(AppState &appState) {
    BeginDrawing();
    ClearBackground(appState.bgColor);
    EndDrawing();
    appState.currentViewData = malloc(sizeof(PlayState));
    memset(appState.currentViewData, 0, sizeof(PlayState));
    PlayState * playState = (PlayState *) appState.currentViewData;
    *playState = PlayState{};
    playState->parent = &appState;
    playState->currentLabel = (char *) malloc(257);
    playState->currentPath = (char *) malloc(257*2);
    playState->currentPath[0] = 0;
    playState->currentLabel[0] = 0;
    playState->loopingY = -40;
    playState->disappearing = 400;
    playState->currentTexture = {.id = 0};
    scePowerSetClockFrequency(333, 333, 166);
    play_loadPlaylist(appState);
    appState.isMenu = 0;
    playState->wasPressing = false;
    play_makeTexture(playState, "assets/cover.mp3");
    playState->noCover[0] = playState->currentTexture;
    playState->noCover[1] = playState->currentSpecularTexture;
    playState->currentTexture = {.id = 0};
    playState->currentSpecularTexture = {.id = 0};
    playState->playModel = LoadModel("assets/player.glb");
    playState->playTexture[0] = playState->playModel.materials[1].maps[MATERIAL_MAP_DIFFUSE].texture;
    playState->playTexture[1] = playState->playModel.materials[2].maps[MATERIAL_MAP_DIFFUSE].texture;
    play_beginPlayback(playState);
}

void play_destroy(AppState &appState) {
    PlayState * playState = (PlayState *) appState.currentViewData;
    mp3_stopPlayback(playState->mp3Instance);
    if (playState->currentTexture.id != 0) {
        UnloadTexture(playState->currentTexture);
        playState->currentTexture.id = 0;
    }
    if (playState->currentSpecularTexture.id != 0) {
        UnloadTexture(playState->currentSpecularTexture);
        playState->currentSpecularTexture.id = 0;
    }
    UnloadTexture(playState->noCover[0]);
    UnloadTexture(playState->noCover[1]);
    playState->playModel.materials[1].maps[MATERIAL_MAP_DIFFUSE].texture = playState->playTexture[0];
    playState->playModel.materials[2].maps[MATERIAL_MAP_DIFFUSE].texture = playState->playTexture[1];
    UnloadModelFull(playState->playModel);
    play_unloadPlaylist(appState);
    free(playState->currentLabel);
    free(playState->currentPath);
    free(playState);
    appState.currentViewData = nullptr;
    scePowerSetClockFrequency(222, 222, 111);
}


void play_nextTrack(PlayState * playState) {
    if (playState->current == nullptr) return;
    playState->current = playState->current->next;
    if (playState->current == nullptr) playState->current = playState->first;
    play_beginPlayback(playState);
}

void play_prevTrack(PlayState * playState) {
    if (playState->current == nullptr) return;
    playState->current = playState->current->prev;
    if (playState->current == nullptr) playState->current = playState->last;
    play_beginPlayback(playState);
}

int play_handle(AppState &appState) {
    PlayState * playState = (PlayState *) appState.currentViewData;
    int rVal = RETURN_STATE_CONTINUE;
    if (appState.controller.o) {
        rVal = RETURN_STATE_HOME;
    }

    if (mp3_tick(playState->mp3Instance)) {
        if (playState->looping) {
            play_beginPlayback(playState);
        } else {
            play_nextTrack(playState);
        }
    }

    if (!playState->firstFrame) {
        playState->rotationTimer+=GetFrameTime();
    }
    playState->firstFrame = false;

    if (appState.controller.ltrigger) {
        if (!playState->wasPressing) {
            if (playState->mp3Instance == nullptr || playState->mp3Instance->numPlayed/playState->mp3Instance->samplingRate < 5) {
                play_prevTrack(playState);
            } else {
                play_beginPlayback(playState);
            }
        }
        playState->wasPressing = true;
    } else if (appState.controller.rtrigger) {
        if (!playState->wasPressing) play_nextTrack(playState);
        playState->wasPressing = true;
    } else if (appState.controller.x) {
        if (!playState->wasPressing && playState->mp3Instance != nullptr) playState->mp3Instance->paused = !playState->mp3Instance->paused; 
        playState->wasPressing = true;
    } else if (appState.controller.s) {
        if (!playState->wasPressing) {
            playState->looping = !playState->looping;
            playState->loopingY = -40;
            playState->showLoopingTooltip = true;
        }
        playState->wasPressing = true;
    } else {
        playState->wasPressing = false;
    }

    playState->playModel.materials[1].maps[MATERIAL_MAP_DIFFUSE].texture = playState->currentTexture.id == 0 ? playState->noCover[0] : playState->currentTexture;
    playState->playModel.materials[2].maps[MATERIAL_MAP_DIFFUSE].texture = playState->currentSpecularTexture.id == 0 ? playState->noCover[1] : playState->currentSpecularTexture;
    float rot = 0;
    if (playState->rotationTimer < 30) {
        playState->rotation = 0;
    } else {
        playState->rotation += GetFrameTime()*25;
        rot = playState->rotation;
        if (playState->rotation > 180) {
            playState->rotation = 0;
            playState->rotationTimer = 0;
            rot = 0;
        }
    }
    rot+=30;

    constexpr Camera3D cam = {
        {0.0f, 0.0f, 4.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        45,
    };
    BeginMode3D(cam);
    DrawModelEx(playState->playModel, {-1.8f, 0, 0}, {0, 1, 0}, rot, {0.5f, 0.5f, 0.5f}, WHITE);
    DrawModelEx(playState->playModel, {-1.8f, 0, 0}, {0, 1, 0}, rot-180, {0.5f, 0.5f, 0.5f}, WHITE);
    EndMode3D();

    char* titleLabel = playState->currentLabel;
    char* authorLabel = nullptr;
    if (playState->mp3Instance != nullptr) {
        if (playState->mp3Instance->title[0] != 0) titleLabel = playState->mp3Instance->title;
        if (playState->mp3Instance->author[0] != 0) {
            authorLabel = titleLabel;
            titleLabel = playState->mp3Instance->author;
        }
    }
    if (authorLabel != nullptr) DrawTextEx(appState.font, authorLabel, {150, ATTR_PSP_HEIGHT/2-16*2-10}, 20, 0, WHITE);
    if (titleLabel != nullptr) DrawTextEx(appState.font, titleLabel, {150, ATTR_PSP_HEIGHT/2-16-10}, 20, 0, (authorLabel == nullptr) ? WHITE : GRAY);
    DrawRectangle(150, ATTR_PSP_HEIGHT/2-5, 250, 5, WHITE);
    if (playState->mp3Instance != nullptr) {
        DrawRectangle(150, ATTR_PSP_HEIGHT/2-5, (float)playState->mp3Instance->numPlayed/(float)playState->mp3Instance->totalSamples*250.f, 5, SKYBLUE);
        char elapsed[16];
        char total[16];
        const int elapsedS = playState->mp3Instance->numPlayed/playState->mp3Instance->samplingRate;
        const int totalS = playState->mp3Instance->totalSamples/playState->mp3Instance->samplingRate;
        if (totalS >= 60) snprintf(elapsed, 16, "%d:%02d", elapsedS/60, elapsedS%60); else snprintf(elapsed, 16, "%d", elapsedS);
        if (totalS >= 60) snprintf(total, 16, "%d:%02d", totalS/60, totalS%60); else snprintf(total, 16, "%d", totalS);
        const char* timeLabel = TextFormat("%s/%s", elapsed, total);
        DrawTextEx(appState.font, timeLabel, {400-MeasureTextEx(appState.font, timeLabel, 15, 0).x, ATTR_PSP_HEIGHT/2-8-10}, 15, 0, LIGHTGRAY);
    }

    if (playState->showLoopingTooltip) {
        float loopingTooltip = fabsf(playState->loopingY);
        if (loopingTooltip > 100) loopingTooltip = 0;
        const char* txt = TextFormat("Looping %s  ", playState->looping ? "ON" : "OFF");
        DrawTextEx(appState.font, txt, {ATTR_PSP_WIDTH-MeasureTextEx(appState.font, txt, 20, 0).x, ATTR_PSP_HEIGHT-20+loopingTooltip}, 20, 0, GRAY);

        if (playState->loopingY < -1000) {
            playState->loopingY += GetFrameTime()*1000;
            if (playState->loopingY > -8000) playState->loopingY = 0;
        }
        if (playState->loopingY < 0) {
            playState->loopingY += GetFrameTime()*100;
            if (playState->loopingY >= 0) playState->loopingY = -10000;
        }
        if (playState->loopingY >= 0) {
            playState->loopingY += GetFrameTime()*100;
            if (playState->loopingY >= 40) {
                playState->loopingY = -40;
                playState->showLoopingTooltip = false;
            }
        }
    }

    if (playState->disappearing > -100) {
        for (int i = sizeof(play_tooltips)/sizeof(play_tooltips[0])-1; i >= 0; i--) {
            DrawTextEx(appState.font, play_tooltips[i], {470-MeasureTextEx(appState.font, play_tooltips[i], 20, 0).x, 250-((float)i*20)-(playState->disappearing >= 0 ? 0 : playState->disappearing)}, 20, 0, LIGHTGRAY);
        }
        playState->disappearing -= GetFrameTime()*100;
    }
    
    return rVal;
}