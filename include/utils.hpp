#pragma once

#include <raylib.h>
#include <ctype.h>
#include <cstring>
#include <stb_image.h>
#include <pspjpeg.h> 
#include <psputility_modules.h>

extern "C" {
    __asm__("	.global _DisableFPUExceptions\n"
    "    .set push \n"
    "    .set noreorder \n"
    "_DisableFPUExceptions: \n"
    "    cfc1    $2, $31 \n"
    "    lui     $8, 0x80 \n"
    "    and     $8, $2, $8\n"
    "    ctc1    $8, $31 \n"
    "    jr      $31 \n"
    "    nop \n"
    "    .set pop \n");
    void _DisableFPUExceptions();
}

void UnloadModelFull(Model &model) {
    if (model.meshes == nullptr) {
        TraceLog(LOG_WARNING, "ModelFull double free attempted");
        return;
    }
    for (int i = 0; i < model.materialCount; ++i) UnloadMaterial(model.materials[i]);
    model.materialCount = 0;
    UnloadModel(model);
    model.meshes = nullptr;
}

void utf8_to_ascii(const char* utf8_input, char* ascii_output, size_t output_size) {
    if (utf8_input == NULL || ascii_output == NULL || output_size == 0) return;
    size_t output_index = 0;
    const unsigned char* input = (const unsigned char*)utf8_input;
    while (*input != '\0' && output_index < output_size - 1) {
        if ((*input & 0x80) == 0) {
            ascii_output[output_index++] = *input;
            input++;
        } else {
            ascii_output[output_index++] = '?';
            if ((*input & 0xE0) == 0xC0) {
                input += 2;
            } else if ((*input & 0xF0) == 0xE0) {
                input += 3;
            } else if ((*input & 0xF8) == 0xF0) {
                input += 4;
            } else {
                input++;
            }
        }
    }
    ascii_output[output_index] = '\0';
}

int LoadStartModule(const char *path) {
    uint32_t loadResult;
    uint32_t startResult;
    int status;

    loadResult = sceKernelLoadModule(path, 0, nullptr);
    if (loadResult & 0x80000000) {
        return -1;
    } else {
        startResult = sceKernelStartModule(loadResult, 0, nullptr, &status, nullptr);
    }

    if (loadResult != startResult) {
        return -2;
    }
    return 0;
}

bool StartJpegPsp() {
    //if (LoadStartModule("flash0:/kd/avcodec.prx") != 0) return false;
    if (sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC) != 0) return false;
    if (sceJpegInitMJpeg() != 0) return false;
    return true;
}

void StopJpegPsp() {
    sceJpegFinishMJpeg();
}

unsigned char* resize_image_to_x_y(unsigned char* dst_data, const unsigned char* src_data, int src_width, int src_height, int bytes_per_pixel, int dst_width, int dst_height) {
    float x_ratio = (float) src_width / dst_width;
    float y_ratio = (float) src_height / dst_height;

    for (int y = 0; y < dst_height; y++) {
        for (int x = 0; x < dst_width; x++) {
            int src_x = (int) (x * x_ratio);
            int src_y = (int) (y * y_ratio);
            int dst_index = (y * dst_width + x) * bytes_per_pixel;
            int src_index = (src_y * src_width + src_x) * bytes_per_pixel;
            memcpy(&dst_data[dst_index], &src_data[src_index], bytes_per_pixel);
        }
    }

    return dst_data;
}

inline bool lowerCaseStrEq(const char* a, const char *b) {
    while (*a && tolower(*a) == tolower(*b) && *b) {
        a++;
        b++;
    }
    return tolower(*a) == tolower(*b);
}

int syncsafe_to_int(unsigned char bytes[4]) {
    return (bytes[0] << 21) | (bytes[1] << 14) | (bytes[2] << 7) | bytes[3];
}

