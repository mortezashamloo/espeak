/* espeak-voices: registers / removes the eSpeak SAPI5 voice tokens.
   A port of the [Code] section of the official espeak installer script (make_espeak.iss):
   same registry layout (Tokens\eSpeak, eSpeak_1, eSpeak_2 ...), same language-code table, same
   PhoneConverter handling.  It can write both the 32-bit and the 64-bit registry view.

   espeak-voices install "<voice list>" "<install dir>" [x64]
   espeak-voices remove
   espeak-voices lcid <voice>            (prints the Windows language code, for testing)
*/
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define RV1 "Software\\Microsoft\\Speech\\Voices\\Tokens\\eSpeak"
#define RPC "Software\\Microsoft\\Speech\\PhoneConverters\\Tokens"
#define CLSID_ENGINE "{BE985C8D-BE32-4A22-AA93-55C16A6D1D91}"

static REGSAM g_view = 0;

/* ---------------- registry helpers (view selected by g_view) ---------------- */
static int reg_write(const char *key, const char *name, const char *val)
{
    HKEY k; DWORD d; LONG r;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, key, 0, NULL, 0, KEY_WRITE | g_view, NULL, &k, &d)) return 0;
    r = RegSetValueExA(k, (name && *name) ? name : NULL, 0, REG_SZ, (const BYTE *)val, (DWORD)strlen(val) + 1);
    RegCloseKey(k);
    return r == 0;
}

static int reg_read(const char *key, const char *name, char *buf, DWORD size)
{
    HKEY k; DWORD type = 0, n = size - 1; LONG r;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ | g_view, &k)) return 0;
    r = RegQueryValueExA(k, (name && *name) ? name : NULL, NULL, &type, (BYTE *)buf, &n);
    RegCloseKey(k);
    if (r != 0 || type != REG_SZ) return 0;
    buf[n < size ? n : size - 1] = 0;
    return 1;
}

static int key_exists(const char *key)
{
    HKEY k;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ | g_view, &k)) return 0;
    RegCloseKey(k);
    return 1;
}

static void key_delete_tree(const char *path)
{
    char parent[512]; const char *leaf; char *slash; HKEY p;
    strncpy(parent, path, sizeof(parent) - 1); parent[sizeof(parent) - 1] = 0;
    slash = strrchr(parent, '\\');
    if (!slash) return;
    *slash = 0; leaf = slash + 1;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, parent, 0, KEY_ALL_ACCESS | g_view, &p)) return;
    RegDeleteTreeA(p, leaf);
    RegDeleteKeyExA(p, leaf, g_view, 0);
    RegCloseKey(p);
}

/* ---------------- language codes ---------------- */
struct lc { const char *code; const char *hex; };
static const struct lc LANGS[] = {
    {"af","436"},{"am","45E"},{"an","40A"},{"as","44D"},{"az","42C"},{"bg","402"},{"bn","445"},
    {"bs","41A"},{"ca","403"},{"cs","405"},{"cy","452"},{"da","406"},{"de","407"},{"dv","465"},
    {"el","408"},{"en","409"},{"es","40A"},{"et","425"},{"eu","42D"},{"fa","429"},{"fi","40B"},
    {"fil","464"},{"fr","40C"},{"ga","83C"},{"gd","43C"},{"gu","447"},{"hi","439"},{"hr","41A"},
    {"hu","40E"},{"hy","42B"},{"id","421"},{"is","40F"},{"it","410"},{"ka","437"},{"kk","43F"},
    {"kn","44B"},{"ko","412"},{"lt","427"},{"lv","426"},{"mk","42F"},{"ml","44C"},{"mn","450"},
    {"ms","43E"},{"mt","43A"},{"my","455"},{"ne","461"},{"nl","413"},{"no","414"},{"or","448"},
    {"pa","446"},{"pl","415"},{"pt","416"},{"ro","418"},{"ru","419"},{"rw","487"},{"si","45B"},
    {"sk","41B"},{"sl","424"},{"sq","41C"},{"sr","81A"},{"sv","41D"},{"sw","441"},{"ta","449"},
    {"te","44A"},{"tg","45E"},{"tl","464"},{"tn","432"},{"tr","41F"},{"tt","444"},{"ur","420"},
    {"vi","42A"},{"wo","488"},{"zh","804"},
    /* mbrola voices */
    {"-af","436"},{"-br","416"},{"-ca","C0C"},{"-cr","41A"},{"-cz","405"},{"-de","407"},
    {"-en","809"},{"-es","40A"},{"-fr","40C"},{"-gr","408"},{"-hu","40E"},{"-ic","40F"},
    {"-in","439"},{"-ir","429"},{"-it","410"},{"-mx","80A"},{"-nl","413"},{"-pl","415"},
    {"-pt","816"},{"-ro","418"},{"-sw","41D"},{"-us","409"},{"-vz","200A"},
    {NULL, NULL}
};

