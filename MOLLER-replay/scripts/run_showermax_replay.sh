#!/usr/bin/env bash
set -euo pipefail
: "${MOLLER_REPLAY:?Source setup.csh or setup.sh first}"
: "${MOLLER_OFFLINE:?Source setup.csh or setup.sh first}"
if [[ $# -lt 2 || $# -gt 3 ]]; then
 echo 'Usage: run_showermax_replay.sh INPUT.root OUTPUT.root [NEVENTS=-1]' >&2; exit 2
fi
export DB_DIR="${DB_DIR:-$MOLLER_REPLAY/DB_showermax}"
[[ -f "$DB_DIR/db_moller.sm.dat" ]] || { echo 'Select DB_DIR=$MOLLER_REPLAY/DB_showermax first' >&2; exit 2; }
[[ -f "$1" ]] || { echo "Input not found: $1" >&2; exit 2; }
[[ ${3:--1} =~ ^(-1|[1-9][0-9]*)$ ]] || { echo 'Invalid event count' >&2; exit 2; }
quote_cpp() { local x=$1; x=${x//\\/\\\\}; x=${x//\"/\\\"}; printf '"%s"' "$x"; }
macro="$MOLLER_REPLAY/replay/replay_showermax.C"
exec analyzer -l -b -q "${macro}($(quote_cpp "$1"),$(quote_cpp "$2"),${3:--1})"
