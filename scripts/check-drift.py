#!/usr/bin/env python3
"""
Two-phase drift guard for wrapper.c / wrapper-rootless.c.

Phase 1 — Skeleton comparison
  Strip each file's implementation-specific regions, normalize whitespace,
  then diff the skeletons. Any remaining difference is unexpected drift.

Phase 2 — Allow-list verification
  Verify that every region actually stripped matches a named entry in the
  allow-list, and that each entry was matched the expected number of times.
  A new region added to either file without updating the allow-list fails here
  even if Phase 1 would have passed after a new strip rule was also added.

Usage:
    python3 scripts/check-drift.py   (run from repo root)

Exit codes:
    0  both phases pass
    1  unexpected drift or allow-list violation
"""

import os
import re
import subprocess
import sys
import tempfile


# ---------------------------------------------------------------------------
# Allow-list
#
# Each entry names a region that is permitted to differ between the two files.
# 'file'    which file the region lives in
# 'type'    single_line | end | end_brace  (see strip_regions)
# 'start'   regex that identifies the first line of the region
# 'end'     regex for the closing line (type "end" only)
# 'expect'  how many times this region must appear (default 1)
# ---------------------------------------------------------------------------

ALLOW_LIST = [
    # ── wrapper.c privileged-only regions ────────────────────────────────
    {
        "name":  "stdlib.h include",
        "file":  "wrapper.c",
        "type":  "single_line",
        "start": re.compile(r"#include\s+<stdlib\.h>"),
    },
    {
        "name":  "CAP_SYS_ADMIN defines",
        "file":  "wrapper.c",
        "type":  "end",
        "start": re.compile(r"#define CAP_SYS_ADMIN_IDX"),
        "end":   re.compile(r"#define CAP_SYS_ADMIN_BIT"),
    },
    {
        "name":  "has_cap_sys_admin function",
        "file":  "wrapper.c",
        "type":  "end_brace",
        "start": re.compile(r"^int has_cap_sys_admin\(\)"),
    },
    {
        "name":  "conditional CLONE_NEWPID unshare",
        "file":  "wrapper.c",
        "type":  "end_brace",
        "start": re.compile(r"if \(has_cap_sys_admin\(\)\)"),
    },

    # ── wrapper-rootless.c rootless-only regions ──────────────────────────
    {
        "name":  "write_file comment",
        "file":  "wrapper-rootless.c",
        "type":  "single_line",
        "start": re.compile(r"/\* rootless-only: write"),
    },
    {
        "name":  "write_file function",
        "file":  "wrapper-rootless.c",
        "type":  "end_brace",
        "start": re.compile(r"^static int write_file\("),
    },
    {
        "name":  "setup_user_namespace comment",
        "file":  "wrapper-rootless.c",
        "type":  "single_line",
        "start": re.compile(r"/\* rootless-only: create"),
    },
    {
        "name":  "setup_user_namespace function",
        "file":  "wrapper-rootless.c",
        "type":  "end_brace",
        "start": re.compile(r"^static int setup_user_namespace\(\)"),
    },
    {
        "name":  "setup_user_namespace call-site comment",
        "file":  "wrapper-rootless.c",
        "type":  "single_line",
        "start": re.compile(r"/\* rootless-only: establish"),
    },
    {
        "name":  "setup_user_namespace call",
        "file":  "wrapper-rootless.c",
        "type":  "end_brace",
        "start": re.compile(r"if \(setup_user_namespace\(\)"),
    },
    {
        "name":  "unconditional CLONE_NEWPID comment",
        "file":  "wrapper-rootless.c",
        "type":  "single_line",
        "start": re.compile(r"/\* rootless-only: user namespace"),
    },
    {
        "name":  "unconditional CLONE_NEWPID unshare",
        "file":  "wrapper-rootless.c",
        "type":  "end_brace",
        "start": re.compile(r"if \(unshare\(CLONE_NEWPID\)"),
    },
]


# ---------------------------------------------------------------------------
# Stripping
# ---------------------------------------------------------------------------

def strip_end_brace(lines, start_idx):
    """Return the index after the closing brace that matches the opening
    brace on lines[start_idx]."""
    depth, seen_open = 0, False
    i = start_idx
    while i < len(lines):
        depth += lines[i].count("{") - lines[i].count("}")
        i += 1
        if depth > 0:
            seen_open = True
        if seen_open and depth <= 0:
            break
    return i


