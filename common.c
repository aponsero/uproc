/* Copyright 2014 Peter Meinicke, Robin Martinjak
 *
 * This file is part of uproc.
 *
 * uproc is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * uproc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with uproc.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#if HAVE__MKDIR
#include <direct.h>
#elif HAVE_MKDIR
#include <sys/stat.h>
#endif

#include <uproc.h>

#include "common.h"

#define PROGRESS_WIDTH 20

uproc_io_stream *open_read(const char *path)
{
    if (!strcmp(path, "-")) {
        return uproc_stdin;
    }
    return uproc_io_open("r", UPROC_IO_GZIP, "%s", path);
}

uproc_io_stream *open_write(const char *path, enum uproc_io_type type)
{
    if (!strcmp(path, "-")) {
        return type == UPROC_IO_GZIP ? uproc_stdout_gz : uproc_stdout;
    }
    return uproc_io_open("w", type, "%s", path);
}

void print_version(const char *progname)
{
    printf(
        "%s, version " UPROC_VERSION
        "\n"
        "Copyright 2014 Peter Meinicke, Robin Martinjak\n"
        "License GPLv3+: GNU GPL version 3 or later " /* no line break! */
        "<http://gnu.org/licenses/gpl.html>\n"
        "\n"
        "This is free software; you are free to change and redistribute it.\n"
        "There is NO WARRANTY, to the extent permitted by law.\n"
        "\n"
        "Please send bug reports to " PACKAGE_BUGREPORT "\n",
        progname);
}

int parse_int(const char *arg, int *x)
{
    char *end;
    int tmp = strtol(arg, &end, 10);
    if (!*arg || *end) {
        return -1;
    }
    *x = tmp;
    return 0;
}

int parse_prot_thresh_level(const char *arg, int *x)
{
    int tmp;
    if (parse_int(arg, &tmp)) {
        return -1;
    }
    if (tmp != 0 && tmp != 2 && tmp != 3) {
        return -1;
    }
    *x = tmp;
    return 0;
}

int parse_orf_thresh_level(const char *arg, int *x)
{
    int tmp;
    if (parse_int(arg, &tmp)) {
        return -1;
    }
    if (tmp != 0 && tmp != 1 && tmp != 2) {
        return -1;
    }
    *x = tmp;
    return 0;
}

void errhandler_bail(enum uproc_error_code num, const char *msg,
                     const char *loc, void *context)
{
    (void)num;
    (void)msg;
    (void)loc;
    (void)context;
    uproc_perror("");
    exit(EXIT_FAILURE);
}

void trim_header(char *s)
{
    char *start = s, *end;
    while (isspace(*start) || *start == ',') {
        start++;
    }
    end = strpbrk(start, ", \f\n\r\t\v");
    if (end) {
        *end = '\0';
    }
}

void make_dir(const char *path)
{
#if HAVE__MKDIR
    _mkdir(path);
#elif HAVE_MKDIR
    mkdir(path, 0777);
#endif
}

static bool prot_filter(const char *seq, size_t len, uproc_class class,
                        double score, void *opaque)
{
    (void)seq;
    (void)class;
    unsigned long rows, cols;
    uproc_matrix *thresh = opaque;
    if (!thresh) {
        return score > UPROC_EPSILON;
    }
    uproc_matrix_dimensions(thresh, &rows, &cols);
    if (len >= rows) {
        len = rows - 1;
    }
    return score >= uproc_matrix_get(thresh, len, 0);
}

static bool orf_filter(const struct uproc_orf *orf, const char *seq,
                       size_t seq_len, double seq_gc, void *opaque)
{
    unsigned long r, c, rows, cols;
    uproc_matrix *thresh = opaque;
    (void)seq;
    if (orf->length < 20) {
        return false;
    }
    if (!thresh) {
        return true;
    }
    uproc_matrix_dimensions(thresh, &rows, &cols);
    r = seq_gc * 100;
    c = seq_len;
    if (r >= rows) {
        r = rows - 1;
    }
    if (c >= cols) {
        c = cols - 1;
    }
    return orf->score >= uproc_matrix_get(thresh, r, c);
}

int create_classifiers(uproc_protclass **pc, uproc_dnaclass **dc,
                       uproc_database *db, uproc_model *model,
                       int prot_thresh_level, bool short_read_mode,
                       bool detailed_mode)
{
    if (!db) {
        uproc_error_msg(UPROC_EINVAL, "database parameter must not be NULL");
        return -1;
    }
    if (!model) {
        uproc_error_msg(UPROC_EINVAL, "model parameter must not be NULL");
        return -1;
    }

    enum uproc_protclass_mode pc_mode = UPROC_PROTCLASS_ALL;
    enum uproc_dnaclass_mode dc_mode = UPROC_DNACLASS_ALL;

    if (dc && short_read_mode) {
        pc_mode = UPROC_PROTCLASS_MAX;
        dc_mode = UPROC_DNACLASS_MAX;
    }
    uproc_matrix *prot_thresh =
        uproc_database_protein_threshold(db, prot_thresh_level);

    *pc = uproc_protclass_create(
        pc_mode, detailed_mode, uproc_database_ecurve(db, UPROC_ECURVE_FWD),
        uproc_database_ecurve(db, UPROC_ECURVE_REV),
        uproc_model_substitution_matrix(model), prot_filter, prot_thresh);
    if (!*pc) {
        return -1;
    }

    if (!dc) {
        return 0;
    }

    *dc = uproc_dnaclass_create(
        dc_mode, *pc, uproc_model_codon_scores(model), orf_filter,
        short_read_mode ? NULL : uproc_model_orf_threshold(model));

    if (!*dc) {
        uproc_protclass_destroy(*pc);
        return -1;
    }
    return 0;
}

void progress(uproc_io_stream *stream, const char *new_label, double percent)
{
    static const char *label;
    static double last_percent;
    if (new_label) {
        last_percent = -1.0;
        label = new_label;
    }
    if (percent < 0.0 || last_percent >= 100.0 ||
        (!new_label && fabs(percent - last_percent) < 0.05 &&
         percent < 100.0)) {
        return;
    }
    char bar[PROGRESS_WIDTH + 1] = "";
    int p = percent / 100.0 * PROGRESS_WIDTH;
    for (int i = 0; i < PROGRESS_WIDTH; i++) {
        bar[i] = i < p ? '#' : ' ';
    }
    uproc_io_printf(stream, "\r%s: [%s] %5.1f%%", label, bar, percent);
    if (percent >= 100.0) {
        uproc_io_printf(stream, "\n");
    }
    last_percent = percent;
}
