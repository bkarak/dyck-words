#!/usr/bin/env bash
# tests/bench.sh ./dyck [N ...]
#
# Wall-clock seconds, best of three, for walking every word of length 2N
# (dyck -c N) and for writing every word to /dev/null (dyck -f bits N).
# N defaults to 14 through 18.
set -euo pipefail

DYCK=${1:-./dyck}
shift || true
sizes=("$@")
[ ${#sizes[@]} -gt 0 ] || sizes=(14 15 16 17 18)

best() {
    local best="" t
    for _ in 1 2 3; do
        t=$( { TIMEFORMAT=%R; time "$@" > /dev/null; } 2>&1 )
        if [ -z "$best" ] || [ "$(echo "$t < $best" | bc)" = 1 ]; then
            best=$t
        fi
    done
    echo "$best"
}

printf '%3s  %15s  %9s  %9s  %10s\n' n words "-c (s)" "bits (s)" "output"
for n in "${sizes[@]}"; do
    words=$("$DYCK" -c "$n")
    bytes=$(( words * (2 * n + 1) ))
    printf '%3s  %15s  %9s  %9s  %8s MB\n' "$n" "$words" \
        "$(best "$DYCK" -c "$n")" "$(best "$DYCK" -f bits "$n")" $(( bytes / 1000000 ))
done
