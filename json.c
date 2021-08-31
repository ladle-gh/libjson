#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include "json.h"

// Ensures portability of strdup
#if !(defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))  // Not POSIX
#ifdef _MSC_VER // Using Microsoft Visual C/C++
#define strdup(string)  _strdup(string)
#else
static char *strdup(const char *STRING) {
    const size_t LEN = strlen(STRING);
    char *new_string = malloc(LEN + 1, sizeof(char));

    if (!new_string)   // malloc() fails
        return NULL;
    for (size_t i = 0; i < LEN; ++i) 
        new_string[i] = STRING[i];
    new_string[LEN] = '\0';
    return new_string;
}
#endif  // #ifndef _MSC_VER
#endif  // #if !(defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))

// Error handler
#define error(err, ret) { errno = err; return ret; }

// Returns ceiling of jfloat_t
#define jfloat_ceil(value)  \
    _Generic(value, double: ceil(value), long double: ceill(value))

// Returns balance factor of given entry
#define json_factor(node)                                           \
    ((node->lchild ? (long long) node->lchild->height + 1 : 0) -    \
    (node->rchild ? (long long) node->rchild->height + 1 : 0))

// JSON entry
typedef struct jentry_t {
    size_t height;
    char *key;
    jvalue_t *value;
    jentry_t *parent, *lchild, *rchild;
} jentry_t;

// Info for json_seek() and json_lookup()
typedef struct jinfo_t {
    int change;         // Change in height
    jentry_t *target;   // Located entry
} jinfo_t;

// Balances entry tree from given node
static void json_balance(jentry_t *root) {
    long long bal_factor;

    while ((bal_factor = json_factor(root)) < -1 || bal_factor > 1) {
        if (bal_factor >= 1)    root = root->lchild;    // Find imbalance
        else                    root = root->rchild;
    }
    while (root->parent)    // Reorder tree with imbalance as root
        root == root->parent->lchild ? rr_rotate(root) : ll_rotate(root);
}

// Frees entire entry tree from given node
static void jentry_free(jentry_t *root) {
    if (root->lchild)   jentry_free(root->lchild);
    if (root->rchild)   jentry_free(root->rchild);
    free(root->key);
    jvalue_free(root->value);
    free(root);
}

/* Performs LL rotation on a non-NULL, rchild node
 * 
 *     2               (4)
 *    / \              / \
 *   1  (4)    ->     2    5
 *      / \          / \
 *     3   5        1   3
 */
static void ll_rotate(jentry_t *node) {
    jentry_t *parent_tmp = node->parent, *child_tmp = node->lchild;

    node->lchild = parent_tmp;
    node->parent = parent_tmp->parent;
    parent_tmp->rchild = child_tmp;
    parent_tmp->parent = node;
    if (child_tmp)
        child_tmp->parent = parent_tmp;
    if (node->parent) {
        if (parent_tmp == node->parent->lchild) node->parent->lchild = node;
        else                                    node->parent->rchild = node;
    }
}

/* Performs RR rotation on a non-NULL, lchild node
 *
 *       4           (2)
 *      / \          / \
 *    (2)  5   ->   1   4
 *    / \              / \
 *   1   3            3   5
 */
static void rr_rotate(jentry_t *node) {
    jentry_t *parent_tmp = node->parent, *child_tmp = node->rchild;

    node->rchild = parent_tmp;
    node->parent = parent_tmp->parent;
    parent_tmp->lchild = child_tmp;
    parent_tmp->parent = node;
    if (child_tmp)
        child_tmp->parent = parent_tmp;
    if (node->parent) {
        if (parent_tmp == node->parent->lchild) node->parent->lchild = node;
        else                                    node->parent->rchild = node;
    }
}

// Constructs a JSON object at new root according to old root
static bool json_build(jentry_t *new_root, const jentry_t *old_root) {
    if (old_root->lchild) {
        new_root->lchild = jentry_new(new_root,
          old_root->lchild->key, old_root->lchild->value);
        if (!new_root->lchild) // jentry_new() fails
            return false;
        if (!json_build(new_root->lchild, old_root->lchild)) {
            jentry_free(new_root->lchild);
            return false;
        }     // json_build() fails
    } else
        new_root->lchild = NULL;
    if (old_root->rchild) {
        new_root->rchild = jentry_new(new_root,
          old_root->rchild->key, old_root->rchild->value);
        if (!new_root->rchild)  // jentry_new() fails
            return false;
        if (!json_build(new_root->rchild, old_root->rchild)) {
            if (new_root->lchild)
                jentry_free(new_root->lchild);
            jentry_free(new_root->rchild);
            return false;
        }   // json_build() fails
    } else
        new_root->rchild = NULL;
    return true;
}