def strip_regions(lines, rules):
    """
    Remove every region matched by rules.
    Returns (stripped_lines, hits) where hits is a dict mapping rule index
    to the number of times that rule was matched.
    """
    hits = {i: 0 for i in range(len(rules))}
    result = []
    i = 0
    while i < len(lines):
        line = lines[i]
        matched_idx = next(
            (idx for idx, r in enumerate(rules) if r["start"].search(line)),
            None,
        )

        if matched_idx is None:
            result.append(line)
            i += 1
            continue

        hits[matched_idx] += 1
        rule = rules[matched_idx]

        if rule["type"] == "single_line":
            i += 1
            continue

        if rule["type"] == "end_brace":
            i = strip_end_brace(lines, i)
            continue

        # type == "end": inclusive range
        end_re = rule["end"]
        i += 1
        while i < len(lines):
            matched = end_re.search(lines[i])
            i += 1
            if matched:
                break

    return result, hits


def normalize(lines):
    out = []
    for line in lines:
        s = line.rstrip()
        if not out and "canonical implementation" in s:
            continue
        if s == "" and out and out[-1] == "":
            continue
        out.append(s)
    return out


def skeleton(path, rules):
    with open(path) as f:
        raw = [l.rstrip("\n") for l in f]
    stripped, hits = strip_regions(raw, rules)
    return normalize(stripped), hits


# ---------------------------------------------------------------------------
# Phase 1 — skeleton comparison
# ---------------------------------------------------------------------------

def phase1(canon_skel, rootless_skel):
    with tempfile.NamedTemporaryFile("w", suffix=".canon",    delete=False) as a:
        a.write("\n".join(canon_skel)    + "\n"); a_path = a.name
    with tempfile.NamedTemporaryFile("w", suffix=".rootless", delete=False) as b:
        b.write("\n".join(rootless_skel) + "\n"); b_path = b.name
    try:
        result = subprocess.run(
            ["diff", "-u",
             "--label", "wrapper.c (skeleton)",
             "--label", "wrapper-rootless.c (skeleton)",
             a_path, b_path],
            capture_output=True, text=True,
        )
    finally:
        os.unlink(a_path)
        os.unlink(b_path)
    return result


# ---------------------------------------------------------------------------
# Phase 2 — allow-list verification
# ---------------------------------------------------------------------------

def phase2(canon_hits, rootless_hits, canon_rules, rootless_rules):
    failures = []

    def check(hits, rules, label):
        for idx, rule in enumerate(rules):
            expected = rule.get("expect", 1)
            actual   = hits[idx]
            if actual != expected:
                failures.append(
                    f"  [{label}] '{rule['name']}': "
                    f"expected {expected} match(es), got {actual}"
                )

    check(canon_hits,    canon_rules,    "wrapper.c")
    check(rootless_hits, rootless_rules, "wrapper-rootless.c")
    return failures


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    canon_rules    = [r for r in ALLOW_LIST if r["file"] == "wrapper.c"]
    rootless_rules = [r for r in ALLOW_LIST if r["file"] == "wrapper-rootless.c"]

    canon_skel,    canon_hits    = skeleton("wrapper.c",          canon_rules)
    rootless_skel, rootless_hits = skeleton("wrapper-rootless.c", rootless_rules)

    p1 = phase1(canon_skel, rootless_skel)
    p2 = phase2(canon_hits, rootless_hits, canon_rules, rootless_rules)

    ok = True

    if p1.returncode != 0:
        ok = False
        print("PHASE 1 FAIL: unexpected skeleton drift.")
        print("  Add a strip rule to ALLOW_LIST for any new approved difference.")
        print()
        print(p1.stdout)

    if p2:
        ok = False
        print("PHASE 2 FAIL: allow-list region count mismatch.")
        print("  Update ALLOW_LIST if a region was intentionally added or removed.")
        print()
        for line in p2:
            print(line)
        print()

    if ok:
        canon_count    = sum(canon_hits.values())
        rootless_count = sum(rootless_hits.values())
        print(
            f"OK: {canon_count} privileged-only region(s) in wrapper.c, "
            f"{rootless_count} rootless-only region(s) in wrapper-rootless.c. "
            f"No unexpected drift."
        )
        return 0

    return 1


if __name__ == "__main__":
    sys.exit(main())
