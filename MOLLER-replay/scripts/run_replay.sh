#!/usr/bin/env bash
set -euo pipefail
if [[ $# -lt 1 || $# -gt 3 ]]; then
 echo "Usage: $0 input.root [output.root] [maxevents: -1 means all]" >&2;exit 2
fi
: "${MOLLER_REPLAY:?Source setup.csh or setup.sh first}"
# Python supplies correct C++ string escaping; argv never becomes shell code.
expression=$(python3 - "$MOLLER_REPLAY/replay/replay.C" "$1" "${2:-replayed_md.root}" "${3:-100}" <<'PYTHON'
import json,sys,pathlib
macro,src,dst,n=sys.argv[1:];int(n)
if pathlib.Path(src).resolve()==pathlib.Path(dst).resolve():
 raise SystemExit('Input and output must be different files')
if pathlib.Path(dst).exists() and pathlib.Path(src).exists() and pathlib.Path(src).samefile(dst):
 raise SystemExit('Input/output refer to the same file')
print(macro+'('+json.dumps(src)+','+json.dumps(dst)+','+n+')')
PYTHON
)
exec analyzer -l -b -q "$expression"