// Comparison functions for json_sort()
static int jvalue_boolcmp(const void *value1, const void *value2) {
    return ((jvalue_t *) value1)->value.boolean -
      ((jvalue_t *) value2)->value.boolean;
}
static int jvalue_numcmp(const void *value1, const void *value2) {
    const jfloat_t CMP = ((jvalue_t *) value1)->value.number -
      ((jvalue_t *) value2)->value.number;

    return CMP < JFLT_EPSILON ? 0 : jfloat_ceil(CMP);
}
static int jvalue_strcmp(const void *value1, const void *value2) {
    return strcmp(((jvalue_t *) value1)->value.string,
      ((jvalue_t *) value2)->value.string);
}
static int jvalue_arrcmp(const void *value1, const void *value2) {
    const size_t SIZE1 = ((jvalue_t *) value1)->value.array->size;
    const size_t SIZE2 = ((jvalue_t *) value2)->value.array->size;

    if (SIZE1 > SIZE2)      return 1;
    else if (SIZE1 < SIZE2) return -1;
    else                    return 0;
}
static int jvalue_objcmp(const void *value1, const void *value2) {
    const size_t SIZE1 = json_size(((jvalue_t *) value1)->value.object);
    const size_t SIZE2 = json_size(((jvalue_t *) value2)->value.object);

    if (SIZE1 > SIZE2)      return 1;
    else if (SIZE1 < SIZE2) return -1;
    else                    return 0;
}

/* Returns comparison value (-1/0/1) of value
 * For values of different types, returns 'diff' */
static int jvalue_find_cmp(const jvalue_t *value1, const jvalue_t *value2, int diff) {
    if (value1->type != value2->type)
        return diff;
    return jvalue_getcmp(value1->type)(value1, value2);
}

/* Generates a new entry
 * Returns NULL and sets errno accordingly on error */
static jentry_t *jentry_new(jentry_t *parent,
  const char *key, const jvalue_t *restrict value) {
    jentry_t *new_entry = malloc(sizeof(jentry_t));

    if (!new_entry)        // malloc() fails
        return NULL;
    new_entry->key = strdup(key);
    if (!new_entry->key) {   // strdup() fails
        free(new_entry);
        return NULL;
    }
    new_entry->value = jvalue_copy(value);
    if (!new_entry->value) {   // jvalue_copy() fails
        free(new_entry->key);
        free(new_entry);
        return NULL;
    }
    new_entry->height = 0;
    new_entry->parent = parent;
    new_entry->lchild = new_entry->rchild = NULL;
    return new_entry;
}

/* Shifts smallest of subtree to top
 * Returns new root */
static jentry_t *json_smallest(jentry_t *root) {
    jentry_t *target = root;

    while (target->lchild)  // Get smallest
        target = target->lchild;
    while (target->parent != root->parent)  // Shift to top
        rr_rotate(target);
    return target;
}

/* Shifts largest of subtree to top
 * Returns new root */
static jentry_t *json_largest(jentry_t *root) {
    jentry_t *target = root;

    while (target->rchild)  // Get largest
        target = target->rchild;
    while (target->parent != root->parent)  // Shift to top
        ll_rotate(target);
    return target;
}

/* Returns entry in tree, starting at root, that matches given key
 * If key is not found, a new entry is added
 * Returns NULL and sets errno accordingly on error */
static jinfo_t *json_seek(jentry_t *root, const char *key, bool add) {
    const int DIF = strcmp(key, root->key);
    jinfo_t *info;

    if (DIF > 0) {
        if (!root->lchild) {
            if (add) {
                info = malloc(sizeof(jinfo_t));
                if (!info) // malloc() fails
                    return NULL;
                info->change = 1;
                info->target = root->rchild = jentry_new(root, key, NULL);
            } else
                info = calloc(1, sizeof(jinfo_t));
            return info;
        }
        info = json_lookup(root->rchild, key);
        root->height += info->change;
        return info;
    } else if (DIF < 0) {
        if (!root->lchild) {
            if (add) {
                info = malloc(sizeof(jinfo_t));
                if (!info) // malloc() fails
                    return NULL;
                info->change = 1;
                info->target = root->lchild = jentry_new(root, key, NULL);
            } else
                info = calloc(1, sizeof(jinfo_t));
            return info;
        }
        info = json_lookup(root->lchild, key);
        root->height += info->change;
        return info;
    }
    info = calloc(1, sizeof(jinfo_t));
    if (!info) // calloc() fails
        return NULL;
    return root;
}

