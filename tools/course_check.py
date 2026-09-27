# Host course validator: JSON validity, engine limits, recall stats and SRS
# coverage for every lesson under data/sd/english_ru (or a given directory).
# Run:  python tools/course_check.py [data/sd/english_ru]
# Exit code 1 on any violation.
import json, sys, os, glob, collections

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.join("data", "sd", "english_ru")
MAX_EX, MAX_OPT, MAX_PAGES, MAX_THEORY, MAX_THEORY_LINES = 48, 6, 12, 4, 20
MAX_JSON = 36 * 1024
MAX_POOL = 18432
MAX_SRS_PER_EX, MAX_TAGS_PER_EX = 4, 4
KTYPES = {"choice", "cloze", "mistake", "reading", "recall"}

problems = []


def bad(msg):
    problems.append(msg)


manifest = json.load(open(os.path.join(ROOT, "course.json"), encoding="utf-8"))
lesson_ids = [l["id"] for l in manifest["lessons"]]
if len(set(lesson_ids)) != len(lesson_ids):
    bad("manifest: duplicate lesson ids")
if len(lesson_ids) > 24:
    bad("manifest: more than 24 lessons")

total_ex = total_recall = 0
srs_map = collections.defaultdict(lambda: {"recall": 0, "quiz": 0})
per_lesson = []

for path in sorted(glob.glob(os.path.join(ROOT, "lessons", "*.json"))):
    name = os.path.basename(path)
    size = os.path.getsize(path)
    if size > MAX_JSON:
        bad(f"{name}: JSON {size} bytes > {MAX_JSON}")
    try:
        d = json.load(open(path, encoding="utf-8"))
    except Exception as e:
        bad(f"{name}: invalid JSON: {e}")
        continue
    lid = d.get("id", "?")
    if lid not in lesson_ids:
        bad(f"{name}: id '{lid}' not in manifest")
    exs = d.get("exercises", [])
    theory = d.get("theory", [])
    if len(exs) > MAX_EX:
        bad(f"{lid}: {len(exs)} exercises > {MAX_EX}")
    if len(theory) > MAX_THEORY:
        bad(f"{lid}: {len(theory)} theory blocks > {MAX_THEORY}")
    for t in theory:
        if len(t.get("lines", [])) > MAX_THEORY_LINES:
            bad(f"{lid}: theory block '{t.get('title','')}' has >{MAX_THEORY_LINES} lines")

    # String-pool estimate: every interned string costs len+1 bytes.
    pool = 0
    seen = set()

    def pool_add(s):
        global pool
        if s is None or s in seen:
            return
        seen.add(s)
        globals()["pool"] = pool = pool + len(s.encode("utf-8")) + 1

    recall = 0
    for i, e in enumerate(exs):
        t = e.get("type")
        if t not in KTYPES:
            bad(f"{lid}[{i}]: unknown type '{t}'")
            continue
        pool_add(e.get("prompt"))
        pool_add(e.get("promptSmall"))
        pool_add(e.get("explain"))
        pool_add(e.get("answer"))
        pool_add(e.get("title"))
        for s in e.get("srs", [])[:MAX_SRS_PER_EX]:
            pool_add(s)
        for tg in e.get("tags", [])[:MAX_TAGS_PER_EX]:
            pool_add(tg)
        if t == "reading":
            pages = e.get("pages", [])
            if not pages or len(pages) > MAX_PAGES:
                bad(f"{lid}[{i}]: reading pages 0 or >{MAX_PAGES}")
            for pg in pages:
                pool_add(pg)
            continue
        if not e.get("prompt"):
            bad(f"{lid}[{i}]: no prompt")
        if len(e.get("srs", [])) > MAX_SRS_PER_EX or len(e.get("tags", [])) > MAX_TAGS_PER_EX:
            bad(f"{lid}[{i}]: too many srs/tags")
        if t == "recall":
            recall += 1
            if not e.get("answer"):
                bad(f"{lid}[{i}]: recall without answer")
            if not e.get("explain"):
                bad(f"{lid}[{i}]: recall without explain")
            if not e.get("srs"):
                bad(f"{lid}[{i}]: recall without srs")
            if len(e.get("prompt", "")) > 220 or len(e.get("answer", "")) > 160:
                bad(f"{lid}[{i}]: recall prompt/answer too long for the screen")
        else:
            opts = e.get("options", [])
            if len(opts) < 2 or len(opts) > MAX_OPT:
                bad(f"{lid}[{i}]: options must be 2..{MAX_OPT}")
            c = e.get("correct", -1)
            if not 0 <= c < len(opts):
                bad(f"{lid}[{i}]: correct index out of range")
            for o in opts:
                pool_add(o)
        for s in e.get("srs", []):
            srs_map[s]["recall" if t == "recall" else "quiz"] += 1

    if pool > MAX_POOL:
        bad(f"{lid}: string pool estimate {pool} > {MAX_POOL}")
    total_ex += len(exs)
    total_recall += recall
    per_lesson.append((lid, len(exs), recall, pool))

all_ids = set(srs_map)
covered = sorted(s for s in all_ids if srs_map[s]["recall"] > 0)
uncovered = sorted(s for s in all_ids if srs_map[s]["recall"] == 0)
repeatable = sum(v["quiz"] + v["recall"] for v in srs_map.values())

print(f"Lessons: {len(per_lesson)}   Exercises: {total_ex}   Recall: {total_recall}")
print(f"Repeatable (SRS-linked) exercises: {repeatable}")
print(f"Recall share of repeatable: {total_recall / repeatable * 100:.1f}%  (target >= 40%)")
print(f"Unique SRS ids: {len(all_ids)}   covered by recall: {len(covered)} ({len(covered) / len(all_ids) * 100:.0f}%)")
print(f"Ids without recall ({len(uncovered)}): {', '.join(uncovered)}")
print("Per lesson (exercises, recall, pool est):")
for lid, n, r, p in per_lesson:
    flag = "  <-- <2 recall" if r < 2 else ""
    print(f"  {lid:8s} {n:2d} {r:2d} {p:6d}{flag}")

if total_recall < 0.4 * repeatable:
    bad(f"recall share {total_recall / repeatable * 100:.1f}% < 40% of repeatable")
if problems:
    print("\nPROBLEMS:")
    for pr in problems:
        print("  -", pr)
    sys.exit(1)
print("\nCourse check: OK")
