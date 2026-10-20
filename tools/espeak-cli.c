/* espeak-cli: tiny command-line front end for espeak.dll (1.48.15).
   Writes the speech to a 16-bit mono WAV file.  Unicode command line.
   Usage: espeak-cli [-v voice] [-s rate] [-p pitch] [-a volume] [-w out.wav] [-f textfile] ["text"]
   espeak.dll must be in the same folder; espeak-data in the parent folder. */
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*Init_t)(int, int, const char *, int);
typedef void (*SetCb_t)(int (*)(short *, int, void *));
typedef int (*SetVoice_t)(const char *);
typedef int (*Synth_t)(const void *, size_t, unsigned int, int, unsigned int, unsigned int, unsigned int *, void *);
typedef int (*Sync_t)(void);
typedef int (*SetParam_t)(int, int, int);
typedef int (*Term_t)(void);

static short *g_buf = NULL;
static size_t g_len = 0, g_cap = 0;

static int callback(short *wav, int numsamples, void *events)
{
    if (wav == NULL) return 0;
    if (g_len + numsamples > g_cap) {
        g_cap = (g_len + numsamples) * 2 + 65536;
        g_buf = (short *)realloc(g_buf, g_cap * sizeof(short));
        if (!g_buf) return 1;
    }
    memcpy(g_buf + g_len, wav, numsamples * sizeof(short));
    g_len += numsamples;
    return 0;
}

static char *to_utf8(const wchar_t *w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = (char *)malloc(n);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    return s;
}

static void put32(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc((v >> 24) & 255, f); }
static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

int wmain(int argc, wchar_t **argv)
{
    const wchar_t *voice = L"fa", *out = L"out.wav", *file = NULL;
    int rate = -1, pitch = -1, vol = -1;
    char *text = NULL;
    int i;
    for (i = 1; i < argc; i++) {
        if (!wcscmp(argv[i], L"-v") && i + 1 < argc) voice = argv[++i];
        else if (!wcscmp(argv[i], L"-w") && i + 1 < argc) out = argv[++i];
        else if (!wcscmp(argv[i], L"-f") && i + 1 < argc) file = argv[++i];
        else if (!wcscmp(argv[i], L"-s") && i + 1 < argc) rate = _wtoi(argv[++i]);
        else if (!wcscmp(argv[i], L"-p") && i + 1 < argc) pitch = _wtoi(argv[++i]);
        else if (!wcscmp(argv[i], L"-a") && i + 1 < argc) vol = _wtoi(argv[++i]);
        else { text = to_utf8(argv[i]); }
    }
    if (file) {
        FILE *tf = _wfopen(file, L"rb");
        if (!tf) { fwprintf(stderr, L"cannot open %ls\n", file); return 2; }
        fseek(tf, 0, SEEK_END); long sz = ftell(tf); fseek(tf, 0, SEEK_SET);
        char *raw = (char *)malloc(sz + 1);
        fread(raw, 1, sz, tf); raw[sz] = 0; fclose(tf);
        text = raw;
        if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) text += 3;
    }
    if (!text) {
        fprintf(stderr, "espeak-cli 1.48.15\nUsage: espeak-cli [-v voice] [-s rate] [-p pitch] [-a volume] [-w out.wav] [-f textfile] [\"text\"]\n");
        return 1;
    }

    wchar_t dir[MAX_PATH], dll[MAX_PATH], parent[MAX_PATH], shortp[MAX_PATH];
    GetModuleFileNameW(NULL, dir, MAX_PATH);
    wchar_t *slash = wcsrchr(dir, L'\\'); if (slash) *slash = 0;
    swprintf(dll, MAX_PATH, L"%ls\\espeak.dll", dir);
    wcscpy(parent, dir);
    slash = wcsrchr(parent, L'\\'); if (slash) *slash = 0;
    if (!GetShortPathNameW(parent, shortp, MAX_PATH)) wcscpy(shortp, parent);
    char data[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, shortp, -1, data, MAX_PATH, NULL, NULL);

    HMODULE h = LoadLibraryW(dll);
    if (!h) { fwprintf(stderr, L"cannot load %ls\n", dll); return 3; }
    Init_t init = (Init_t)GetProcAddress(h, "espeak_Initialize");
    SetCb_t setcb = (SetCb_t)GetProcAddress(h, "espeak_SetSynthCallback");
    SetVoice_t setvoice = (SetVoice_t)GetProcAddress(h, "espeak_SetVoiceByName");
    Synth_t synth = (Synth_t)GetProcAddress(h, "espeak_Synth");
    Sync_t sync = (Sync_t)GetProcAddress(h, "espeak_Synchronize");
    SetParam_t setparam = (SetParam_t)GetProcAddress(h, "espeak_SetParameter");
    Term_t term = (Term_t)GetProcAddress(h, "espeak_Terminate");
    if (!init || !setcb || !setvoice || !synth || !sync || !setparam || !term) { fprintf(stderr, "espeak.dll is missing functions\n"); return 3; }

    int sr = init(2 /* AUDIO_OUTPUT_SYNCHRONOUS */, 0, data, 0);
    if (sr <= 0) { fprintf(stderr, "espeak_Initialize failed (espeak-data not found in %s)\n", data); return 4; }
    setcb(callback);
    char *v8 = to_utf8(voice);
    if (setvoice(v8) != 0) { fprintf(stderr, "voice '%s' not found\n", v8); return 5; }
    if (rate > 0) setparam(1, rate, 0);
    if (vol >= 0) setparam(2, vol, 0);
    if (pitch >= 0) setparam(3, pitch, 0);
    synth(text, strlen(text) + 1, 0, 1 /* POS_CHARACTER */, 0, 1 /* espeakCHARS_UTF8 */, NULL, NULL);
    sync();
    term();

    FILE *f = _wfopen(out, L"wb");
    if (!f) { fwprintf(stderr, L"cannot write %ls\n", out); return 6; }
    unsigned bytes = (unsigned)(g_len * 2);
    fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 1); put32(f, sr); put32(f, sr * 2); put16(f, 2); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, bytes);
    if (g_len) fwrite(g_buf, 2, g_len, f);
    fclose(f);
    wprintf(L"wrote %ls (%u samples, %d Hz)\n", out, (unsigned)g_len, sr);
    return 0;
}