//
static int (*jvalue_getcmp(char type))(const void *, const void *) {
    switch (type) {
    case J_BOOL:    return jvalue_boolcmp;
    case J_NUM:     return jvalue_numcmp;
    case J_STR:     return jvalue_strcmp;
    case J_ARR:     return jvalue_arrcmp;
    case J_OBJ:     return jvalue_objcmp;
    }
}

void jarray_free(jarray_t *array) {
    if (array) {
        for (size_t i = 0; i < array->size; ++i)
            jvalue_free(array->values + i * sizeof(jvalue_t));
        free(array);
    }
}
void json_free(json_t *json) {
    if (json) {  // Do not free NULL
        jentry_free(json->root);
        free(json);
    }
}
void jvalue_free(jvalue_t *value) {
    if (value) {
        switch (value->type) {
        case J_STR: free(value->value.string);          break;
        case J_ARR: jarray_free(value->value.array);    break;
        case J_OBJ: json_free(value->value.object);
        }
        free(value);
    }
}
bool jarray_pushb(jarray_t *array, const jvalue_t *restrict value) {
    if (!array || !value)
        error(EINVAL, false);
    if (array->size == SIZE_MAX /
      sizeof(jvalue_t *))   // Array takes up more than SIZE_MAX bytes
        error(E2BIG, false);
    if (array->size == array->capacity)
        realloc(array->values, array->capacity *= 2);
    array->values[array->size++] = jvalue_copy(value);
    return true;
}
bool jarray_pushf(jarray_t *array, const jvalue_t *restrict value) {
    if (!array || !value)
        error(EINVAL, false);
    if (array->size == SIZE_MAX / sizeof(jvalue_t *))
        error(E2BIG, false);
    if (array->size == array->capacity)
        realloc(array->values, array->capacity *= 2);
    ++array->size;
    for (size_t i = 1; i < array->size; ++i)    // Shift members forward
        array->values[i] = array->values[i - 1];
    array->values[0] = jvalue_copy(value);
    return true;
}
bool jarray_remove(jarray_t *restrict array, size_t index) {
    if (!array)
        error(EINVAL, false);
    if (!array->size || index > array->size - 1)   // Index is out-of-bounds
        error(ENOENT, false);
    free(array->values[index]);
    for (size_t i = array->size - 1;; --i) {                // Shift members backward
        array->values[i] = i + 1 == array->size ? NULL : array->values[i + 1];
        if (i == index)
            break;
    }
    --array->size;
    return true;
}
bool jarray_sort(jarray_t *restrict array) {
    if (!array)
        error(EINVAL, false);
    
    const jtype_t TYPE = array->values[0]->type;

    for (size_t i = 1; i < array->size; ++i) {  // Ensures types are identical
        if (array->values[i]->type != TYPE)
            error(EOPNOTSUPP, false);
    }
    qsort(array->values, array->size, sizeof(jvalue_t *), jvalue_getcmp(TYPE));
    return true;
}
bool json_add(json_t *json, const char *key, const jvalue_t *value) {
    if (!json || !key || !value)
        error(EINVAL, false);
    if (!json->root) {                          // Create root node
        json->root = jentry_new(NULL, strdup(key), value);
        if (!json->root)
            return false;
        return true;
    }

    jinfo_t *info = json_seek(json->root, key, true);   // link node to tree

    if (!info) // json_seek() fails
        return false;
    info->target->value = value;
    json_balance(json->root);
    free(info);
    return true;
}

//
static void indent_print(const FILE *restrict file,
  size_t indent, const char *restrict fmt, ...) {
    va_list args;

    while (indent) {
        fputs("    ", file);
        --indent;
    }
    va_start(args, file);
    vfprintf(file, fmt, args);
    va_end(args);
}

//
static void jarray_print(const jarray_t *restrict array,
  const FILE *restrict file, size_t indent) {
    indent_print(file, indent, "[\n");
    for (size_t i = 0; i < array->size; ++i) {
        jvalue_print(array->values[i], file, indent + 1);
        fputs(i != array->size - 1 ? ",\n" : "\n", file);
    }
    indent_print(file, indent, "]\n");
}

//
static void jfloat_print(jfloat_t number, const FILE *restrict file) {
    
}