static const char *language_from_voice(const char *voice)
{
    char lang1[16]; size_t len = strlen(voice); int i;
    memset(lang1, 0, sizeof(lang1));
    strncpy(lang1, voice, 2);                         /* first two characters */
    if (!_stricmp(lang1, "mb")) {                     /* mbrola: mb-de4-en etc. */
        memset(lang1, 0, sizeof(lang1));
        if (len > 2) strncpy(lang1, voice + 2, 3);
        if (len > 8) { memset(lang1, 0, sizeof(lang1)); strncpy(lang1, voice + 7, 6); }
    }
    if (!_stricmp(voice, "pt-pt")) return "816";
    for (i = 0; LANGS[i].code; i++)
        if (!_stricmp(LANGS[i].code, lang1)) return LANGS[i].hex;
    return "409";                                     /* default: en-us */
}

/* ---------------- phone converters ---------------- */
static int check_phone_convertors(const char *lcode)
{
    HKEY k; char name[256]; DWORD i, n; size_t len = strlen(lcode); int result = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, RPC, 0, KEY_READ | g_view, &k)) return 0;
    for (i = 0;; i++) {
        char sub[600], langs[1024];
        n = sizeof(name);
        if (RegEnumKeyExA(k, i, name, &n, NULL, NULL, NULL, NULL)) break;
        _snprintf(sub, sizeof(sub), "%s\\%s\\Attributes", RPC, name);
        if (reg_read(sub, "Language", langs, sizeof(langs))) {
            char semi[64];
            if (!strncmp(langs, lcode, len)) result = 1;
            if (!strcmp(langs, lcode)) result = 1;
            _snprintf(semi, sizeof(semi), ";%s", lcode);
            if (strstr(langs, semi)) result = 1;
        }
    }
    RegCloseKey(k);
    return result;
}

static void set_phone_convertor(const char *lcode)
{
    char s1[600], langs[1024], newv[1100];
    int done = 0;
    if (check_phone_convertors(lcode)) return;
    _snprintf(s1, sizeof(s1), "%s\\Universal\\Attributes", RPC);
    if (reg_read(s1, "Language", langs, sizeof(langs))) {
        _snprintf(newv, sizeof(newv), "%s;%s", lcode, langs);
        done = reg_write(s1, "Language", newv);
    }
    if (!done) {
        _snprintf(s1, sizeof(s1), "%s\\eSpeak\\Attributes", RPC);
        if (!reg_read(s1, "Language", langs, sizeof(langs))) {
            /* add the dummy 'eSpeak' PhoneConvertor */
            reg_write(RPC "\\eSpeak", "CLSID", "{9185F743-1143-4C28-86B5-BFF14F20E5C8}");
            reg_write(RPC "\\eSpeak", "PhoneMap", "- 0001");
            reg_write(s1, "Language", "");
        }
        if (reg_read(s1, "Language", langs, sizeof(langs))) {
            _snprintf(newv, sizeof(newv), "%s;%s", lcode, langs);
            reg_write(s1, "Language", newv);
        }
    }
}

/* ---------------- voices ---------------- */
static void clear_registry(void)
{
    int index = 0;
    for (;;) {
        char key[128];
        if (index == 0) strcpy(key, RV1); else _snprintf(key, sizeof(key), "%s_%d", RV1, index);
        if (!key_exists(key)) break;
        key_delete_tree(key);
        index++;
    }
}

static void upcase(char *s) { for (; *s; s++) *s = (char)toupper((unsigned char)*s); }

