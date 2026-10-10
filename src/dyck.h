/*
 * dyck.h - the Dyck words of length 2n, one after the other, in lexicographic
 * order, and the rank of each one in that order.
 *
 * A word is held as the positions of its n opening letters. pos[0] is always
 * 1, and pos[j] can be anywhere from pos[j-1] + 1 to 2j + 1, its "home": the
 * place it has in the smallest word, ()()...(). Moving any opening letter
 * left makes the word bigger, so the sequence runs from ()()...() to
 * ((...)), which is the order of the numbers the words spell in binary with
 * ( as 1 and ) as 0.
 *
 * The algorithm is the one Kostas Manes and Vassilios Karakoidas wrote in
 * 2002, and the ranking is Kostas Manes's of 2004.
 */
#ifndef DYCK_H
#define DYCK_H

#include <stdint.h>

/* The largest n accepted at all; it keeps 2n and the positions far from
   INT_MAX. */
#define DYCK_MAX_N 1000000

/* The largest n whose Catalan number, and so every rank, fits in 64 bits. */
#define DYCK_RANK_MAX_N 36

typedef struct {
    int n;
    int *pos;      /* pos[j]: 1-based position of the (j+1)-th opening letter */
    char *word;    /* the current word: 2n letters and a NUL */
    char open;     /* the two letters the word is spelled with */
    char close;
} dyck_t;

/* Allocates d and sets it to the first word, ()()...(), spelled with the two
   letters given. Returns 0, or -1 if n is out of range or memory ran out. */
int dyck_init(dyck_t *d, int n, char open, char close);
void dyck_free(dyck_t *d);

/* The successor's slow half; dyck_next() is the whole of it. */
int dyck_push(dyck_t *d);

/*
 * Advances d to the next word and returns 1, or returns 0 and leaves d alone
 * if it already held the last one.
 *
 * Shift: if the last opening letter has room, it moves one place left. That
 * is the case for most words, and it costs two letters, so it is kept here
 * where it can be inlined into the caller's loop. Otherwise dyck_push()
 * finds the opening letter to move.
 */
static inline int dyck_next(dyck_t *d)
{
    int last = d->n - 1;
    int *p = d->pos;

    if (last > 0 && p[last] > p[last - 1] + 1) {
        d->word[p[last] - 1] = d->close;
        p[last]--;
        d->word[p[last] - 1] = d->open;
        return 1;
    }
    return dyck_push(d);
}

/* C(n), or 0 if it does not fit in 64 bits (n > DYCK_RANK_MAX_N). */
uint64_t dyck_catalan(int n);

/* The rank of d's word, counting from 1 for ()()...(); 0 if n is over
   DYCK_RANK_MAX_N. */
uint64_t dyck_rank(const dyck_t *d);

/* Sets d to the word of the given rank. Returns 0, or -1 if rank is not in
   1..C(n) or n is over DYCK_RANK_MAX_N; d is unchanged then. */
int dyck_unrank(dyck_t *d, uint64_t rank);

/* Sets d to the word in text, written with ( and ) or with 1 and 0. Returns
   0, or -1 if text is not a Dyck word of length 2n; d is unchanged then. */
int dyck_parse(dyck_t *d, const char *text);

#endif
