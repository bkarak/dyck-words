/*
 * dyck.c - the successor, the ranking and the unranking.
 */
#include <stdlib.h>
#include <string.h>

#include "dyck.h"

int dyck_init(dyck_t *d, int n, char open, char close)
{
    int j;

    if (n < 1 || n > DYCK_MAX_N)
        return -1;
    d->n = n;
    d->open = open;
    d->close = close;
    d->pos = malloc((size_t)n * sizeof *d->pos);
    d->word = malloc(2 * (size_t)n + 1);
    if (d->pos == NULL || d->word == NULL) {
        dyck_free(d);
        return -1;
    }
    for (j = 0; j < n; j++) {
        d->pos[j] = 2 * j + 1;
        d->word[2 * j] = open;
        d->word[2 * j + 1] = close;
    }
    d->word[2 * n] = '\0';
    return 0;
}

void dyck_free(dyck_t *d)
{
    free(d->pos);
    free(d->word);
    d->pos = NULL;
    d->word = NULL;
}

/*
 * Push: the last opening letter sits right after the one before it, so it
 * cannot move. Walk left along that run of adjacent opening letters to the
 * first one that has room, move it one place left, and send every opening
 * letter after it back home. If the run reaches pos[0], every letter is as
 * far left as it can go and this was (((...))).
 */
int dyck_push(dyck_t *d)
{
    int *p = d->pos;
    char *w = d->word;
    int n = d->n, i = n - 1, j;

    while (i > 0 && p[i] == p[i - 1] + 1)
        i--;
    if (i == 0)
        return 0;

    w[p[i] - 1] = d->close;
    p[i]--;
    w[p[i] - 1] = d->open;

    /* Clear them all before setting any: a home can be where another stood. */
    for (j = i + 1; j < n; j++)
        w[p[j] - 1] = d->close;
    for (j = i + 1; j < n; j++) {
        p[j] = 2 * j + 1;
        w[p[j] - 1] = d->open;
    }
    return 1;
}

uint64_t dyck_catalan(int n)
{
    uint64_t c = 1, g, a, b, t;
    int i;

    if (n < 0 || n > DYCK_RANK_MAX_N)
        return 0;
    /* C(i+1) = C(i) * 2(2i+1) / (i+2). Dividing out the common factor first
       keeps the product inside 64 bits whenever C(i+1) itself fits. */
    for (i = 0; i < n; i++) {
        a = c;
        b = (uint64_t)i + 2;
        while (b != 0) {
            t = a % b;
            a = b;
            b = t;
        }
        g = a;
        c = (c / g) * ((uint64_t)(4 * i + 2) / ((uint64_t)(i + 2) / g));
    }
    return c;
}

/*
 * Kostas Manes's weights. Number the opening letters from the right, so the
 * i-th has home 2i-1 counted from the right-hand end, and call a letter that
 * sits k places left of its home "moved k". Then d(i,k) is the number of
 * words that come before the first word in which the i-th letter is moved k.
 *
 *     d(i,0) = 0,   d(i,1) = C(i),   d(i,k) = d(i+1,k-1) - d(i+1,k-2)
 *
 * and the rank of a word is 1 plus the sum of d(i,k) over its letters. The
 * values do not depend on n, so one table serves every n up to the limit.
 * It is Catalan's triangle, d(i,k) = k/(i+k) * binomial(2i+k-1, i).
 */
static uint64_t weight[DYCK_RANK_MAX_N][DYCK_RANK_MAX_N + 1];

static void fill_weights(void)
{
    static int filled = 0;
    int i, k;

    if (filled)
        return;
    for (i = DYCK_RANK_MAX_N - 1; i >= 1; i--) {
        weight[i][0] = 0;
        weight[i][1] = dyck_catalan(i);
        for (k = 2; k <= DYCK_RANK_MAX_N - i; k++)
            weight[i][k] = weight[i + 1][k - 1] - weight[i + 1][k - 2];
    }
    filled = 1;
}

/* Counting from the left, the j-th opening letter is the (n-j)-th from the
   right, and it is moved (2j+1) - pos[j]. */
uint64_t dyck_rank(const dyck_t *d)
{
    uint64_t rank = 1;
    int j, n = d->n;

    if (n > DYCK_RANK_MAX_N)
        return 0;
    fill_weights();
    for (j = 1; j < n; j++)
        rank += weight[n - j][2 * j + 1 - d->pos[j]];
    return rank;
}

/*
 * Left to right, give each opening letter the largest move whose weight
 * still fits in what is left of the rank. A letter can be moved at most one
 * place more than the letter before it, or it would collide with it.
 */
int dyck_unrank(dyck_t *d, uint64_t rank)
{
    int n = d->n, j, k, prev = 0;
    uint64_t left;

    if (n > DYCK_RANK_MAX_N || rank < 1 || rank > dyck_catalan(n))
        return -1;
    fill_weights();
    left = rank - 1;
    memset(d->word, d->close, 2 * (size_t)n);
    d->pos[0] = 1;
    d->word[0] = d->open;
    for (j = 1; j < n; j++) {
        k = prev + 1;
        while (k > 0 && weight[n - j][k] > left)
            k--;
        left -= weight[n - j][k];
        d->pos[j] = 2 * j + 1 - k;
        d->word[d->pos[j] - 1] = d->open;
        prev = k;
    }
    return 0;
}

int dyck_parse(dyck_t *d, const char *text)
{
    int n = d->n, i, j = 0, height = 0;

    if (strlen(text) != 2 * (size_t)n)
        return -1;
    for (i = 0; i < 2 * n; i++) {
        if (text[i] == '(' || text[i] == '1')
            height++;
        else if (text[i] == ')' || text[i] == '0')
            height--;
        else
            return -1;
        if (height < 0 || height > 2 * n - i - 1)
            return -1;
    }
    for (i = 0; i < 2 * n; i++) {
        if (text[i] == '(' || text[i] == '1') {
            d->pos[j++] = i + 1;
            d->word[i] = d->open;
        } else {
            d->word[i] = d->close;
        }
    }
    return 0;
}