static int setup_voice(const char *voice, int index, const char *appdir)
{
    char key[160], attr[200], name[200], disp[200];
    const char *lcode = language_from_voice(voice);
    if (index == 0) strcpy(key, RV1); else _snprintf(key, sizeof(key), "%s_%d", RV1, index);
    _snprintf(attr, sizeof(attr), "%s\\Attributes", key);
    strncpy(disp, voice, sizeof(disp) - 1); disp[sizeof(disp) - 1] = 0; upcase(disp);
    _snprintf(name, sizeof(name), "eSpeak-%s", disp);
    reg_write(key, "", name);
    reg_write(key, "CLSID", CLSID_ENGINE);
    reg_write(key, "Path", appdir);
    reg_write(key, "VoiceName", voice);
    _snprintf(name, sizeof(name), "eSpeak-%s", voice);
    reg_write(attr, "Name", name);
    reg_write(attr, "Gender", "Male");
    reg_write(attr, "Age", "Adult");
    reg_write(attr, "Language", lcode);
    reg_write(attr, "Vendor", "http://espeak.sf.net");
    set_phone_convertor(lcode);
    printf("  voice %-20s token %-12s language %s\n", voice, key + strlen("Software\\Microsoft\\Speech\\Voices\\Tokens\\"), lcode);
    return 1;
}

static int is_win64(void)
{
#ifdef _WIN64
    return 1;
#else
    typedef BOOL (WINAPI *IW64)(HANDLE, PBOOL);
    IW64 f = (IW64)GetProcAddress(GetModuleHandleA("kernel32"), "IsWow64Process");
    BOOL w = FALSE;
    return f && f(GetCurrentProcess(), &w) && w;
#endif
}

static void do_install(const char *list, const char *appdir, int want64)
{
    REGSAM views[2]; int nviews = 0, v;
    char dir[MAX_PATH]; size_t dl;
    strncpy(dir, appdir, sizeof(dir) - 1); dir[sizeof(dir) - 1] = 0;
    dl = strlen(dir);
    while (dl > 3 && (dir[dl - 1] == '\\' || dir[dl - 1] == '/')) dir[--dl] = 0;

    if (is_win64()) {
        views[nviews++] = KEY_WOW64_32KEY;
        if (want64) views[nviews++] = KEY_WOW64_64KEY;
    } else views[nviews++] = 0;

    if (is_win64() && !want64) {            /* no 64-bit engine: drop older 64-bit tokens of ours */
        g_view = KEY_WOW64_64KEY;
        clear_registry();
    }
    for (v = 0; v < nviews; v++) {
        char *copy = _strdup(list), *tok, *ctx = NULL;
        char seen[200][96]; int nseen = 0, index = 0, i, dup;
        g_view = views[v];
        printf("registry view: %s\n", g_view == KEY_WOW64_64KEY ? "64-bit" : (g_view == KEY_WOW64_32KEY ? "32-bit" : "native"));
        clear_registry();
        for (tok = strtok_s(copy, " \t,;", &ctx); tok; tok = strtok_s(NULL, " \t,;", &ctx)) {
            char uc[96];
            strncpy(uc, tok, sizeof(uc) - 1); uc[sizeof(uc) - 1] = 0; upcase(uc);
            dup = 0;
            for (i = 0; i < nseen; i++) if (!strcmp(seen[i], uc)) dup = 1;
            if (dup || nseen >= 200) continue;
            strcpy(seen[nseen++], uc);
            if (setup_voice(tok, index, dir)) index++;
        }
        free(copy);
    }
}

static void do_remove(void)
{
    REGSAM views[2]; int nviews = 0, v;
    if (is_win64()) { views[nviews++] = KEY_WOW64_32KEY; views[nviews++] = KEY_WOW64_64KEY; }
    else views[nviews++] = 0;
    for (v = 0; v < nviews; v++) {
        g_view = views[v];
        clear_registry();
        key_delete_tree(RPC "\\eSpeak");
    }
    printf("eSpeak voices removed\n");
}

int main(int argc, char **argv)
{
    if (argc >= 4 && !_stricmp(argv[1], "install")) {
        do_install(argv[2], argv[3], argc >= 5 && !_stricmp(argv[4], "x64"));
        return 0;
    }
    if (argc >= 2 && !_stricmp(argv[1], "remove")) { do_remove(); return 0; }
    if (argc >= 3 && !_stricmp(argv[1], "lcid")) { printf("%s\n", language_from_voice(argv[2])); return 0; }
    fprintf(stderr, "usage: espeak-voices install \"<voices>\" \"<install dir>\" [x64] | remove | lcid <voice>\n");
    return 1;
}
