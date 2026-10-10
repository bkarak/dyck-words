/*
 * main.c - the dyck command: enumerate, count, rank and unrank Dyck words.
 */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dyck.h"

#define EXIT_USAGE 2

static const char *usage =
    "usage: dyck [-f FORMAT] N           every Dyck word of length 2N, in order\n"
    "       dyck -c N                    count them, and print only the count\n"
    "       dyck -r WORD | -r -          the rank of WORD, or of each word on stdin\n"
    "       dyck [-f FORMAT] -u RANK N   the word of rank RANK; -u - reads ranks from stdin\n"
    "\n"
    "FORMAT is one of\n"
    "  parens      (()())             the word (the default)\n"
    "  bits        110100             the word in binary, ( as 1\n"
    "  pairs       {1,6} {2,3} {4,5}  the nested set of pairs it stands for\n"
    "  ncp         {1,2,3}            the non-crossing partition of 1..N it stands for\n"
    "  positions   1 2 4              where its opening brackets are\n"
    "\n"
    "Ranks count from 1, for ()()...(), and need N <= 36.\n";

enum format { F_PARENS, F_BITS, F_PAIRS, F_NCP, F_POSITIONS };

static const char *format_names[] = { "parens", "bits", "pairs", "ncp", "positions" };

/* ------------------------------------------------------------------ output */

/* A block buffer in front of stdout, as the 2002 writer had: one fwrite per
   64 KiB rather than one per word. */
typedef struct {
    char buf[1 << 16];
    size_t len;
    int failed;
} out_t;

static void out_flush(out_t *o)
{
    if (o->len > 0 && fwrite(o->buf, 1, o->len, stdout) != o->len)
        o->failed = 1;
    o->len = 0;
}

static void out_bytes(out_t *o, const char *s, size_t n)
{
    if (o->len + n > sizeof o->buf)
        out_flush(o);
    if (n > sizeof o->buf) {
        if (fwrite(s, 1, n, stdout) != n)
            o->failed = 1;
        return;
    }
    memcpy(o->buf + o->len, s, n);
    o->len += n;
}

static void out_char(out_t *o, char c)
{
    if (o->len == sizeof o->buf)
        out_flush(o);
    o->buf[o->len++] = c;
}

