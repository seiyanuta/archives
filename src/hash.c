/// @file
/// @brief A chained-with-linked-list fashion hash table implementation.
#include "internal.h"
#include "malloc.h"
#include "hash.h"
#include "api.h"
#include "internal.h"

#define INITIAL_NUM_BUCKETS 32
#define REHASH_THRESHOLD 8
#define REHASH_NEEDED(table) (((table)->num_entries / (table)->num_buckets) > REHASH_THRESHOLD)
#define ENTRY_EQUALS(table, entry, digest, key) \
        ((digest) == (table)->methods->hash(key) && (table)->methods->equals(key, (entry)->key))

/// Reduce the number of entries in a bucket.
/// @arg table The hash table.
static void rehash(UNUSED struct ena_hash_table *table) {
    // TODO
}

/// Initializes a hash table.
/// @arg table The hash table.
/// @arg methods The hash methods.
void ena_hash_init_table(struct ena_hash_table *table, struct ena_hash_methods *methods) {
    table->methods = methods;
    table->num_entries = 0;
    table->num_buckets = INITIAL_NUM_BUCKETS;
    table->buckets = ena_malloc(sizeof(struct ena_hash_entry *) * INITIAL_NUM_BUCKETS);
    for (int i = 0; i < INITIAL_NUM_BUCKETS; i++) {
        table->buckets[i] = NULL;
    }
}

/// Searches the table for a given key.
/// @arg table The hash table.
/// @arg key The key.
/// @returns The entry if it is found or NULL otherwise.
struct ena_hash_entry *ena_hash_search(struct ena_hash_table *table, void *key) {
    ena_hash_digest_t digest = table->methods->hash(key);
    struct ena_hash_entry *entry = table->buckets[digest % table->num_buckets];
    while (entry != NULL) {
        if (ENTRY_EQUALS(table, entry, digest, key)) {
            return entry;
        }

        entry = entry->next;
    }

    return NULL;
}

/// Inserts an entry (allows duplication).
/// @arg table The hash table.
/// @arg key The key.
/// @arg value The value.
/// @note This function inserts a new entry even if there is an entry with the same key.
void ena_hash_insert(struct ena_hash_table *table, void *key, void *value) {
    ena_hash_digest_t digest = table->methods->hash(key);
    struct ena_hash_entry **entry = &table->buckets[digest % table->num_buckets];
    for (;;) {
        if (*entry == NULL) {
            // Prepend a new entry to the end of bucket.
            struct ena_hash_entry *new_entry = ena_malloc(sizeof(*new_entry));
            new_entry->key   = key;
            new_entry->value = value;
            new_entry->hash  = table->methods->hash(key);
            new_entry->next  = NULL;
            *entry = new_entry;
            table->num_entries++;

            if (REHASH_NEEDED(table)) {
                // Too many entries. Add some buckets.
                rehash(table);
            }

            break;
        }

        entry = &(*entry)->next;
    }
}

/// Searches for an entry or inserts an new entry if it does not exists.
/// @arg table The hash table.
/// @arg key The key.
/// @arg value The value.
/// @note This function doesn't insert if the table has key present.
/// @returns NULL if successful or an existing entry (with same key) if it already exists.
struct ena_hash_entry *ena_hash_search_or_insert(struct ena_hash_table *table, void *key, void *value) {
    struct ena_hash_entry *entry = ena_hash_search(table, key);
    if (entry) {
        return entry;
    }

    ena_hash_insert(table, key, value);
    return NULL;
}

/// Removes an entry from the table.
/// @arg table The hash table.
/// @arg key The key of entry to be removed.
/// @note This function removes the *first* entry. To remove all entries with the same key use
///       ena_hash_remove_all().
/// @returns true if it found and removed an entry with the key or false otherwise.
bool ena_hash_remove(struct ena_hash_table *table, void *key) {
    ena_hash_digest_t digest = table->methods->hash(key);
    struct ena_hash_entry *prev = NULL;
    struct ena_hash_entry *current = table->buckets[digest % table->num_buckets];
    while (current != NULL) {
        if (ENTRY_EQUALS(table, current, digest, key)) {
            if (prev) {
                prev->next = current->next;
            }

            ena_free(current);
            table->num_entries--;
            return true;
        }

        prev = current;
        current = current->next;
    }

    return false;
}

/// Removes all entries with given key from the table.
/// @arg table The hash table.
/// @arg key The key of entry to be removed.
void ena_hash_remove_all(struct ena_hash_table *table, void *key) {
    while (ena_hash_remove(table, key));
}

static bool ident_equals(void *key1, void *key2) {
    return (int) key1 == (int) key2;
}

static ena_hash_digest_t ident_hash(void *key) {
    return (ena_hash_digest_t) key;
}

static struct ena_hash_methods ident_hash_methods = {
    .equals = ident_equals,
    .hash = ident_hash,
};

/// Initializes a int hash table.
/// @arg table The hash table.
/// @arg methods The hash methods.
void ena_hash_init_ident_table(struct ena_hash_table *table) {
    ena_hash_init_table(table, &ident_hash_methods);
}

static bool cstr_equals(void *key1, void *key2) {
    return !ena_strcmp(key1, key2);
}

static ena_hash_digest_t cstr_hash(void *key) {
    unsigned char *str = (unsigned char *) key;

    ena_hash_digest_t digest = 0;
    while (*str != '\0') {
        digest += *str;
        str++;
    }

    return digest;
}

static struct ena_hash_methods cstr_hash_methods = {
    .equals = cstr_equals,
    .hash = cstr_hash,
};

/// Initializes a C-string (ASCII characters terminated by NUL) hash table.
/// @arg table The hash table.
/// @arg methods The hash methods.
void ena_hash_init_cstr_table(struct ena_hash_table *table) {
    ena_hash_init_table(table, &cstr_hash_methods);
}

/// Print all entries in a hash<ena_ident_t, ena_value_t>.
void ena_hash_dump_ident_value_table(struct ena_vm *vm, struct ena_hash_table *table) {
    int num_entries = 0;
    for (int i = 0; i < table->num_buckets; i++) {
        struct ena_hash_entry *e = table->buckets[i];
        while (e) {
            char buf[32];
            ena_ident_t key = (ena_ident_t) e->key;
            ena_value_t value = (ena_value_t) e->value;
            ena_stringify((char *) &buf, sizeof(buf), value);
            DEBUG("%s -> %s", ena_ident2cstr(vm, key), buf);
            num_entries++;
            e = e->next;
        }
    }

    DEBUG("%d entries in total", num_entries);
}