//
static void jvalue_print(const jvalue_t *restrict value,
  const FILE *restrict file, size_t indent) {
    switch (value->type) {
    case J_BOOL:    indent_print(file, indent,
                      value->value.boolean ? "true" : "false");         break;
    case J_NUM:     jfloat_print(value->value.number, file);            break;
    case J_STR:     indent_print(file, indent, value->value.string);    break;
    case J_ARR:     jarray_print(value->value.array, file, indent);     break;
    case J_OBJ:     json_print(value->value.object, file, indent);      break;
    }
}

//
static bool jentry_print(const jentry_t *restrict root,
  const FILE *restrict file, size_t indent) {
    if (root->lchild) {
        if (!jentry_print(root->lchild, file, indent))
            return false;   // jentry_print() fails
    }
    if (root->rchild) {
        if (!jentry_print(root->rchild, file, indent))
            return false;
    }
    indent_print(file, indent, "\"%s\": ", root->key);
    if (root->value->type & (J_ARR|J_OBJ))
        fputs("\n", file);  // Use fputs(), as FILE param is restricted
    jvalue_print(root->value, file, indent);
    fputs(",\n", file);
    return true;
}

bool json_print(const json_t *restrict json,
  const FILE *restrict file, size_t indent) {
    if (!file)
        error(EINVAL, false);
    indent_print(file, indent, "{\n");
    if (json->root) {
        if (!jentry_print(json->root, file, indent + 1))
            return false;   // jentry_print() fails
    }
    indent_print(file, indent, "}\n");
    return true;
}

