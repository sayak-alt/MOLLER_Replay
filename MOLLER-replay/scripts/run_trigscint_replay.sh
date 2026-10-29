#!/usr/bin/env bash
set -euo pipefail
: "${MOLLER_REPLAY:?Source setup.csh or setup.sh from your framework root first}"
: "${MOLLER_OFFLINE:?Source setup.csh or setup.sh first}"
[[ $# -ge 2 && $# -le 3 ]] || { echo 'Usage: run_trigscint_replay.sh INPUT.root OUTPUT.root [NEVENTS=100]' >&2; exit 2; }
export DB_DIR="${DB_DIR:-$MOLLER_REPLAY/DB_trigscint}"
for file in db_moller.ts.dat db_cratemap.dat MOLLER_trigscint_electronics_map.csv; do
 [[ -f "$DB_DIR/$file" ]] || { echo "Missing $DB_DIR/$file; select DB_DIR=\$MOLLER_REPLAY/DB_trigscint" >&2; exit 2; }
done
expression=$(python3 - "$MOLLER_REPLAY/replay/replay_trigscint.C" "$1" "$2" "${3:-100}" <<'PYTHON'
import json,pathlib,sys
macro,src,dst,text=sys.argv[1:]
n=int(text)
if n==0 or n < -1:raise SystemExit('Use a positive event count or -1 for all')
a,b=pathlib.Path(src),pathlib.Path(dst)
if not a.is_file():raise SystemExit('Missing TS digitised input: '+src)
if a.resolve()==b.resolve() or (b.exists() and a.samefile(b)):raise SystemExit('Input and output must be different files')
print(macro+'('+json.dumps(src)+','+json.dumps(dst)+','+str(n)+')')
PYTHON
)
exec analyzer -l -b -q "$expression"
