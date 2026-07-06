"""Pre-buildfs hook: stage a gzipped copy of data/ and point the LittleFS
image builder at it. Text/vector assets are stored as <name>.gz (served with
Content-Encoding: gzip by ESPAsyncWebServer, which probes for .gz first).
Keeps the editable originals in data/ untouched."""
Import("env")  # noqa: F821
import os
import gzip
import shutil

GZIP_EXT = {".html", ".css", ".js", ".svg", ".json", ".ico", ".txt"}

src = env.get("PROJECT_DATA_DIR")  # noqa: F821
stage = os.path.join(env.subst("$BUILD_DIR"), "data_gz")  # noqa: F821


def stage_assets():
    if not src or not os.path.isdir(src):
        return
    if os.path.isdir(stage):
        shutil.rmtree(stage)
    total = 0
    for root, _dirs, files in os.walk(src):
        rel = os.path.relpath(root, src)
        outdir = os.path.join(stage, rel) if rel != "." else stage
        os.makedirs(outdir, exist_ok=True)
        for f in files:
            inpath = os.path.join(root, f)
            ext = os.path.splitext(f)[1].lower()
            if ext in GZIP_EXT:
                with open(inpath, "rb") as fi, gzip.open(
                    os.path.join(outdir, f + ".gz"), "wb", 9
                ) as fo:
                    shutil.copyfileobj(fi, fo)
            else:
                shutil.copy2(inpath, os.path.join(outdir, f))
            total += 1
    print(f"[gzip_fs] staged {total} web asset(s) -> {stage}")


stage_assets()
env.Replace(PROJECT_DATA_DIR=stage)  # noqa: F821
