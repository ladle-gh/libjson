#ifndef LADLE_JSON_H
#define LADLE_JSON_H
#include <float.h>          // floating-point constants
#include <stddef.h>         // size_t
#include <stdio.h>          // FILE
#ifndef __cplusplus
#include <stdbool.h>    // bool
#endif

#include <ladle/common/defs.h>

// Ensures parsing of .json files does not result in precision loss
#if DBL_DIG < 15
#if LDBL_DIG < 15
#pragma GCC warning                     \
    "64-bit precision not supported. "  \
    "Parsing .json files may cause precision loss."
#endif
typedef long double jfloat_t;
#define JFLT_MANT_DIG   LDBL_MANT_DIG
#define JFLT_DIG        LDBL_DIG
#define JFLT_MIN_EXP    LDBL_MIN_EXP
#define JFLT_MIN_10_EXP LDBL_MIN_10_EXP
#define JFLT_MAX_EXP    LDBL_MAX_EXP
#define JFLT_MAX_10_EXP LDBL_MAX_10_EXP
#define JFLT_MAX        LDBL_MAX
#define JFLT_EPSILON    LDBL_EPSILON
#define JFLT_MIN        LDBL_MIN
#if (defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) \
    || (defined (__cplusplus) && __cplusplus >= 201703L)
#define JFLT_DECIMAL_DIG    LDBL_DECIMAL_DIG
#define JFLT_HAS_SUBNORM    LDBL_HAS_SUBNORM
#define JFLT_TRUE_MIN       LDBL_TRUE_MIN
#endif  // C11 || C++17
#else
typedef double jfloat_t;
#define JFLT_MANT_DIG   DBL_MANT_DIG
#define JFLT_DIG        DBL_DIG
#define JFLT_MIN_EXP    DBL_MIN_EXP
#define JFLT_MIN_10_EXP DBL_MIN_10_EXP
#define JFLT_MAX_EXP    DBL_MAX_EXP
#define JFLT_MAX_10_EXP DBL_MAX_10_EXP
#define JFLT_MAX        DBL_MAX
#define JFLT_EPSILON    DBL_EPSILON
#define JFLT_MIN        DBL_MIN
#if (defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) \
    || (defined (__cplusplus) && __cplusplus >= 201703L)
#define JFLT_DECIMAL_DIG    DBL_DECIMAL_DIG
#define JFLT_HAS_SUBNORM    DBL_HAS_SUBNORM
#define JFLT_TRUE_MIN       DBL_TRUE_MIN
#endif  // C11 || C++17
#endif

// Use implementation-specific 'restrict' for C++, if possible
#ifdef __cplusplus
#ifdef __GNUC__
#define restrict    __restrict__
#elif defined(_MSC_VER)
#define restrict    __restrict
#else
#define restrict
#endif
#endif  // C++

// Capacity of newly-allocated jarray_t
#define JARRAY_DEFCAP  8

// Used to tell what type a JSON entry is
typedef enum jtype_t {J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ} jtype_t;

// JSON array
typedef struct jarray_t {
    size_t size, capacity;
    struct jvalue_t **values;
} jarray_t;

// JSON value
typedef union jany_t {
    bool boolean;
    jfloat_t number;
    char *string;
    struct jarray_t *array;
    struct json_t *object;
} jany_t;

// JSON object
typedef struct json_t {
    struct jentry_t *root;
} json_t;

// JSON value
typedef struct jvalue_t {
    char type;
    union jany_t value;
} jvalue_t;

// Frees memory held within a JSON array
void jarray_free(jarray_t *array)
attribute(nonnull, nothrow);

// Frees memory held within a JSON object
void json_free(json_t *json)
attribute(nonnull, nothrow);

// Frees memory held within a JSON value
void jvalue_free(jvalue_t *value)
attribute(nonnull, nothrow);

bool jarray_pushb(jarray_t *array, const jvalue_t *restrict value)
attribute(nonnull, nothrow);

bool jarray_pushf(jarray_t *array, const jvalue_t *restrict value)
attribute(nonnull, nothrow);

bool jarray_remove(jarray_t *restrict array, size_t index)
attribute(nonnull, nothrow);

bool jarray_sort(jarray_t *restrict array)
attribute(nonnull, nothrow);

/* Adds a value to a JSON object
 * Returns true on normal operation
 * Returns false and sets errno accordingly on error */
bool json_add(json_t *json, const char *key, const jvalue_t *value)
attribute(nonnull, nothrow);

bool json_print(const json_t *json, const FILE *file, size_t indent)
attribute(nonnull, nothrow);

/* Removes a value from a JSON object
 * Returns true on normal operation
 * Returns false and sets errno accordingly on error */
bool json_remove(json_t *json, const char *key);

/* Modifies a JSON value
 * Returns true on normal operation
 * Returns false and sets errno accordingly on error */
bool jvalue_modify(jvalue_t *value, const jvalue_t *NEW_VALUE)
attribute(nonnull, nothrow);

int jvalue_cmp(const jvalue_t *value1, const jvalue_t *value2)
attribute(nonnull, nothrow);

bool jarray_pushb(jarray_t *array, const jvalue_t *value)
attribute(nonnull, nothrow);

bool jarray_pushf(jarray_t *array, const jvalue_t *value)
attribute(nonnull, nothrow);

bool jarray_remove(jarray_t *array, size_t index)
attribute(nonnull, nothrow);

bool jarray_sort(jarray_t *array)
attribute(nonnull, nothrow);

size_t json_size(const json_t *restrict json)
attribute(nonnull, nothrow);

jarray_t *jarray_copy(const jarray_t *restrict array)
attribute(nonnull, nothrow, warn_unused_result);

/* Generates a new, empty JSON array
 * Returns NULL and sets errno accordingly on error */
jarray_t *jarray_new(void)
attribute(nothrow, warn_unused_result);

json_t *json_copy(const json_t *json)
attribute(nonnull, nothrow, warn_unused_result);

/* Generates a new, empty JSON object
 * Returns NULL and sets errno to ENOMEM on error */
json_t *json_new(void)
attribute(nothrow, warn_unused_result);

/* Generates a new JSON object from a .json file
 * Returns NULL and sets errno accordingly on error
 *
 * Show errors here */
json_t *json_parse(const FILE *file)
attribute(nonnull, nothrow, warn_unused_result);

jvalue_t *jarray_findf(jarray_t *array, const jvalue_t *value)
attribute(nonnull, nothrow);

jvalue_t *jarray_findfn(jarray_t *array, const jvalue_t *value)
attribute(nonnull, nothrow);

jvalue_t *jarray_findl(jarray_t *array, const jvalue_t *value)
attribute(nonnull, nothrow);

jvalue_t *jarray_findln(jarray_t *array, const jvalue_t *value)
attribute(nonnull, nothrow);

jvalue_t *jarray_get(const jarray_t *array, size_t index)
attribute(nonnull, nothrow);

jvalue_t *jarray_popf(jarray_t *array)
attribute(nonnull, nothrow, warn_unused_result);

jvalue_t *jarray_popb(jarray_t *array)
attribute(nonnull, nothrow, warn_unused_result);

/* Returns the value of the given key within the JSON object
 * Returns NULL on error or if no value is found */
jvalue_t *json_find(const json_t *json, const char *key)
attribute(nonnull, nothrow);

jvalue_t *jvalue_copy(const jvalue_t *restrict value)
attribute(nothrow, warn_unused_result);

jvalue_t *jvalue_new(char type, const jany_t value)
attribute(nothrow, warn_unused_result);

#include <ladle/common/undefs.h>
#endif  // #ifndef LADLE_JSON_H
