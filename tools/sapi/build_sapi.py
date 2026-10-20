#!/usr/bin/env python3
"""Builds the eSpeak 1.48.15 SAPI5 engine (espeak_sapi.dll) with Visual C++ on Windows.

    python tools/sapi/build_sapi.py --arch x64      (or --arch x86)
    python tools/sapi/build_sapi.py --arch x64 --dry-run   (prepare sources, print commands)

Output: build-out/sapi/espeak_sapi-<arch>.dll

What it does: copies platforms/windows/windows_sapi, drops the 1.48.15 sources from src/ into it
(keeping the project's own speech.h / StdAfx.h), applies the small fixes marked PATCH 1-7 in prepare(),
then runs cl / rc / link from a Visual Studio developer environment. No precompiled headers.
Needs: Visual Studio 2017+ (or Build Tools) with the C++ workload, ATL and a Windows 10/11 SDK.
"""
import argparse
import glob
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SAPI_SRC = os.path.join(ROOT, "platforms", "windows", "windows_sapi")

# espeak library sources compiled into the engine (same list as the original ttseng.dsp)
LIB_SOURCES = ["compiledict", "dictionary", "intonation", "klatt", "numbers", "phonemelist",
               "readclause", "setlengths", "sonic", "speak_lib", "synth_mbrola", "synthdata",
               "synthesize", "tr_languages", "translate", "voices", "wavegen"]
ENGINE_SOURCES = ["ttseng", "ttsengobj"]
# Windows-specific files of the project that must not be overwritten by src/ copies
KEEP_WINDOWS = {"speech.h", "stdafx.h", "stdint.h"}


# Two SAPI data-format GUIDs that the engine references. The project's old (32-bit) sapi.lib supplied
# them; the 64-bit build cannot use that library, so they are defined here (values verified against
# the official espeak_sapi.dll). If the SDK's sapi.lib also defines them, build() retries without.
GUIDS_CPP = """#include <windows.h>
extern "C" extern const GUID SPDFID_Text =
    {0x7CEEF9F9, 0x3D13, 0x11d2, {0x9E, 0xE7, 0x00, 0xC0, 0x4F, 0x79, 0x73, 0x96}};
extern "C" extern const GUID SPDFID_WaveFormatEx =
    {0xC31ADBAE, 0x527F, 0x4ff5, {0xA2, 0x30, 0xF6, 0x2B, 0xB6, 0x1F, 0xF7, 0x0C}};
"""


def log(msg):
    print("[build_sapi] " + msg, flush=True)


def annotate(level, title, text, limit=3500):
    """GitHub Actions annotation (error/warning/notice). Shown on the run summary page, which
    can be read without logging in. Newlines are encoded as %0A."""
    text = text.strip()
    if len(text) > limit:
        text = text[:limit] + "\n... (cut)"
    text = text.replace("%", "%25").replace("\r", "").replace("\n", "%0A")
    print("::%s title=%s::%s" % (level, title.replace(",", ";").replace(":", " -"), text), flush=True)


def read(path):
    with open(path, "r", encoding="latin-1", newline="") as f:
        return f.read()


def write(path, text):
    with open(path, "w", encoding="latin-1", newline="") as f:
        f.write(text)


def patch(path, old, new, count=1):
    s = read(path)
    if old not in s:
        raise SystemExit("patch target not found in %s: %r" % (path, old))
    write(path, s.replace(old, new, count))
    log("patched %s" % os.path.relpath(path, ROOT))


