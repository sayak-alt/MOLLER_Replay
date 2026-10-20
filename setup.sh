# source this script in bash; paths follow the script location.
_moller_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export MOLLER_OFFLINE="$_moller_root/install"
export MOLLER_REPLAY="$_moller_root/MOLLER-replay"
export DB_DIR="$MOLLER_REPLAY/DB"
export LD_LIBRARY_PATH="$MOLLER_OFFLINE/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export ROOT_INCLUDE_PATH="$MOLLER_OFFLINE/include${ROOT_INCLUDE_PATH:+:$ROOT_INCLUDE_PATH}"
unset _moller_root
