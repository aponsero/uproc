#ifndef UPROC_WORD_H
#define UPROC_WORD_H

/** \file uproc/word.h
 *
 * Manipulate amino were acid words
 */

#include <stdint.h>
#include <stdbool.h>
#include "uproc/alphabet.h"

/** Struct defining an amino acid word */
struct uproc_word {
    /** First few amino acids */
    uproc_prefix prefix;

    /** Last few amino acids */
    uproc_suffix suffix;
};

/** Initializer to be used for all `struct uproc_word` objects */
#define UPROC_WORD_INITIALIZER { 0, 0 }

/** Transform a string to amino acid word
 *
 * Translates the first `#UPROC_WORD_LEN` characters of the given string to a word
 * object. Failure occurs if the string ends or an invalid character is
 * encountered.
 *
 * \param word  _OUT_: amino acid word
 * \param str   string representation
 * \param alpha alphabet to use for translation
 *
 * \retval #UPROC_SUCCESS  the string was translated successfully
 * \retval #UPROC_FAILURE  the string was too short or contained an invalid character
 */
int uproc_word_from_string(struct uproc_word *word, const char *str,
                        const struct uproc_alphabet *alpha);

/** Build string corresponding to amino acid word
 *
 * Translates an amino acid word back to a string. The string will be
 * null-terminated, thus `str` should point to a buffer of at least
 * `#UPROC_WORD_LEN + 1` bytes.
 *
 * \param str   buffer to store the string in
 * \param word  amino acid word
 * \param alpha alphabet to use for translation
 *
 * \retval #UPROC_SUCCESS  the word was translated successfully
 * \retval #UPROC_FAILURE  the word contained an invalid amino acid
 */
int uproc_word_to_string(char *str, const struct uproc_word *word,
                      const struct uproc_alphabet *alpha);

/** Append amino acid
 *
 * The leftmost amino acid of `word` is removed, for example
 *   `append(ANERD, S) == NERDS`
 * (in case of a word length of 5).
 *
 * NOTE: It will _not_ be verified whether `amino` is a valid amino acid
 *
 * \param word  word to append to
 * \param amino amino acid to append
 */
void uproc_word_append(struct uproc_word *word, uproc_amino amino);

/** Prepend amino acid
 *
 * Complementary to the append operation, e.g.
 *   `prepend(NERDS, A) == ANERD`.
 *
 * \param word  word to prepend to
 * \param amino amino acid to prepend
 */
void uproc_word_prepend(struct uproc_word *word, uproc_amino amino);

/** Compare first amino acid of a word */
bool uproc_word_startswith(const struct uproc_word *word, uproc_amino amino);

/** Test for equality
 *
 * \return `true` if the words are equal or `false` if not.
 */
bool uproc_word_equal(const struct uproc_word *w1, const struct uproc_word *w2);

/** Compare words
 *
 * Comparison is done lexcographically (prefix first, suffix second).
 *
 * \return  -1, 0 or 1 if `w1` is less than, equal to, or greater than `w2`.
 */
int uproc_word_cmp(const struct uproc_word *w1, const struct uproc_word *w2);

/** Iterator over all words in an amino acid sequence */
struct uproc_worditer {
    /** Iterated sequence */
    const char *sequence;

    /** Index of the next character to read */
    size_t index;

    /** Translation alphabet */
    const struct uproc_alphabet *alphabet;
};

/** Initialize an iterator over a sequence
 *
 * The iterator may not be used after the lifetime of the objects pointed to
 * by `seq` and `alpha` has ended (only the pointer value is stored).
 *
 * \param iter  _OUT_: initialized iterator
 * \param seq   sequence to iterate
 * \param alpha translation alphabet
 */
void uproc_worditer_init(struct uproc_worditer *iter, const char *seq,
                      const struct uproc_alphabet *alpha);

/** Obtain the next word(s) from a word iterator
 *
 * Invalid characters are not simply skipped, instead the first complete after
 * such character is returned next.
 *
 * \param iter      iterator
 * \param index     _OUT_: starting index of the current "forward" word
 * \param fwd_word  _OUT_: word to append the next character to
 * \param rev_word  _OUT_: word to prepend the next character to
 *
 * \retval #UPROC_ITER_YIELD   a pair of words was read
 * \retval #UPROC_ITER_STOP    the iterator is exhausted (i.e. the end of the
 *                          sequence was reached, no words were read)
 * \retval other            an error occured
 */
int uproc_worditer_next(struct uproc_worditer *iter, size_t *index,
                     struct uproc_word *fwd_word, struct uproc_word *rev_word);

#endif