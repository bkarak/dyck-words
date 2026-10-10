#!/bin/sh
# tests/run.sh ./dyck
#
# What is checked, and against what:
#
#   - n = 4 in every format, against tests/expected/4.*, which were written by
#     a separate Python reference: the pairs by matching brackets, the
#     partitions by the minimal-arc construction of the 2002 notes rather than
#     by reading letter pairs, the order by sorting a brute-force list.
#   - n = 1 to 14 in bits, against checksums of what the 2002 program,
#     nsp-classic, writes to output.result for the same n (compiled with a
#     2026 cc; its code is not in this repository).
#   - every count from 1 to 16 against the Catalan numbers.
#   - ranking and unranking, against the position of each word in the list,
#     for every word up to n = 9, and at the ends of the range for n = 36.
#   - partitions and pairs are a bijection: no two words give the same one.
#   - errors exit 1 for bad data and 2 for bad usage.
set -u

DYCK=${1:-./dyck}
HERE=$(cd "$(dirname "$0")" && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
failures=0

fail() {
    echo "FAIL: $*"
    failures=$((failures + 1))
}

for f in parens bits pairs ncp positions; do
    if ! "$DYCK" -f "$f" 4 | cmp -s - "$HERE/expected/4.$f"; then
        fail "n=4 -f $f differs from tests/expected/4.$f"
    fi
done

# cksum of nsp-classic's output.result, n = 1 to 14.
while read -r n sum size; do
    got=$("$DYCK" -f bits "$n" | cksum | awk '{ print $1, $2 }')
    [ "$got" = "$sum $size" ] || fail "n=$n -f bits: cksum $got, nsp-classic wrote $sum $size"
done <<'EOF'
1 1057598734 3
2 1664015047 10
3 1834257362 35
4 1913733203 126
5 1200704861 462
6 355870757 1716
7 2576400717 6435
8 2417562635 24310
9 4256370142 92378
10 3026867634 352716
11 3113176182 1352078
12 3948573928 5200300
13 2285381706 20058300
14 489721856 77558760
EOF

n=1
for c in 1 2 5 14 42 132 429 1430 4862 16796 58786 208012 742900 2674440 9694845 35357670; do
    got=$("$DYCK" -c "$n")
    [ "$got" = "$c" ] || fail "-c $n printed $got, C($n) = $c"
    n=$((n + 1))
done

n=1
for c in 1 2 5 14 42 132 429 1430 4862; do
    seq 1 "$c" > "$TMP/ranks"
    "$DYCK" "$n" > "$TMP/words"
    "$DYCK" -f bits "$n" | "$DYCK" -r - | cmp -s - "$TMP/ranks" \
        || fail "-r: ranks of the n=$n list are not 1..$c"
    "$DYCK" -r - < "$TMP/words" | cmp -s - "$TMP/ranks" \
        || fail "-r: ranks of the n=$n list, in parentheses, are not 1..$c"
    "$DYCK" -u - "$n" < "$TMP/ranks" | cmp -s - "$TMP/words" \
        || fail "-u: unranking 1..$c does not give the n=$n list"
    n=$((n + 1))
done

open36=$(printf '((((((((((((((((((((((((((((((((((((')
close36=$(printf '))))))))))))))))))))))))))))))))))))')
[ "$("$DYCK" -r "$open36$close36")" = 11959798385860453492 ] \
    || fail "-r: the last word of n=36 is not rank C(36)"
[ "$("$DYCK" -u 11959798385860453492 36)" = "$open36$close36" ] \
    || fail "-u: rank C(36) is not the last word of n=36"
[ "$("$DYCK" -r "$(printf '()%.0s' $(seq 1 36))")" = 1 ] \
    || fail "-r: the first word of n=36 is not rank 1"

for f in ncp pairs; do
    [ "$("$DYCK" -f "$f" 10 | sort -u | wc -l | tr -d ' ')" = 16796 ] \
        || fail "-f $f 10: two words give the same $f"
done

expect_status() {
    want=$1
    shift
    "$@" > /dev/null 2>&1
    got=$?
    [ "$got" = "$want" ] || fail "'$*' exited $got, expected $want"
}

expect_status 0 "$DYCK" -h
expect_status 2 "$DYCK"
expect_status 2 "$DYCK" 0
expect_status 2 "$DYCK" 3 4
expect_status 2 "$DYCK" -f words 3
expect_status 2 "$DYCK" -u 1 37
expect_status 2 "$DYCK" -c -r '()'
expect_status 1 "$DYCK" -r '())('
expect_status 1 "$DYCK" -r '(()'
expect_status 1 "$DYCK" -r '(x)'
expect_status 1 "$DYCK" -u 15 4
expect_status 1 "$DYCK" -u 0 4
expect_status 1 "$DYCK" -u -1 4

if [ "$failures" -ne 0 ]; then
    echo "$failures failed"
    exit 1
fi
echo "all tests passed"