static void out_uint(out_t *o, uint64_t v)
{
    char tmp[20];
    size_t i = sizeof tmp;

    do {
        tmp[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v != 0);
    out_bytes(o, tmp + i, sizeof tmp - i);
}

/* ----------------------------------------------------------------- formats */

/* Scratch space for the formats that have to read the word as a structure. */
typedef struct {
    int *stack;    /* n */
    int *partner;  /* 2n: for each opening bracket, the index of its closer */
    int *next;     /* n + 1: the next point in the same block, 0 for none */
    int *tail;     /* n + 1: the last point so far of each block */
    int *first;    /* n: the first point of each block, in order of opening */
} scratch_t;

static int scratch_init(scratch_t *s, int n)
{
    s->stack = malloc((size_t)n * sizeof(int));
    s->partner = malloc(2 * (size_t)n * sizeof(int));
    s->next = malloc((size_t)(n + 1) * sizeof(int));
    s->tail = malloc((size_t)(n + 1) * sizeof(int));
    s->first = malloc((size_t)n * sizeof(int));
    return s->stack && s->partner && s->next && s->tail && s->first ? 0 : -1;
}

static void scratch_free(scratch_t *s)
{
    free(s->stack);
    free(s->partner);
    free(s->next);
    free(s->tail);
    free(s->first);
}

/* Each opening bracket and its closer are a pair, and pairs never cross. */
static void emit_pairs(const dyck_t *d, scratch_t *s, out_t *o)
{
    int i, j, top = 0;

    for (i = 0; i < 2 * d->n; i++) {
        if (d->word[i] == d->open)
            s->stack[top++] = i;
        else
            s->partner[s->stack[--top]] = i;
    }
    for (j = 0; j < d->n; j++) {
        if (j > 0)
            out_char(o, ' ');
        out_char(o, '{');
        out_uint(o, (uint64_t)d->pos[j]);
        out_char(o, ',');
        out_uint(o, (uint64_t)s->partner[d->pos[j] - 1] + 1);
        out_char(o, '}');
    }
}

/*
 * Read the word two letters at a time: the k-th two letters say what point k
 * of the partition does. (( opens a block, )) closes the innermost open one,
 * )( joins it and leaves it open, and () is a block on its own. The blocks
 * open in order of their smallest point, so printing them in the order they
 * were opened sorts them.
 */
static void emit_ncp(const dyck_t *d, scratch_t *s, out_t *o)
{
    int k, b, blocks = 0, top = 0;
    const char *w = d->word;

    for (k = 1; k <= d->n; k++) {
        char a = w[2 * k - 2], c = w[2 * k - 1];

        s->next[k] = 0;
        if (a == d->open) {
            s->first[blocks] = k;
            s->tail[blocks] = k;
            if (c == d->open)
                s->stack[top++] = blocks;
            blocks++;
        } else {
            b = s->stack[top - 1];
            s->next[s->tail[b]] = k;
            s->tail[b] = k;
            if (c == d->close)
                top--;
        }
    }
    for (b = 0; b < blocks; b++) {
        if (b > 0)
            out_char(o, ' ');
        out_char(o, '{');
        for (k = s->first[b]; k != 0; k = s->next[k]) {
            if (k != s->first[b])
                out_char(o, ',');
            out_uint(o, (uint64_t)k);
        }
        out_char(o, '}');
    }
}

static void emit(const dyck_t *d, enum format f, scratch_t *s, out_t *o)
{
    int j;

    switch (f) {
    case F_PARENS:
    case F_BITS:
        out_bytes(o, d->word, 2 * (size_t)d->n);
        break;
    case F_PAIRS:
        emit_pairs(d, s, o);
        break;
    case F_NCP:
        emit_ncp(d, s, o);
        break;
    case F_POSITIONS:
        for (j = 0; j < d->n; j++) {
            if (j > 0)
                out_char(o, ' ');
            out_uint(o, (uint64_t)d->pos[j]);
        }
        break;
    }
    out_char(o, '\n');
}

/* --------------------------------------------------------------- arguments */

static int parse_n(const char *arg, int max, int *n)
{
    char *end;
    long v;

    errno = 0;
    v = strtol(arg, &end, 10);
    if (errno != 0 || end == arg || *end != '\0' || v < 1 || v > max) {
        fprintf(stderr, "dyck: N must be a whole number from 1 to %d, not '%s'\n", max, arg);
        return -1;
    }
    *n = (int)v;
    return 0;
}

static int parse_rank(const char *arg, uint64_t *rank)
{
    char *end;
    unsigned long long v;

    errno = 0;
    v = strtoull(arg, &end, 10);
    if (errno != 0 || end == arg || *end != '\0' || arg[0] == '-' || arg[0] == '+') {
        fprintf(stderr, "dyck: not a rank: '%s'\n", arg);
        return -1;
    }
    *rank = (uint64_t)v;
    return 0;
}

/* Reads one line into buf, without its line ending. Returns 0 at end of
   input, -1 for a line too long for buf, 1 otherwise. */
static int read_line(char *buf, size_t size)
{
    size_t len;
    int c;

    if (fgets(buf, (int)size, stdin) == NULL)
        return 0;
    len = strlen(buf);
    if (len > 0 && buf[len - 1] != '\n' && !feof(stdin)) {
        while ((c = getchar()) != '\n' && c != EOF)
            ;
        return -1;
    }
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
    return 1;
}

/* ------------------------------------------------------------------- modes */

static int run_enumerate(int n, enum format f)
{
    static out_t o;
    scratch_t s;
    dyck_t d;
    int status = 0;

    if (dyck_init(&d, n, f == F_BITS ? '1' : '(', f == F_BITS ? '0' : ')') != 0
        || scratch_init(&s, n) != 0) {
        fprintf(stderr, "dyck: out of memory\n");
        return EXIT_FAILURE;
    }
    if (f == F_PARENS || f == F_BITS) {
        /* The common case, kept to one copy per word. */
        do {
            out_bytes(&o, d.word, 2 * (size_t)n);
            out_char(&o, '\n');
        } while (dyck_next(&d));
    } else {
        do
            emit(&d, f, &s, &o);
        while (dyck_next(&d));
    }
    out_flush(&o);
    if (o.failed || fflush(stdout) != 0) {
        perror("dyck: write");
        status = EXIT_FAILURE;
    }
    scratch_free(&s);
    dyck_free(&d);
    return status;
}

/* Walks every word, as run_enumerate does, and writes only the total. Where
   the Catalan number fits in 64 bits the total is checked against it. */
static int run_count(int n)
{
    uint64_t count = 0, expected = dyck_catalan(n);
    dyck_t d;

    if (dyck_init(&d, n, '(', ')') != 0) {
        fprintf(stderr, "dyck: out of memory\n");
        return EXIT_FAILURE;
    }
    do
        count++;
    while (dyck_next(&d));
    dyck_free(&d);
    printf("%" PRIu64 "\n", count);
    if (expected != 0 && count != expected) {
        fprintf(stderr, "dyck: counted %" PRIu64 " words, but C(%d) = %" PRIu64 "\n",
                count, n, expected);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int rank_one(const char *text, dyck_t *d)
{
    size_t len = strlen(text);
    int n = (int)(len / 2);

    if (len == 0 || len % 2 != 0 || n > DYCK_RANK_MAX_N) {
        fprintf(stderr, "dyck: not a Dyck word of up to %d letters: '%s'\n",
                2 * DYCK_RANK_MAX_N, text);
        return -1;
    }
    if (d->word == NULL || d->n != n) {
        dyck_free(d);
        if (dyck_init(d, n, '(', ')') != 0) {
            fprintf(stderr, "dyck: out of memory\n");
            return -1;
        }
    }
    if (dyck_parse(d, text) != 0) {
        fprintf(stderr, "dyck: not a Dyck word: '%s'\n", text);
        return -1;
    }
    printf("%" PRIu64 "\n", dyck_rank(d));
    return 0;
}

static int run_rank(const char *arg)
{
    char line[2 * DYCK_RANK_MAX_N + 3];
    dyck_t d = { 0, NULL, NULL, '(', ')' };
    int r, status = EXIT_SUCCESS;

    if (strcmp(arg, "-") != 0)
        status = rank_one(arg, &d) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    else
        while ((r = read_line(line, sizeof line)) != 0) {
            if (r < 0) {
                fprintf(stderr, "dyck: line too long for a word of up to %d letters\n",
                        2 * DYCK_RANK_MAX_N);
                status = EXIT_FAILURE;
                break;
            }
            if (rank_one(line, &d) != 0) {
                status = EXIT_FAILURE;
                break;
            }
        }
    dyck_free(&d);
    return status;
}

static int unrank_one(uint64_t rank, dyck_t *d, enum format f, scratch_t *s, out_t *o)
{
    if (dyck_unrank(d, rank) != 0) {
        fprintf(stderr, "dyck: rank %" PRIu64 " is not between 1 and C(%d) = %" PRIu64 "\n",
                rank, d->n, dyck_catalan(d->n));
        return -1;
    }
    emit(d, f, s, o);
    return 0;
}

static int run_unrank(const char *arg, int n, enum format f)
{
    static out_t o;
    char line[32];
    scratch_t s;
    dyck_t d;
    uint64_t rank;
    int r, status = EXIT_SUCCESS;

    if (dyck_init(&d, n, f == F_BITS ? '1' : '(', f == F_BITS ? '0' : ')') != 0
        || scratch_init(&s, n) != 0) {
        fprintf(stderr, "dyck: out of memory\n");
        return EXIT_FAILURE;
    }
    if (strcmp(arg, "-") != 0) {
        if (parse_rank(arg, &rank) != 0 || unrank_one(rank, &d, f, &s, &o) != 0)
            status = EXIT_FAILURE;
    } else {
        while ((r = read_line(line, sizeof line)) != 0) {
            if (r < 0 || parse_rank(line, &rank) != 0 || unrank_one(rank, &d, f, &s, &o) != 0) {
                if (r < 0)
                    fprintf(stderr, "dyck: line too long for a rank\n");
                status = EXIT_FAILURE;
                break;
            }
        }
    }
    out_flush(&o);
    if (o.failed || fflush(stdout) != 0) {
        perror("dyck: write");
        status = EXIT_FAILURE;
    }
    scratch_free(&s);
    dyck_free(&d);
    return status;
}

int main(int argc, char *argv[])
{
    enum format f = F_PARENS;
    const char *rank_arg = NULL, *unrank_arg = NULL;
    int count = 0, n, i, k;

    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            fputs(usage, stdout);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-c") == 0) {
            count = 1;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            for (k = 0; k < (int)(sizeof format_names / sizeof *format_names); k++)
                if (strcmp(argv[i + 1], format_names[k]) == 0)
                    break;
            if (k == (int)(sizeof format_names / sizeof *format_names)) {
                fprintf(stderr, "dyck: unknown format '%s'\n", argv[i + 1]);
                return EXIT_USAGE;
            }
            f = (enum format)k;
            i++;
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            rank_arg = argv[++i];
        } else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
            unrank_arg = argv[++i];
        } else {
            fputs(usage, stderr);
            return EXIT_USAGE;
        }
    }

    if (count + (rank_arg != NULL) + (unrank_arg != NULL) > 1) {
        fprintf(stderr, "dyck: -c, -r and -u do not combine\n");
        return EXIT_USAGE;
    }
    if (rank_arg != NULL) {
        if (i != argc) {
            fputs(usage, stderr);
            return EXIT_USAGE;
        }
        return run_rank(rank_arg);
    }
    if (i + 1 != argc) {
        fputs(usage, stderr);
        return EXIT_USAGE;
    }
    if (unrank_arg != NULL) {
        if (parse_n(argv[i], DYCK_RANK_MAX_N, &n) != 0)
            return EXIT_USAGE;
        return run_unrank(unrank_arg, n, f);
    }
    if (parse_n(argv[i], DYCK_MAX_N, &n) != 0)
        return EXIT_USAGE;
    return count ? run_count(n) : run_enumerate(n, f);
}
