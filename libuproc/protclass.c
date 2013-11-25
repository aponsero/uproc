#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "uproc/common.h"
#include "uproc/error.h"
#include "uproc/bst.h"
#include "uproc/protclass.h"



/*********************
 * score computation *
 *********************/

struct sc
{
    size_t index;
    double total, dist[UPROC_WORD_LEN];
};

static void
reverse_array(void *p, size_t n, size_t sz)
{
    unsigned char *s = p, tmp;
    size_t i, k, i1, i2;

    for (i = 0; i < n / 2; i++) {
        for (k = 0; k < sz; k++) {
            i1 = sz * i + k;
            i2 = sz * (n - i - 1) + k;
            tmp = s[i1];
            s[i1] = s[i2];
            s[i2] = tmp;
        }
    }
}

static void
sc_init(struct sc *s)
{
    size_t i;
    s->index = -1;
    s->total = 0.0;
    for (i = 0; i < UPROC_WORD_LEN; i++) {
        s->dist[i] = -INFINITY;
    }
}

static void
sc_add(struct sc *score, size_t index, double dist[static UPROC_SUFFIX_LEN],
       bool reverse)
{
    size_t i, diff;
    double tmp[UPROC_WORD_LEN];

    for (i = 0; i < UPROC_PREFIX_LEN; i++) {
        tmp[i] = -INFINITY;
    }
    memcpy(tmp + UPROC_PREFIX_LEN, dist, sizeof *dist * UPROC_SUFFIX_LEN);
    if (reverse) {
        reverse_array(tmp, UPROC_WORD_LEN, sizeof *tmp);
    }

    if (score->index != (size_t) -1) {
        diff = index - score->index;
        if (diff > UPROC_WORD_LEN) {
            diff = UPROC_WORD_LEN;
        }
        for (i = 0; i < diff; i++) {
            if (isfinite(score->dist[i])) {
                score->total += score->dist[i];
                score->dist[i] = -INFINITY;
            }
        }
    }
    else {
        diff = 0;
    }

    for (i = 0; i + diff < UPROC_WORD_LEN; i++) {
#define MAX(a, b) (a > b ? a : b)
        score->dist[i] = MAX(score->dist[i + diff], tmp[i]);
    }
    for (; i < UPROC_WORD_LEN; i++) {
        score->dist[i] = tmp[i];
    }
    score->index = index;
}

static double
sc_finalize(struct sc *score)
{
    size_t i;
    for (i = 0; i < UPROC_WORD_LEN; i++) {
        if (isfinite(score->dist[i])) {
            score->total += score->dist[i];
        }
    }
    return score->total;
}

static int
scores_add(struct uproc_bst *scores, uproc_family family, size_t index,
           double dist[static UPROC_SUFFIX_LEN], bool reverse)
{
    struct sc sc;
    union uproc_bst_key key = { .uint = family };
    sc_init(&sc);
    (void) uproc_bst_get(scores, key, &sc);
    sc_add(&sc, index, dist, reverse);
    return uproc_bst_update(scores, key, &sc);
}

static void
align_suffixes(double dist[static UPROC_SUFFIX_LEN], uproc_suffix s1, uproc_suffix s2,
               const struct uproc_substmat *substmat)
{
    size_t i;
    uproc_amino a1, a2;
    for (i = 0; i < UPROC_SUFFIX_LEN; i++) {
        a1 = s1 & UPROC_BITMASK(UPROC_AMINO_BITS);
        a2 = s2 & UPROC_BITMASK(UPROC_AMINO_BITS);
        s1 >>= UPROC_AMINO_BITS;
        s2 >>= UPROC_AMINO_BITS;
        if (substmat) {
            dist[i] = uproc_substmat_get(substmat, i, a1, a2);
        }
        else {
            dist[i] = a1 == a2 ? 1.0 : 0.0;
        }
    }
}

static int
scores_add_word(struct uproc_bst *scores, const struct uproc_word *word, size_t index,
                bool reverse, const struct uproc_ecurve *ecurve,
                const struct uproc_substmat *substmat)
{
    int res;
    struct uproc_word
        lower_nb = UPROC_WORD_INITIALIZER,
        upper_nb = UPROC_WORD_INITIALIZER;
    uproc_family lower_family, upper_family;
    double dist[UPROC_SUFFIX_LEN];

    if (!ecurve) {
        return 0;
    }
    uproc_ecurve_lookup(ecurve, word, &lower_nb, &lower_family, &upper_nb, &upper_family);
    align_suffixes(dist, word->suffix, lower_nb.suffix, substmat);
    res = scores_add(scores, lower_family, index, dist, reverse);
    if (res || uproc_word_equal(&lower_nb, &upper_nb)) {
        return res;
    }
    align_suffixes(dist, word->suffix, upper_nb.suffix, substmat);
    res = scores_add(scores, upper_family, index, dist, reverse);
    return res;
}