int be_to_int(unsigned char bytes[4]) {
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

void max_alpha(unsigned char* data, const int width, const int height) {
    if (!data) return;
    for (long i = 0; i < width * height*4; i+=4) {
        data[i+3] = 255;
    }
}

#define USE_DECODER_SONY    true
Texture readPictureFromFileMP3(const char * filePath, AppState * appState, const int resizeAs = 128, uint8_t* copyTo = nullptr, int8_t* channelsRequired = nullptr) {
    Texture tex = {.id = 0};
    FILE *fp = fopen(filePath, "rb");
    if (!fp) {
        TraceLog(LOG_WARNING, "Unable to open file %s", filePath);
        return {.id = 0};
    }

    unsigned char header[10];
    if (fread(header, 1, 10, fp) != 10 || memcmp(header, "ID3", 3) != 0) {
        fclose(fp);
        return {.id = 0};
    }

    int tag_size = syncsafe_to_int(&header[6]);
    long tag_end = 10 + tag_size;

    while (ftell(fp) < tag_end) {
        unsigned char frame_header[10];
        if (fread(frame_header, 1, 10, fp) != 10) break;
        if (frame_header[0] == 0) break;
        char frame_id[5] = {0};
        memcpy(frame_id, frame_header, 4);
        int frame_size = be_to_int(&frame_header[4]);
        if (memcmp(frame_id, "APIC", 4) == 0) {
            unsigned char *data = (unsigned char*) malloc(frame_size);
            if (!data) {
                TraceLog(LOG_ERROR, "Memory allocation failed %d", frame_size);
                fclose(fp);
                return {.id = 0};
            }
            fread(data, 1, frame_size, fp);
            int pos = 0;
            unsigned char encoding = data[pos++];
            char *mime = (char*) &data[pos];
            int mime_len = strlen(mime);
            pos += mime_len + 1;
            unsigned char pic_type = data[pos++];
            char *desc = (char*) &data[pos];
            int desc_len = strlen(desc);
            pos += desc_len + 1;
            int image_data_size = frame_size - pos;
            TraceLog(LOG_INFO, "Found image: (t enc: %d)", encoding);
            TraceLog(LOG_INFO, "  MIME Type: %s", mime);
            TraceLog(LOG_INFO, "  Picture Type: %d", pic_type);
            TraceLog(LOG_INFO, "  Description: %s", desc);
            TraceLog(LOG_INFO, "  Image Data Size: %d bytes", image_data_size);

            char* fC = strrchr(mime, '/');
            if (fC != nullptr) *fC = '.';
            bool isJpg = false;
            const int dstX = resizeAs, dstY = resizeAs;
            unsigned char resizedBuf[dstX*dstY*4] __attribute__((aligned(16)));
            Image img = {.data = nullptr};
            if (fC != nullptr) {
                isJpg = lowerCaseStrEq(fC, ".jpg") || lowerCaseStrEq(fC, ".jpeg");
            }
            if (!isJpg) {
                img = LoadImageFromMemory(mime, &data[pos], image_data_size); 
                ImageResizeNN(&img, dstX, dstY);
            } else {
                int w, h, c;
                unsigned char* imgData = nullptr;
                double start = GetTime();
                if (USE_DECODER_SONY && appState->jpegWorking) {
                    c = 4;

                    int w = 0, h = 0;
                    const uint8_t * buf = &data[pos];
                    for (int i = 2; i < image_data_size;) {
                        if (buf[i] == 0xFF) {
                            i++;
                            switch(buf[i]){
                                case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC5: case 0xC6: case 0xC7: case 0xC9: case 0xCA: case 0xCB: case 0xCD: case 0xCE: case 0xCF:
                                    i += 4;
                                    h = (buf[i] << 8) | (buf[i+1]);
                                    w = (buf[i+2] << 8) | (buf[i+3]);
                                    i = image_data_size; break;
                                case 0xDA: case 0xD9: break;
                                default:
                                    i += ((buf[i+1] << 8) | (buf[i+2])) + 1;
                                    break;
                            }
                        } else i++;
                    }
                    if (w <= 0 || h <= 0 || sceJpegCreateMJpeg(w, h) != 0) {
                        TraceLog(LOG_ERROR, "JPEG error %d %d", w, h);
                        goto retry;
                    }
                    const uint32_t bufferSize = c * w * h;
                    unsigned char* rgbabuf = (unsigned char*) malloc(bufferSize);
                    if (rgbabuf == nullptr) {
                        TraceLog(LOG_ERROR, "JPEG Oom");
                        sceJpegDeleteMJpeg();
                        free(rgbabuf);
                        goto retry;
                    }
                    int res = sceJpegDecodeMJpeg(&data[pos], image_data_size, rgbabuf, 0);
                    sceJpegDeleteMJpeg();
                    if (res < 0) {
                        TraceLog(LOG_INFO, "JPEG Decoding Error: %d", res);
                        free(rgbabuf);
                        goto retry;
                    }
                    TraceLog(LOG_INFO, "JPEG Decoding %d %d: %f", w, h, (GetTime() - start) * 1000);
                    start = GetTime();
                    img.data = resize_image_to_x_y(resizedBuf, rgbabuf, w, h, c, dstX, dstY);
                    free(rgbabuf);
                    max_alpha((uint8_t *) img.data, dstX, dstY);
                    TraceLog(LOG_INFO, "(hard) JPEG Resizing and conversion: %f", (GetTime() - start) * 1000);
                } else {
                    retry:
                    imgData = stbi_load_from_memory(&data[pos], image_data_size, &w, &h, &c, 0);
                    if (imgData == nullptr || c > 4) {
                        free(data);
                        goto error;
                    }
                    TraceLog(LOG_INFO, "JPEG Decoding %d %d: %f", w, h, (GetTime() - start) * 1000);
                    start = GetTime();
                    img.data = resize_image_to_x_y(resizedBuf, imgData, w, h, c, dstX, dstY);
                    TraceLog(LOG_INFO, "(soft) JPEG Resizing: %f", (GetTime() - start) * 1000);
                    stbi_image_free(imgData);
                }

                if (img.data == nullptr) {
                    free(data);
                    goto error;
                }
                img.width = dstX;
                img.height = dstY;
                img.mipmaps = 1;
                if (c == 1) img.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
                else if (c == 2) img.format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
                else if (c == 3) img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
                else if (c == 4) img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            }

            free(data);
            tex = LoadTextureFromImage(img);
            if (copyTo != nullptr) {
                if (channelsRequired != nullptr) {
                    int8_t c = 0;
                    if (img.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) c = 0;
                    else if (img.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) c = 1;
                    else if (img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) c = 3;
                    else if (img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) c = 4;
                    else {
                        TraceLog(LOG_ERROR, "Unsupported image format %d, stack would be destroyed", img.format);
                        *channelsRequired = -1;
                        goto unload;
                    }
                    memcpy(copyTo, img.data, img.width * img.height * c);
                    *channelsRequired = c;
                }
            }
            unload:
            if (!isJpg) UnloadImage(img);
            break;
        } else {
            fseek(fp, frame_size, SEEK_CUR);
        }
    }

    if (tex.id == 0) {
        TraceLog(LOG_INFO, "No picture %s", filePath);
    }
    fclose(fp);
    return tex;
    error:
    TraceLog(LOG_ERROR, "JPEG Decoding failed!");
    fclose(fp);
    return tex;
}

bool isFilePlayable(char* file) {
    const int len = strlen(file);
    if (len > 4) {
        if (lowerCaseStrEq(file + len - 4, ".mp3")) {
            return true;            
        }
    }
    return false;
}

inline Color colorEqualise(Color origin, Color target, int percent, bool alpha = false) {
    float r = (float) origin.r + (((float) target.r - (float) origin.r) / 100) * (float) percent;
    float g = (float) origin.g + (((float) target.g - (float) origin.g) / 100) * (float) percent;
    float b = (float) origin.b + (((float) target.b - (float) origin.b) / 100) * (float) percent;
    float a = origin.a;
    if (alpha) a = (float) origin.a + (((float) target.a - (float) origin.a) / 100) * (float) percent;
    r = (r > 255) ? 255 : r;
    g = (g > 255) ? 255 : g;
    b = (b > 255) ? 255 : b;
    a = (a > 255) ? 255 : a;
    r = (r < 0) ? 0 : r;
    g = (g < 0) ? 0 : g;
    b = (b < 0) ? 0 : b;
    a = (a < 0) ? 0 : a;
    return {(uint8_t) r, (uint8_t) g, (uint8_t) b, (uint8_t) a};
}

void convert_rgb_to_rgba_in_place(unsigned char *buffer, size_t width, size_t height) {
    size_t total_pixels = width * height;
    for (size_t i = total_pixels - 1; i >= 0; --i) {
        size_t src_index = i * 3;
        size_t dst_index = i * 4;
        buffer[dst_index + 0] = buffer[src_index + 0];
        buffer[dst_index + 1] = buffer[src_index + 1];
        buffer[dst_index + 2] = buffer[src_index + 2];
        buffer[dst_index + 3] = 255;
    }
}

void flip_fade_texture(uint8_t *pixels, int width, int height, int bytes_per_pixel, AppState * appState) {
    const int row_size = width * bytes_per_pixel;
    uint8_t row_buffer[row_size];

    for (int y = 0; y < height / 2; ++y) {
        uint8_t *row_top = pixels + y * row_size;
        uint8_t *row_bottom = pixels + (height - 1 - y) * row_size;
        memcpy(row_buffer, row_top, row_size);
        memcpy(row_top, row_bottom, row_size);
        memcpy(row_bottom, row_buffer, row_size);
    }

    if (bytes_per_pixel == 3) convert_rgb_to_rgba_in_place(pixels, width, height); else max_alpha(pixels, width, height);

    Color *pixelsRGBA = (Color *) pixels;
    const int fade_height = height / 2;
    for (int y = 0; y < height; y++) {
        if (y >= fade_height) {
            for (int x = 0; x < width; x++) pixelsRGBA[y * width + x] = appState->bgColor;
            continue;
        }
        float currentAlpha = ((float) y / fade_height) * 100 + 25.0f;
        if (currentAlpha >= 100.0f) currentAlpha = 100;
        for (int x = 0; x < width; x++) pixelsRGBA[y * width + x] = colorEqualise(pixelsRGBA[y * width + x], appState->bgColor, currentAlpha, true);
    }
}