def prepare(build_dir):
    """Create build_dir with the SAPI project + 1.48.15 sources and apply the fixes."""
    if os.path.isdir(build_dir):
        shutil.rmtree(build_dir)
    shutil.copytree(SAPI_SRC, build_dir)
    src_dir = os.path.join(build_dir, "src")
    os.makedirs(src_dir, exist_ok=True)
    n = 0
    for f in sorted(glob.glob(os.path.join(ROOT, "src", "*"))):
        name = os.path.basename(f)
        if name.lower() in KEEP_WINDOWS or not os.path.isfile(f):
            continue
        shutil.copy2(f, os.path.join(src_dir, name))
        n += 1
    log("copied %d files from src/" % n)

    # PATCH 1: the project's stdint.h is a VC6 stand-in; modern Visual C++ has <stdint.h>
    stale = os.path.join(src_dir, "stdint.h")
    if os.path.exists(stale):
        os.remove(stale)
    # PATCH 2: speech.h (Windows version) uses uint64_t for long64 on 64-bit builds
    patch(os.path.join(src_dir, "speech.h"), "#define SPEECH_H", "#define SPEECH_H\n#include <stdint.h>")
    # PATCH 3: Visual C++ defines _WIN64, not __WIN64__; build_sapi passes /D__WIN64__ for x64.
    # PATCH 4: ATL needs a newer minimum Windows version than the 0x0400 in the old stdafx.h
    patch(os.path.join(build_dir, "stdafx.h"), "#define _WIN32_WINNT 0x0400", "#define _WIN32_WINNT 0x0601")
    # PATCH 5: the typelib in the project is a 32-bit one; the engine does not need it registered
    cpp = os.path.join(build_dir, "ttseng.cpp")
    patch(cpp, "_Module.RegisterServer(TRUE)", "_Module.RegisterServer(FALSE)")
    patch(cpp, "_Module.UnregisterServer(TRUE)", "_Module.UnregisterServer(FALSE)")
    # PATCH 6: `_export` is a 16-bit Windows keyword that modern Visual C++ rejects (C2146/C2059)
    hdr = os.path.join(build_dir, "ttsengobj.h")
    h = read(hdr)
    h2 = re.sub(r"FAR PASCAL\s+_export", "FAR PASCAL", h)
    if h2 == h:
        raise SystemExit("patch target not found in ttsengobj.h: _export")
    write(hdr, h2)
    log("patched ttsengobj.h (_export)")
    # PATCH 7: SPDBG_FUNC is a debug-trace macro of the old SAPI SDK; the Windows 10 SDK headers do
    # not define it (C3861). Define it as a no-op when it is missing.
    eng = os.path.join(build_dir, "ttsengobj.cpp")
    c = read(eng)
    eol = "\r\n" if "\r\n" in c else "\n"
    anchor = '#include "stdio.h"'
    if anchor not in c:
        raise SystemExit("patch target not found in ttsengobj.cpp: " + anchor)
    c = c.replace(anchor, anchor + eol + "#ifndef SPDBG_FUNC" + eol + "#define SPDBG_FUNC(x)" + eol + "#endif", 1)
    write(eng, c)
    log("patched ttsengobj.cpp (SPDBG_FUNC)")
    return build_dir


def vs_environment(arch):
    """Return os.environ plus the variables set by vcvarsall.bat for the given arch."""
    vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
                           "Microsoft Visual Studio", "Installer", "vswhere.exe")
    base = [vswhere, "-latest", "-products", "*", "-property", "installationPath"]
    root = subprocess.run(base + ["-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"],
                          capture_output=True, text=True).stdout.strip()
    if not root:
        annotate("error", "vswhere", "Visual Studio with the C++ tools was not found (vswhere: %s)" % vswhere)
        raise SystemExit("Visual Studio with the C++ tools was not found")
    atl = subprocess.run(base + ["-requires", "Microsoft.VisualStudio.Component.VC.ATL"],
                         capture_output=True, text=True).stdout.strip()
    if not atl:
        log("ATL component missing - installing it (a few minutes)")
        setup = os.path.join(os.path.dirname(vswhere), "setup.exe")
        subprocess.run([setup, "modify", "--installPath", root, "--add",
                        "Microsoft.VisualStudio.Component.VC.ATL", "--quiet", "--norestart",
                        "--noUpdateInstaller"], check=False)
    vcvars = os.path.join(root, "VC", "Auxiliary", "Build", "vcvarsall.bat")
    out = subprocess.run('cmd /c ""%s" %s >nul && set"' % (vcvars, arch), shell=True,
                         capture_output=True, text=True).stdout
    env = {k.upper(): v for k, v in os.environ.items()}
    for line in out.splitlines():
        if "=" in line and not line.startswith("="):
            k, v = line.split("=", 1)
            env[k.upper()] = v
    return env


def tool(name, env):
    """Full path of a tool found on the PATH of `env` (Windows' CreateProcess would otherwise
    search the PATH of this Python process, which does not contain the Visual C++ folders)."""
    path = env.get("PATH") or env.get("Path") or os.environ.get("PATH", "")
    exe = shutil.which(name, path=path)
    if not exe:
        annotate("error", "Tool not found", "%s was not found on PATH:\n%s" % (name, path[:1500]))
        raise SystemExit("tool not found: " + name)
    return exe


