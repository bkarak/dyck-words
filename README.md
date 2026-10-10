# dyck

Every Dyck word of length 2n, one after the other, in lexicographic order; the rank of any word
in that order; and the word at any rank. A Dyck word is a string of n opening and n closing
brackets in which no prefix closes more than it has opened, `(()())` but not `())(()`, and there
are C(n) of them, the n-th Catalan number.

The algorithm is the one Kostas Manes and Vassilios Karakoidas worked out in 2002, while
studying nested sets of pairs, and the ranking is Kostas Manes's of 2004. Between them they
wrote it more than a dozen times, in Visual Basic, C and C++. This repository is one C99
implementation of it, written in 2026 from those programs and the notes that came with them,
and it writes exactly what the 2004 C program wrote: [the tests](#tests) compare the two byte
for byte.

## Build and run

C99 and nothing else; the Makefile is portable make.

```sh
make            # ./dyck; warning-free with Apple clang 21 under -Wall -Wextra -Wpedantic -Wconversion
make test       # tests/run.sh
make bench      # tests/bench.sh, the timings below
```

```
usage: dyck [-f FORMAT] N           every Dyck word of length 2N, in order
       dyck -c N                    count them, and print only the count
       dyck -r WORD | -r -          the rank of WORD, or of each word on stdin
       dyck [-f FORMAT] -u RANK N   the word of rank RANK; -u - reads ranks from stdin
```

`make install` honours `PREFIX` and `DESTDIR`, and every variable in the Makefile can be set from
the command line (`make CC=gcc OPT=-O3`).

## What it prints

One word per line, in one of five spellings. For n = 3:

```
$ dyck 3            $ dyck -f bits 3    $ dyck -f pairs 3        $ dyck -f ncp 3    $ dyck -f positions 3
()()()              101010              {1,2} {3,4} {5,6}        {1} {2} {3}        1 3 5
()(())              101100              {1,2} {3,6} {4,5}        {1} {2,3}          1 3 4
(())()              110010              {1,4} {2,3} {5,6}        {1,2} {3}          1 2 5
(()())              110100              {1,6} {2,3} {4,5}        {1,2,3}            1 2 4
((()))              111000              {1,6} {2,5} {3,4}        {1,3} {2}          1 2 3
```

- **parens**, the default, is the word itself.
- **bits** spells it with `1` for an opening bracket and `0` for a closing one, which is what the
  2002 and 2004 programs wrote. Read as binary numbers the words are 42, 44, 50, 52, 56: the
  order is simply increasing order of those numbers.
- **pairs** is the nested set of pairs the word stands for. Put 2n points on a line and join
  them in pairs by arcs drawn above it, with no two arcs crossing; every such drawing is a Dyck
  word, with each arc's left end an opening bracket and its right end the matching close.
- **ncp** is the non-crossing partition of 1..n the word stands for. See
  [Pairs and partitions](#pairs-and-partitions).
- **positions** lists where the opening brackets are, counting from 1. This is what the program
  actually keeps; everything else is spelled from it.

```
$ dyck -c 18
477638700
$ dyck -r '(()())'
4
$ dyck -u 4 3
(()())
$ dyck -f bits 9 | dyck -r - | tail -1
4862
```

`-c` walks every word, exactly as the listing does, and writes only the total; where C(n) fits in
64 bits it also checks the total against it and fails if they differ. `-r` reads either spelling.
Ranks count from 1 and need n ≤ 36, the largest n for which C(n), 11,959,798,385,860,453,492, still
fits in 64 bits. Listing has no such limit, only patience: see [How fast](#how-fast).

## How it works

The program keeps a word as the positions of its n opening brackets, `pos[0]` to `pos[n-1]`,
counting from 1. The first one is always at 1. Each of the others has a *home*, the place it has
in the smallest word `()()...()`, which for `pos[j]` is 2j + 1, and it can sit anywhere from one
place after the bracket before it up to its home:

```
pos[j-1] < pos[j] <= 2j + 1
```

Moving an opening bracket to the left makes the word bigger, so the list starts at `()()...()`,
with every bracket at home, and ends at `((...))`, with every bracket as far left as it can go.
The successor of a word is found in one of two ways, which Kostas Manes called *shift* and
*push*:

- **Shift.** If the last opening bracket has room, move it one place left. That is two letters
  of the word, and it is the common case.
- **Push.** Otherwise the last bracket is up against the one before it. Walk left along that run
  of adjacent opening brackets until one has room, move that one a place left, and send every
  bracket after it back home. If the walk reaches the first bracket there is no room anywhere,
  and the word was `((...))`, the last.

For n = 3, starting from home:

```
()()()   1 3 5   shift: the last bracket moves from 5 to 4
()(())   1 3 4   push:  4 is against 3; 3 has room, moves to 2, and the last goes home to 5
(())()   1 2 5   shift: 5 to 4
(()())   1 2 4   shift: 4 to 3
((()))   1 2 3   push:  3 is against 2 and 2 against 1; nothing has room, so this is the last
```

The code is in [src/dyck.h](src/dyck.h) and [src/dyck.c](src/dyck.c). The shift is
`dyck_next()`, `static inline` in the header so that it compiles into the caller's loop; the push
is `dyck_push()`, the only part with a loop in it.

**What a word costs.** The push looks expensive, since it can walk the whole word, but it is
rare, and the count is exact: a word is stuck, needing a push, exactly when its last two opening
brackets are adjacent, and those words are in one-to-one correspondence with the Dyck words of
length 2(n−1) (delete one of the two brackets and one closing bracket after them). So the push
runs C(n−1) − 1 times, the shift C(n) − C(n−1) times, and since C(n)/C(n−1) = (4n−2)/(n+1) tends
to 4, three words in four come from a shift. Counted with an instrumented copy of the loop, for
n = 18: 347,993,910 shifts and 129,644,789 pushes, and 1.37 bracket moves per word on average,
counting the ones sent home.

## Ranking

The rank of a word is its place in the list, from 1. Kostas Manes worked out how to compute it
without walking the list, in a note called *RANKING*, begun on 25 July 2004, and a C++
implementation dated five days later; `dyck_rank()` and `dyck_unrank()` are that method.

Number the opening brackets from the right this time, so that the i-th one has its home 2i − 1
places from the right-hand end, and say a bracket is *moved k* when it sits k places to the left
of its home. Then let d(i, k) be the number of words that come before the first word in which the
i-th bracket is moved k. His note derives

```
d(i, 0) = 0
d(i, 1) = C(i)
d(i, k) = d(i+1, k-1) - d(i+1, k-2)
```

and the rank of a word as 1 plus d(i, k) summed over its brackets, each with the k it is moved.
One step of the derivation is marked "to be shown" in the note, and it holds: the rank agrees with
the word's place in the list for every word up to n = 12. The values do not depend on n, so one table, filled once, serves every n up to 36. The
note prints the table for n = 10, and the program's table matches it entry for entry:

```
i:     1    2    3    4    5    6    7    8    9
k=1    1    2    5   14   42  132  429 1430 4862
k=2    2    5   14   42  132  429 1430 4862
k=3    3    9   28   90  297 1001 3432
k=4    4   14   48  165  572 2002
k=5    5   20   75  275 1001
k=6    6   27  110  429
k=7    7   35  154
k=8    8   44
k=9    9
```

The note also observes that the diagonal, where i + k = 10, sums to 16,795, which is C(10) − 1:
the rank of `(((((((((())))))))))` less the 1 it starts from. It is Catalan's triangle, as it
happens; d(i, k) = k/(i+k) · binomial(2i+k−1, i), which was checked for every entry up to n = 36
but is not how the program computes it.

Unranking runs the sum backwards. Going left to right, each bracket takes the largest move whose
weight still fits in what is left of the rank, and no bracket can be moved more than one place
further than the bracket before it, or the two would collide.

## Pairs and partitions

The words in this program were never really about brackets. The 2002 work was about *nested sets
of pairs*: 2n points on a line, joined in pairs by arcs that do not cross. Each arc is a matching
pair of brackets, so `-f pairs` is only another spelling of the word.

The less obvious correspondence is with the *non-crossing partitions* of 1..n, which are also
counted by C(n), and it is the one the 2002 notes spend most of their pages on. They get there by
colouring: shade the regions under the arcs alternately, starting with the region under each
outermost arc, and put a point in each gap between neighbouring points. The shaded gaps, the ones
under an odd number of arcs, are always (1,2), (3,4), ..., (2n−1, 2n), which the notes prove, so
there are always n of them; and two of them go in the same block of the partition when they sit in
the same shaded region, under the same innermost arc. That is a partition of n points, and it is
non-crossing because the arcs are.

It sounds like work, and it reduces to reading the word two letters at a time. The k-th pair of
letters says what point k does:

| letters | bits | point k |
| --- | --- | --- |
| `((` | `11` | opens a block |
| `)(` | `01` | joins the innermost open block, which stays open |
| `))` | `00` | joins the innermost open block, and closes it |
| `()` | `10` | is a block on its own |

So `(()())` is `((`, `)(`, `))`: 1 opens a block, 2 joins it, 3 joins it and closes it, `{1,2,3}`.
The notes have this too, as a base-4 reading of the word, and their table of all fourteen
partitions for n = 4 is, line for line and in the same order, what `dyck -f ncp 4` prints. The
tests check the two-letter reading against the regions construction, which a separate script
implements, and check that no two words of length 20 give the same partition.

## How fast

`make bench`, on an Apple M4 Max under macOS 27.0.1, with Apple clang 21 and the Makefile's
`-O2`; best of three, wall clock:

```
  n            words     -c (s)   bits (s)      output
 14          2674440      0.007      0.019        77 MB
 15          9694845      0.015      0.065       300 MB
 16         35357670      0.049      0.209      1166 MB
 17        129644790      0.172      0.776      4537 MB
 18        477638700      0.614      3.106     17672 MB
 19       1767263190      2.278     12.811     68923 MB
 20       6564120420      8.452     41.552    269128 MB
```

The `bits` column writes every word to `/dev/null`, so it measures the program and not a disk.
Each n costs about four times the one before, the same (4n−2)/(n+1) again: n = 24 would take
about half an hour to count and n = 30 about two months, while any single word up to n = 36 is
one `-u` away.

For comparison, the 2004 C programs were timed on their own machines, writing to disk:
nsp-classic took 106 s for n = 18 and nsp-fast 33 s; Kostas Manes's shift/push in C++, counting
only, took 10 s for n = 18 and 2 min 22 s for n = 20 on a 2.4 GHz Celeron. Compiled today and run
on the machine above, the same nsp-classic writes the same 17.7 GB for n = 18 in 14.1 s against
this program's 3.1 s, because it spells out every word from scratch where this one changes two
letters. Counting only, the 2004 programs are the faster ones, 0.30 to 0.42 s for n = 18 against
0.61 s: they keep the positions and never spell the words, so a word costs them one integer store
and one addition to a `double` counter. `-c` keeps the word spelled, since that is what it is
meant to time.

## Tests

`make test` runs [tests/run.sh](tests/run.sh), which checks:

- n = 4 in all five formats, against [tests/expected/](tests/expected), which a separate Python
  script wrote: the order by sorting a brute-force list, the pairs by matching brackets, the
  partitions by the regions construction rather than by reading letter pairs;
- n = 1 to 14 in `bits`, against checksums of the `output.result` the 2004 nsp-classic writes for
  the same n (its code is not in this repository; it was compiled with a current cc to record
  them, and it agrees through n = 15, 300 MB);
- every count from 1 to 16 against the Catalan numbers;
- rank and unrank against the list itself, for every word up to n = 9, and at both ends of the
  range for n = 36;
- that no two words give the same partition or the same set of pairs;
- the exit status of the errors: 1 for bad data, 2 for bad usage.

The tests were mutation-checked when they were written: each of four deliberate one-line bugs in
`dyck.c` fails between 9 and 54 of the checks. CI builds and tests with gcc and clang on Linux and clang
on macOS, and again under the address and undefined-behaviour sanitisers.

## Where it came from

- **March 2002.** Kostas Manes's notes, two Word documents: the correspondence between nested
  sets of pairs and non-crossing partitions, with its proofs, and the listing, done with
  arithmetic. The word is an even number whose binary digits are the brackets, a shift adds
  2<sup>A</sup> for the position A of the rightmost one, a push counts the adjacent ones and adds
  them back at the bottom, and each word is given as a recurrence on the one before.
- **June 2002.** His first program, in Visual Basic, which does exactly that.
- **August 2002.** *Nested Sets of Pairs and the Matching Property*, version 1.66, the Visual
  Basic program around it: the nested sets of a given size with their partitions, drawn; the
  k-matching sets of a given one; the partitions with k blocks. It stops at n = 13.
- **July and August 2004.** The ranking note and its C++. Kostas Manes's C++ versions of the enumeration,
  recursive and iterative, timed on an 800 MHz Pentium III and a 2.4 GHz Celeron. Vassilios
  Karakoidas's C ones: nsp-classic, the positions version of the same algorithm; nsp-fast, which
  writes the whole shift run in one loop and is the same thing as Kostas's shift/push (its
  executable is dated 17 August 2004, the same day as his); variants that wrote hex, raw bytes,
  bzip2 and lists of arcs; a recursive walk over the tree of bracket heights; one that reused
  the list for n − 1 to build the list for n; and one, nsp-ultra-fast, that never got past an
  empty loop.
- **2008.** A Mac OS X build of nsp-classic on the author's site, without its source. It is 32-bit
  Intel, and no current Mac will run it.
- **2026.** This repository.

The dates are the files' own, since the copies they survive in were all made in 2016: the
creation and save dates Word keeps inside each document, the link time in the header of each
Windows executable, and the timestamps in the zips the programs were passed around in.

The 2004 programs computed C(n) from factorials in a `double` and counted up to it, which is the
one thing in them that has aged badly. Compiled today, the value is inexact from n = 14 on (the
x87 of 2004 kept more bits in between, so it may have held out a little longer). That is harmless
while it rounds down, and at n = 24 it rounds up, to 1289904147324.000244, so nsp-classic would
write one word too many, the first word again. This program stops when the successor runs out, and keeps C(n) only
to check its count.

## Layout

```
src/
  dyck.h          the word, the successor (its shift half inline), rank and unrank
  dyck.c          the push, the Catalan numbers, the ranking table, parsing
  main.c          the command: options, the five formats, a block-buffered writer
tests/
  run.sh          the tests above
  bench.sh        the timing table above
  expected/       n = 4 in each format, from the Python reference
Makefile
.github/          CI
```

## Credits and licence

The enumeration algorithm is Kostas Manes's and Vassilios Karakoidas's, and the ranking and
unranking are Kostas Manes's; this code is new and is MIT-licensed (see [LICENSE](LICENSE)). Two
papers by Jens Liebehenschel of Goethe University Frankfurt were on the desk in 2002 and 2004, and
they generalise all of this to Dyck languages over more than one kind of bracket: *Lexicographical
Generation of a Generalized Dyck Language* and *Ranking and Unranking of a Generalized Dyck
Language*.

## Elsewhere

The program's page is at
[bkarak.wizhut.tech/software/dyck](https://bkarak.wizhut.tech/software/dyck), and the story of how
it came back, with the theory at more length, is the post
[Nested sets of pairs, twenty-four years later](https://bkarak.wizhut.tech/blog/2026/10102026).