static int
scores_compute(const struct uproc_protclass *pc, const char *seq, struct uproc_bst *scores)
{
    int res;
    struct uproc_worditer iter;
    size_t index;
    struct uproc_word
        fwd_word = UPROC_WORD_INITIALIZER,
        rev_word = UPROC_WORD_INITIALIZER;

    uproc_worditer_init(&iter, seq,
        pc->fwd ? &pc->fwd->alphabet : &pc->rev->alphabet);

    while ((res = uproc_worditer_next(&iter, &index, &fwd_word, &rev_word)) > 0) {
        res = scores_add_word(scores, &fwd_word, index, false, pc->fwd,
                pc->substmat);
        if (res) {
            break;
        }
        res = scores_add_word(scores, &rev_word, index, true, pc->rev,
                pc->substmat);
        if (res) {
            break;
        }
    }
    return res;
}


/****************
 * finalization *
 ****************/

static int
scores_finalize(const struct uproc_protclass *pc, const char *seq,
        struct uproc_bst *score_tree, struct uproc_pc_results *results)
{
    int res = 0;
    struct uproc_bstiter iter;
    union uproc_bst_key key;
    struct sc value;
    size_t seq_len = strlen(seq);

    results->n = uproc_bst_size(score_tree);
    if (results->n > results->sz) {
        void *tmp;
        tmp = realloc(results->preds, results->n * sizeof *results->preds);
        if (!tmp) {
            res = uproc_error(UPROC_ENOMEM);
            goto error;
        }
        results->preds = tmp;
        results->sz = results->n;
    }

    results->n = 0;
    uproc_bstiter_init(&iter, score_tree);
    while (uproc_bstiter_next(&iter, &key, &value) > 0) {
        uproc_family family = key.uint;
        double score = sc_finalize(&value);
        if (pc->filter &&
                !pc->filter(seq, seq_len, family, score, pc->filter_arg)) {
            continue;
        }
        results->preds[results->n].score = score;
        results->preds[results->n].family = family;
        results->n++;
    }

    if (pc->mode == UPROC_PC_MAX && results->n > 0) {
        size_t imax = 0;
        for (size_t i = 1; i < results->n; i++) {
            if (results->preds[i].score > results->preds[imax].score) {
                imax = i;
            }
        }
        results->preds[0] = results->preds[imax];
        results->n = 1;
    }
    res = 0;
error:
    return res;
}


/**********************
 * exported functions *
 **********************/

int
uproc_pc_init(struct uproc_protclass *pc,
        enum uproc_pc_mode mode,
        const struct uproc_ecurve *fwd,
        const struct uproc_ecurve *rev,
        const struct uproc_substmat *substmat,
        uproc_pc_filter *filter, void *filter_arg)
{
    if (!(fwd || rev)) {
        return uproc_error_msg(
            UPROC_EINVAL, "protein classifier requires at least one ecurve");
    }
    *pc = (struct uproc_protclass) {
        .mode = mode,
        .substmat = substmat,
        .fwd = fwd,
        .rev = rev,
        .filter = filter,
        .filter_arg = filter_arg,
    };
    return 0;
}

static int
classify(const struct uproc_protclass *pc, const char *seq,
        struct uproc_pc_results *results)
{
    int res;
    struct uproc_bst scores;

    uproc_bst_init(&scores, UPROC_BST_UINT, sizeof (struct sc));
    res = scores_compute(pc, seq, &scores);
    if (res || uproc_bst_isempty(&scores)) {
        results->n = 0;
        goto error;
    }
    res = scores_finalize(pc, seq, &scores, results);
error:
    uproc_bst_clear(&scores, NULL);
    return res;
}

int
uproc_pc_classify(const struct uproc_protclass *pc, const char *seq,
        struct uproc_pc_results *results)
{
    int res;
    size_t i, imax = 0;
    res = classify(pc, seq, results);
    if (res || !results->n) {
        return res;
    }
    if (pc->mode == UPROC_PC_MAX) {
        for (i = 1; i < results->n; i++) {
            if (results->preds[i].score > results->preds[imax].score) {
                imax = i;
            }
        }
        results->preds[0] = results->preds[imax];
        results->n = 1;
    }
    return 0;
}
