#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "uproc/matrix.h"
#include "uproc/error.h"
#include "uproc/common.h"
#include "uproc/storage.h"
#include "uproc/io.h"

int
uproc_matrix_init(struct uproc_matrix *matrix, size_t rows, size_t cols,
               const double *values)
{
    if (rows == 1) {
        rows = cols;
        cols = 1;
    }
    matrix->rows = rows;
    matrix->cols = cols;

    matrix->values = malloc(rows * cols * sizeof *matrix->values);
    if (!matrix->values) {
        return uproc_error(UPROC_ENOMEM);
    }
    if (values) {
        memcpy(matrix->values, values, rows * cols * sizeof *matrix->values);
    }
    return 0;
}

void
uproc_matrix_destroy(struct uproc_matrix *matrix)
{
    free(matrix->values);
}

void
uproc_matrix_set(struct uproc_matrix *matrix, size_t row, size_t col, double value)
{
    matrix->values[row * matrix->cols + col] = value;
}

double
uproc_matrix_get(const struct uproc_matrix *matrix, size_t row, size_t col)
{
    return matrix->values[row * matrix->cols + col];
}

void
uproc_matrix_dimensions(const struct uproc_matrix *matrix, size_t *rows, size_t *cols)
{
    *rows = matrix->rows;
    *cols = matrix->cols;
}

static int
matrix_load(struct uproc_matrix *matrix, uproc_io_stream *stream)
{
    int res;
    size_t i, k, rows, cols;
    double val;
    char buf[1024];

    if (!uproc_io_gets(buf, sizeof buf, stream) ||
        sscanf(buf, UPROC_MATRIX_HEADER_SCN, &rows, &cols) != 2)
    {
        return uproc_error_msg(UPROC_EINVAL, "invalid matrix header");
    }

    res = uproc_matrix_init(matrix, rows, cols, NULL);
    if (res) {
        return res;
    }

    for (i = 0; i < rows; i++) {
        for (k = 0; k < cols; k++) {
            if (!uproc_io_gets(buf, sizeof buf, stream) ||
                sscanf(buf, "%lf", &val) != 1) {
                uproc_matrix_destroy(matrix);
                return uproc_error_msg(UPROC_EINVAL, "invalid value or EOF");
            }
            uproc_matrix_set(matrix, i, k, val);
        }
    }
    return 0;
}

int
uproc_matrix_loadv(struct uproc_matrix *matrix, enum uproc_io_type iotype,
        const char *pathfmt, va_list ap)
{
    int res;
    uproc_io_stream *stream = uproc_io_openv("r", iotype, pathfmt, ap);
    if (!stream) {
        return -1;
    }
    res = matrix_load(matrix, stream);
    (void) uproc_io_close(stream);
    return res;
}

int
uproc_matrix_load(struct uproc_matrix *matrix, enum uproc_io_type iotype,
        const char *pathfmt, ...)
{
    int res;
    va_list ap;
    va_start(ap, pathfmt);
    res = uproc_matrix_loadv(matrix, iotype, pathfmt, ap);
    va_end(ap);
    return res;
}

static int
matrix_store(const struct uproc_matrix *matrix, uproc_io_stream *stream)
{
    int res;
    size_t i, k, rows, cols;
    uproc_matrix_dimensions(matrix, &rows, &cols);
    res = uproc_io_printf(stream, UPROC_MATRIX_HEADER_PRI, rows, cols);
    if (res < 0) {
        return -1;
    }
    for (i = 0; i < rows; i++) {
        for (k = 0; k < cols; k++) {
            res = uproc_io_printf(stream, "%lf\n", uproc_matrix_get(matrix, i, k));
            if (res < 0) {
                return -1;
            }
        }
    }
    return 0;
}

int
uproc_matrix_storev(const struct uproc_matrix *matrix, enum uproc_io_type iotype,
        const char *pathfmt, va_list ap)
{
    int res;
    uproc_io_stream *stream = uproc_io_openv("w", iotype, pathfmt, ap);
    if (!stream) {
        return -1;
    }
    res = matrix_store(matrix, stream);
    uproc_io_close(stream);
    return res;
}

int
uproc_matrix_store(const struct uproc_matrix *matrix, enum uproc_io_type iotype,
        const char *pathfmt, ...)
{
    int res;
    va_list ap;
    va_start(ap, pathfmt);
    res = uproc_matrix_storev(matrix, iotype, pathfmt, ap);
    va_end(ap);
    return res;
}