bool json_remove(json_t *json, const char *key) {
    if (!json || !key)
        error(EINVAL, false);
    if (!json->root)   // Entry doesn't exist
        error(ENOENT, false);

    jinfo_t *info = json_seek(json->root, key, false);

    if (!info)   // json_seek() fails
       return false;
    
    jentry_t *tmp, *target = info->target;

    if (!target)   // Entry not found
        error(ENOENT, false);
    
    const bool IS_LCHILD = target == target->parent->lchild;

    if (target->lchild) {
        if (IS_LCHILD) {
            tmp = json_smallest(target->rchild);
            target->parent->lchild = tmp;
        } else {
            tmp = json_largest(target->rchild);
            target->parent->rchild = tmp;
        }
        tmp->rchild = target->rchild;
        tmp->parent = target->parent;
    } else if (target->rchild) {
        if (IS_LCHILD) {
            tmp = json_largest(target->rchild);
            target->parent->lchild = tmp;
        } else {
            tmp = json_smallest(target->rchild);
            target->parent->rchild = tmp;
        }
        tmp->lchild = target->lchild;
        tmp->parent = target->parent;
    } else {
        if (IS_LCHILD)  target->parent->lchild = NULL;
        else            target->parent->rchild = NULL;
    }
    free(target->key);
    jvalue_free(target->value);
    free(target);
    json_balance(json->root);
    return true;
}
bool jvalue_modify(jvalue_t *restrict value, const jvalue_t *restrict NEW_VALUE) {
    if (!value || !NEW_VALUE)
        error(EINVAL, false);
    switch (value->type = NEW_VALUE->type) {
    case J_STR: value->value.string = strdup(NEW_VALUE->value.string);      break;
    case J_ARR: value->value.array  = jarray_copy(NEW_VALUE->value.array);   break;
    case J_OBJ: value->value.object = json_copy(NEW_VALUE->value.object);    break;
    default:    value->value = NEW_VALUE->value;
    }
    return true;
}
int jvalue_cmp(const jvalue_t *value1, const jvalue_t *value2) {
    if (!value1 || !value2 || value1->type != value2->type)
        error(EINVAL, 0);
    return jvalue_getcmp(value1->type)(value1, value2);
}
size_t json_size(const json_t *restrict json) {
    if (!json)
        error(EINVAL, 0);
    return json->root ? json->root->height : 0;
}
jarray_t *jarray_copy(const jarray_t *restrict array) {
    if (!array)
        error(EINVAL, NULL);

    jarray_t *restrict new_array = malloc(sizeof(json_t));

    if (!new_array)    // malloc() fails
        return NULL;
    new_array->values = malloc(array->capacity * sizeof(jvalue_t *));
    if (!new_array->values) {
        free(new_array);
        return NULL;
    }
    new_array->size = array->size;
    new_array->capacity = array->capacity;
    for (size_t i = 0; i < array->size; ++i)
        new_array->values[i] = jvalue_copy(array->values[i]);
    return true;
}
jarray_t *jarray_new(void) {
    jarray_t *new_array = malloc(sizeof(jarray_t));

    new_array->size = 0;
    new_array->capacity = JARRAY_DEFCAP;
    new_array->values = malloc(JARRAY_DEFCAP * sizeof(jvalue_t *));
    return new_array;
}
json_t *json_copy(const json_t *restrict json) {
    if (!json)
        error(EINVAL, NULL);

    json_t *new_json = malloc(sizeof(json_t));

    if (!new_json) // malloc() fails
        return NULL;
    if (json->root) {
        new_json->root = jentry_new(NULL, json->root->key, json->root->value);
        if (!new_json->root || !json_build(new_json->root, json->root)) {
            free(new_json);
            return NULL;
        }   // jentry_new() fails || json_build() fails
    } else
        new_json->root = NULL;
    return true;
}
json_t *json_new(void) { return calloc(1, sizeof(json_t)); }
json_t *json_parse(const FILE *file) { //TODO
    if (!file)
        error(EINVAL, NULL);
    
}
jvalue_t *jarray_findf(jarray_t *restrict array, const jvalue_t *value) {
    if (!array || !value)
        error(EINVAL, NULL);
    for (size_t i = 0; i < array->size; ++i) {
        if (!jvalue_find_cmp(array->values[i], value, 1))
            return array->values[i];
    }
    return NULL;
}
jvalue_t *jarray_findfn(jarray_t *restrict array, const jvalue_t *value) {
    if (!array || !value)
        error(EINVAL, NULL);
    for (size_t i = 0; i < array->size; ++i) {
        if (jvalue_find_cmp(array->values[i], value, 0))
            return array->values[i];
    }
    return NULL;
}
jvalue_t *jarray_findl(jarray_t *restrict array, const jvalue_t *value) {
    if (!array || !value)
        error(EINVAL, NULL);
    for (size_t i = array->size - 1;; ++i) {
        if (!jvalue_find_cmp(array->values[i], value, 1))
            return array->values[i];
        if (!i)
            break;
    }
    return NULL;
}
jvalue_t *jarray_findln(jarray_t *restrict array, const jvalue_t *value) {
    if (!array || !value)
        error(EINVAL, NULL);
    for (size_t i = array->size - 1;; ++i) {
        if (jvalue_find_cmp(array->values[i], value, 0))
            return array->values[i];
        if (!i)
            break;
    }
    return NULL;
}
jvalue_t *jarray_get(const jarray_t *restrict array, size_t index) {
    if (!array)
        error(EINVAL, NULL);
    if (!array->size || index > array->size - 1)   // Index is out-of-bounds
        error(ENOENT, NULL);
    return array->values[index];
}
// restrict return
jvalue_t *jarray_popf(jarray_t *restrict array) {
    if (!array)
        error(EINVAL, NULL);
    if (!array->size)
        error(ENOENT, NULL);

    jvalue_t *value = array->values[0];

    --array->size;
    for (size_t i = 0; i < array->size; ++i)    // Shift members forward
        array->values[i] = array->values[i + 1];
    return value;
}
// restrict return
jvalue_t *jarray_popb(jarray_t *restrict array) {
    if (!array)
        error(EINVAL, NULL);
    if (!array->size)
        error(ENOENT, NULL);

    jvalue_t *value = array->values[array->size - 1];

    --array->size;
    array->values[array->size] = NULL;
    return value;
}
jvalue_t *json_find(const json_t *restrict json, const char *key) {
    if (!json || !key)
        error(EINVAL, NULL);
    return json_seek(json->root, key, false)->target;
}
jvalue_t *jvalue_copy(const jvalue_t *restrict value) {
    if (!value)
        error(EINVAL, NULL);

    jvalue_t *new_value = malloc(sizeof(jvalue_t));

    if (!new_value)    // malloc() fails
        return NULL;
    new_value->type = value->type;
    switch (value->type) {
    case J_BOOL:    new_value->value.boolean = value->value.boolean;            break;
    case J_NUM:     new_value->value.number  = value->value.number;             break;
    case J_STR:     new_value->value.string  = strdup(value->value.string);     break;
    case J_ARR:     new_value->value.array   = jarray_copy(value->value.array); break;
    case J_OBJ:     new_value->value.object  = json_copy(value->value.object);
    }
    return new_value;
}

jvalue_t *jvalue_new(char type, const jany_t value) {
    if (!(type & (J_BOOL|J_NUM|J_STR|J_ARR|J_OBJ)) ||  // Invalid type ||
      type & (J_STR|J_ARR|J_OBJ) && !value.object)              // Passed NULL pointer
        error(EINVAL, NULL);
    
}