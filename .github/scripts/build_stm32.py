#!/usr/bin/env python3
"""Build an STM32CubeIDE project headlessly with arm-none-eabi-gcc.

Reads the project's .cproject (Debug configuration by default) to get the
include paths, defines, source folders, MCU/FPU options and linker script,
then compiles and links with the same flags STM32CubeIDE uses. Exits non-zero
if any file fails to compile or the link fails.

Usage: build_stm32.py <project-dir> [--config Debug] [--jobs N]
"""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
import xml.etree.ElementTree as ET
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PREFIX = "arm-none-eabi-"
# STM32 family -> CPU core. FPU / float ABI are read from .cproject.
CORES = {"F0": "cortex-m0", "G0": "cortex-m0plus", "L0": "cortex-m0plus",
         "F1": "cortex-m3", "F2": "cortex-m3", "L1": "cortex-m3",
         "F3": "cortex-m4", "F4": "cortex-m4", "G4": "cortex-m4",
         "L4": "cortex-m4", "WB": "cortex-m4", "F7": "cortex-m7",
         "H7": "cortex-m7", "L5": "cortex-m33", "U5": "cortex-m33"}
SRC_EXT = {".c": "c", ".cpp": "cpp", ".cc": "cpp", ".cxx": "cpp", ".s": "asm", ".S": "asm"}


def opt(cfg, suffix):
    """Return the first <option> whose superClass ends with suffix."""
    for o in cfg.iter("option"):
        if o.get("superClass", "").endswith(suffix):
            return o
    return None


def opt_list(cfg, suffix):
    o = opt(cfg, suffix)
    return [v.get("value") for v in o.findall("listOptionValue")] if o is not None else []


def enum_tail(o, default):
    """'...option.fpu.value.fpv4-sp-d16' -> 'fpv4-sp-d16'."""
    if o is None or not o.get("value"):
        return default
    return o.get("value").rsplit(".value.", 1)[-1]


def resolve(project: Path, value: str) -> Path:
    v = value.strip().strip('"').replace("&quot;", "")
    v = v.replace("${workspace_loc:/${ProjName}", "").replace("${ProjDirPath}", "")
    v = v.rstrip("}").lstrip("/")
    if v.startswith("../"):  # relative to the <project>/<config> build dir
        v = v[3:]
    return (project / v).resolve()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("project", type=Path)
    ap.add_argument("--config", default="Debug")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    args = ap.parse_args()
    project = args.project.resolve()

    root = ET.parse(project / ".cproject").getroot()
    cfg = next((c for c in root.iter("configuration") if c.get("name") == args.config), None)
    if cfg is None:
        sys.exit(f"{project.name}: no '{args.config}' configuration in .cproject")

    mcu = opt(cfg, "option.target_mcu").get("value")          # e.g. STM32G491KCUx
    core = CORES.get(mcu[5:7])
    if core is None:
        sys.exit(f"{project.name}: unknown MCU family for {mcu}; add it to CORES")
    fpu = enum_tail(opt(cfg, "option.fpu"), "")
    abi = enum_tail(opt(cfg, "option.floatabi"), "soft")
    arch = [f"-mcpu={core}", "-mthumb", f"-mfloat-abi={abi}"] + ([f"-mfpu={fpu}"] if fpu and abi != "soft" else [])

    def tool_flags(tool):  # tool = "c.compiler" | "cpp.compiler" | "assembler"
        incs = [f"-I{resolve(project, p)}" for p in opt_list(cfg, f"tool.{tool}.option.includepaths")]
        defs = [f"-D{d}" for d in opt_list(cfg, f"tool.{tool}.option.definedsymbols")]
        return incs + defs

    common = arch + ["-g3", "-O0", "-ffunction-sections", "-fdata-sections", "-Wall",
                     "--specs=nano.specs", "-MMD", "-MP"]
    flags = {
        "c":   [PREFIX + "gcc", "-std=gnu11"] + common + tool_flags("c.compiler"),
        "cpp": [PREFIX + "g++", "-std=gnu++14", "-fno-exceptions", "-fno-rtti",
                "-fno-use-cxa-atexit"] + common + tool_flags("cpp.compiler"),
        "asm": [PREFIX + "gcc", "-x", "assembler-with-cpp", "-g3"] + arch
               + ["--specs=nano.specs"] + tool_flags("assembler"),
    }
    flags["asm"] += ["-DDEBUG"] if args.config == "Debug" else []

    # Source folders from <sourceEntries>, honouring 'excluding' patterns.
    sources = []
    for e in cfg.iter("entry"):
        if e.get("kind") != "sourcePath":
            continue
        base = project / e.get("name", "")
        excl = [x for x in (e.get("excluding") or "").split("|") if x]
        for f in sorted(base.rglob("*")):
            rel = f.relative_to(base).as_posix()
            if f.suffix in SRC_EXT and not any(rel == x or rel.startswith(x.rstrip("/") + "/") for x in excl):
                sources.append(f)
    sources = sorted(set(sources))
    if not sources:
        sys.exit(f"{project.name}: no sources found")

    out = project / "build-ci" / args.config
    out.mkdir(parents=True, exist_ok=True)

    def compile_one(src: Path):
        obj = out / (src.relative_to(project).as_posix().replace("/", "__") + ".o")
        cmd = flags[SRC_EXT[src.suffix]] + ["-c", str(src), "-o", str(obj)]
        r = subprocess.run(cmd, capture_output=True, text=True)
        return src, obj, r

    print(f"== {project.name}: {mcu} ({' '.join(arch)}), {len(sources)} sources", flush=True)
    failed, objs = [], []
    with ThreadPoolExecutor(args.jobs) as pool:
        for src, obj, r in pool.map(compile_one, sources):
            rel = src.relative_to(project).as_posix()
            if r.stdout or r.stderr:
                print(f"--- {rel}\n{r.stdout}{r.stderr}", end="", flush=True)
            if r.returncode != 0:
                failed.append(rel)
                print(f"::error file={project.name}/{rel}::compile failed", flush=True)
            objs.append(obj)
    if failed:
        print(f"\n{project.name}: {len(failed)} file(s) failed to compile: {', '.join(failed)}")
        return 1

    ld_opt = opt(cfg, "c.linker.option.script") or opt(cfg, "cpp.linker.option.script")
    ldscript = resolve(project, ld_opt.get("value"))
    other = []
    for v in opt_list(cfg, "cpp.linker.option.otherflags") + opt_list(cfg, "c.linker.option.otherflags"):
        other += shlex.split(v)
    elf = out / f"{project.name}.elf"
    link = ([PREFIX + "g++", "-o", str(elf)] + [str(o) for o in objs] + arch
            + [f"-T{ldscript}", "--specs=nosys.specs", "--specs=nano.specs",
               f"-Wl,-Map={out / (project.name + '.map')}", "-Wl,--gc-sections", "-static"]
            + other + ["-Wl,--start-group", "-lc", "-lm", "-lstdc++", "-lsupc++", "-Wl,--end-group"])
    r = subprocess.run(link, capture_output=True, text=True)
    print(r.stdout + r.stderr, end="")
    if r.returncode != 0:
        print(f"::error::{project.name}: link failed")
        return 1
    subprocess.run([PREFIX + "size", str(elf)])
    print(f"{project.name}: build OK -> {elf.relative_to(project)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
