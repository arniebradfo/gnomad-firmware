#!/usr/bin/env python3
# Gnomad, a fork of Felucca (see FORK.md). Not part of upstream Felucca.
"""The failing tests of a test runner's log, by name: a summary at its end.

  gn_test_summary.py LOG      (the runner run with `sh -x`: tests/run_tests.sh, tests/run_fork_tests.sh)

Upstream's runner (and ours) runs each test as `run "NAME" CMD...` and only sets fail=1 when CMD fails, so a test that
prints no FAIL line leaves nothing to read. Run with `sh -x`, the shell traces `+ fail=1` right after the failing
command; this names the test it belongs to (the last "== NAME" before it) and lists, per test, the lines that say what
went wrong (FAIL, OVER, Traceback, not found, FAILED). It also lists such lines of tests that did not set fail. It only
reads the log; exit 0 (the runner's own exit status is the verdict)."""
import re
import sys

BAD = re.compile(r"\bFAIL\b|\bOVER\b|Traceback|not found|FAILED|Assertion|Segmentation|Abort")
text = open(sys.argv[1], errors="replace").read().splitlines()
test, failed, lines, order = "(before the first test)", [], {}, []
for ln in text:
    if ln.startswith("== "):
        test = ln[3:].strip()
        continue
    if re.match(r"^\++ fail=1$", ln.strip()):
        if test not in failed:
            failed.append(test)
        continue
    if ln.startswith("+") or not BAD.search(ln) or re.search(r"\bok$", ln.strip()) or \
            re.fullmatch(r"(HOST|FORK) TESTS FAILED", ln.strip()):            # (the runner's own verdict)
        continue
    if test not in lines:
        lines[test] = []
        order.append(test)
    if len(lines[test]) < 12:
        lines[test].append(ln.strip())
print("=" * 100)
print(f"SUMMARY ({sys.argv[1]}): {len(failed)} failing test(s)" + (":" if failed else ""))
for t in failed:
    print(f"  FAILED  {t}")
    for ln in lines.get(t, ["(it printed no FAIL line: its command's exit status failed it; see its section above)"]):
        print(f"            {ln}")
others = [t for t in order if t not in failed]
if others:
    print("  lines that read as failures in tests that passed (for the reader; not failures):")
    for t in others:
        for ln in lines[t][:3]:
            print(f"    {t[:60]}: {ln}")