def run(cmd, env, cwd, dry):
    """Run a command, echo its output, return (returncode, output)."""
    log("> " + " ".join(cmd))
    if dry:
        return 0, ""
    cmd = [tool(cmd[0], env)] + list(cmd[1:])
    r = subprocess.run(cmd, env=env, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = r.stdout.decode("utf-8", "replace")
    print(out, flush=True)
    return r.returncode, out


def error_lines(out, maxlines=14):
    """The interesting lines of compiler/linker output."""
    keep = [l.strip() for l in out.splitlines()
            if re.search(r"\berror\b|\bfatal\b|LNK\d+|RC\d+|cannot open|unresolved", l, re.I)]
    return "\n".join(keep[:maxlines]) or out[-1200:]


def find_header(env, name):
    for d in env.get("INCLUDE", "").split(";"):
        if d and os.path.exists(os.path.join(d, name)):
            return os.path.join(d, name)
    return None


NO_VS = False


def build(arch, dry):
    build_dir = prepare(os.path.join(ROOT, "build-tmp", "sapi-" + arch))
    out_dir = os.path.join(ROOT, "build-out", "sapi")
    os.makedirs(out_dir, exist_ok=True)
    env = os.environ if (dry or NO_VS) else vs_environment("x64" if arch == "x64" else "x86")
    if not (dry or NO_VS):
        found, missing = [], []
        for h in ("atlbase.h", "atlcom.h", "spddkhlp.h", "spcollec.h", "sapiddk.h"):
            p = find_header(env, h)
            log("%-12s %s" % (h, p or "NOT FOUND"))
            (found if p else missing).append("%s -> %s" % (h, p or "NOT FOUND"))
        ver = subprocess.run([tool("cl", env)], env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT).stdout.decode("utf-8", "replace").splitlines()
        annotate("notice", "Toolchain " + arch, "\n".join(ver[:1] + found + missing))
        if missing:
            annotate("error", "Missing headers " + arch, "\n".join(missing))
            raise SystemExit("required headers are missing (ATL component or Windows SDK)")

    defs = ["NDEBUG", "WIN32", "_WINDOWS", "_MBCS", "_USRDLL", "_ATL_STATIC_REGISTRY",
            "_CRT_SECURE_NO_WARNINGS", "_CRT_NONSTDC_NO_WARNINGS", "_WINSOCK_DEPRECATED_NO_WARNINGS",
            "_WIN32_WINNT=0x0601"]
    if arch == "x64":
        defs.append("__WIN64__")
    cflags = ["cl", "/nologo", "/c", "/MT", "/O1", "/EHsc", "/W3", "/wd4244", "/wd4267", "/wd4996",
              "/wd4305", "/wd4309", "/wd4838", "/wd4018", "/wd4101", "/wd4102", "/wd4311", "/wd4302",
              "/I."] + ["/D" + d for d in defs]
    engine = list(ENGINE_SOURCES)
    if arch == "x64":
        stale = os.path.join(build_dir, "sapi.lib")      # the project's 32-bit import library
        if os.path.exists(stale):
            os.remove(stale)
            log("removed the project's 32-bit sapi.lib (the SDK's x64 sapi.lib is used)")
        write(os.path.join(build_dir, "sapi_guids.cpp"), GUIDS_CPP)
        engine.append("sapi_guids")
    objs = []
    failures = []
    for name in engine:
        rc, out = run(cflags + ["/Fo%s.obj" % name, name + ".cpp"], env, build_dir, dry)
        objs.append(name + ".obj")
        if rc:
            failures.append((name + ".cpp", out))
    for name in LIB_SOURCES:
        rc, out = run(cflags + ["/Fosrc_%s.obj" % name, os.path.join("src", name + ".cpp")], env, build_dir, dry)
        objs.append("src_%s.obj" % name)
        if rc:
            failures.append(("src/" + name + ".cpp", out))
    if failures:
        log("%d source file(s) failed to compile" % len(failures))
        annotate("error", "Compile summary " + arch, "failed: " + ", ".join(f for f, _ in failures))
        for f, out in failures[:8]:
            annotate("error", "cl " + f, error_lines(out))
        raise SystemExit(1)
    rc, out = run(["rc", "/nologo", "/l", "0x409", "/I.", "/fo", "ttseng.res", "ttseng.rc"], env, build_dir, dry)
    if rc:
        annotate("error", "rc ttseng.rc", error_lines(out))
        raise SystemExit(1)
    dll = "espeak_sapi-%s.dll" % arch
    libs = ["ttseng.res", "kernel32.lib", "user32.lib", "advapi32.lib", "ole32.lib", "oleaut32.lib",
            "uuid.lib", "sapi.lib"]

    def link(objlist):
        return run(["link", "/nologo", "/DLL", "/OUT:" + dll, "/DEF:ttseng.def",
                    "/MACHINE:" + ("X64" if arch == "x64" else "X86")] + objlist + libs, env, build_dir, dry)

    rc, out = link(objs)
    if rc and "LNK2005" in out and "SPDFID" in out and "sapi_guids.obj" in objs:
        log("the SDK's sapi.lib already defines the SPDFID GUIDs - linking again without ours")
        rc, out = link([o for o in objs if o != "sapi_guids.obj"])
    if rc:
        annotate("error", "link " + dll, error_lines(out))
        raise SystemExit(1)
    if not dry:
        shutil.copy2(os.path.join(build_dir, dll), os.path.join(out_dir, dll))
        log("built " + os.path.join(out_dir, dll))


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--arch", choices=["x86", "x64"], required=True)
    ap.add_argument("--dry-run", action="store_true", help="prepare sources and print commands only")
    ap.add_argument("--no-vs", action="store_true", help="(testing) use cl/rc/link from PATH, skip vswhere")
    a = ap.parse_args()
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass
    NO_VS = a.no_vs
    try:
        build(a.arch, a.dry_run)
    except SystemExit:
        raise
    except Exception:
        import traceback
        annotate("error", "build_sapi.py crashed", traceback.format_exc())
        raise
