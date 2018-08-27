#ifndef __API_H__
#define __API_H__

// The standard libraries we can use are limited to freestanding ones.
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

// We use only lower 32-bit and MSB.
typedef uintptr_t ena_value_t;

enum ena_value_type {
    ENA_T_UNDEFINED, // used internally
    ENA_T_NULL,
    ENA_T_INT,
    ENA_T_STRING,
    ENA_T_LIST,
    ENA_T_MAP,
    ENA_T_BOOL,
    ENA_T_FUNC,
    ENA_T_CLASS,
    ENA_T_INSTANCE,
    ENA_T_ANY,
};

#define ENA_VALUE2OBJ(type, value) ((struct ena_##type *) (value))

// Assuming that there are no insane memory allocators in this world which returns
// too low memory address (< 0x1000). Typically the page is never used in order to
// detect NULL pointer dereferences and in an embedded software (MMU is disabled)
// important data strctures like interrupt table exist.
#define ENA_IS_IN_HEAP(value) (((uintptr_t) (value)) >= 0x1000)
#define ENA_UNDEFINED 0
#define ENA_NULL      (1 << 1)
#define ENA_TRUE      (2 << 1)
#define ENA_FALSE     (3 << 1)

enum ena_error_type {
    ENA_ERROR_NONE,
    ENA_ERROR_NOT_YET_IMPLEMENTED,
    ENA_ERROR_INVALID_SYNTAX,
    ENA_ERROR_BUG,
    ENA_ERROR_TYPE,
    ENA_ERROR_RUNTIME,
};

struct ena_vm;
typedef ena_value_t (*ena_native_method_t)(struct ena_vm *vm, ena_value_t self, ena_value_t *args, int num_args);
typedef ena_value_t (*ena_native_func_t)(struct ena_vm *vm, ena_value_t *args, int num_args);

/// An ena interpreter instance.
struct ena_vm *ena_create_vm();
void ena_destroy_vm(struct ena_vm *vm);
bool ena_eval(struct ena_vm *vm, char *script);
const char *ena_get_error_cstr(struct ena_vm *vm);

struct ena_class;
void ena_define_native_method(struct ena_vm *vm, struct ena_class *cls, const char *name, ena_native_method_t method);

enum ena_value_type ena_get_type(ena_value_t value);
void ena_stringify(char *buf, size_t buf_len, ena_value_t value);

/// To value.
ena_value_t ena_create_int(int value);
ena_value_t ena_create_string(struct ena_vm *vm, const char *str, size_t size);
ena_value_t ena_create_bool(int condition);
ena_value_t ena_create_func(ena_native_func_t *func);
struct ena_module *ena_create_module(void);

ena_value_t ena_get_var(struct ena_vm *vm, struct ena_module *module, const char *name);

#endif
