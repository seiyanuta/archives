#include "internal.h"
#include "api.h"
#include "parser.h"
#include "lexer.h"
#include "malloc.h"
#include "internal.h"

#define NEXT_CHAR() fetch_next_char(vm)
#define PUSHBACK_CHARS(num) pushback_chars(vm, num)
#define SKIP_CHARS(num) skip_chars(vm, num)

const char *ena_get_token_name(ena_token_type_t type) {
#define DEFINE_TOKEN_NAME(name) [ENA_TOKEN_##name] = #name

    static const char *names[ENA_TOKEN_MAX_NUM] = {
        DEFINE_TOKEN_NAME(UNKNOWN),
        DEFINE_TOKEN_NAME(EOF),
        DEFINE_TOKEN_NAME(ID),
        DEFINE_TOKEN_NAME(NULL),
        DEFINE_TOKEN_NAME(INT_LIT),
        DEFINE_TOKEN_NAME(STRING_LIT),
        DEFINE_TOKEN_NAME(PLUS),
        DEFINE_TOKEN_NAME(MINUS),
        DEFINE_TOKEN_NAME(SLASH),
        DEFINE_TOKEN_NAME(PERCENT),
        DEFINE_TOKEN_NAME(ASTERISK),
        DEFINE_TOKEN_NAME(DOUBLECOLON),
        DEFINE_TOKEN_NAME(SEMICOLON),
        DEFINE_TOKEN_NAME(LPAREN),
        DEFINE_TOKEN_NAME(RPAREN),
        DEFINE_TOKEN_NAME(LBRACKET),
        DEFINE_TOKEN_NAME(RBRACKET),
        DEFINE_TOKEN_NAME(LBRACE),
        DEFINE_TOKEN_NAME(RBRACE),
        DEFINE_TOKEN_NAME(DOT),
        DEFINE_TOKEN_NAME(COMMA),
        DEFINE_TOKEN_NAME(EQ),
        DEFINE_TOKEN_NAME(ADD_ASSIGN),
        DEFINE_TOKEN_NAME(SUB_ASSIGN),
        DEFINE_TOKEN_NAME(MUL_ASSIGN),
        DEFINE_TOKEN_NAME(DIV_ASSIGN),
        DEFINE_TOKEN_NAME(MOD_ASSIGN),
        DEFINE_TOKEN_NAME(DOUBLE_EQ),
        DEFINE_TOKEN_NAME(NEQ),
        DEFINE_TOKEN_NAME(LT),
        DEFINE_TOKEN_NAME(LTE),
        DEFINE_TOKEN_NAME(GT),
        DEFINE_TOKEN_NAME(GTE),
        DEFINE_TOKEN_NAME(VAR),
        DEFINE_TOKEN_NAME(IF),
        DEFINE_TOKEN_NAME(ELSE),
        DEFINE_TOKEN_NAME(FUNC),
        DEFINE_TOKEN_NAME(RETURN),
        DEFINE_TOKEN_NAME(TRUE),
        DEFINE_TOKEN_NAME(FALSE),
        DEFINE_TOKEN_NAME(CLASS),
        DEFINE_TOKEN_NAME(WHILE),
        DEFINE_TOKEN_NAME(BREAK),
        DEFINE_TOKEN_NAME(CONTINUE),
        DEFINE_TOKEN_NAME(NOT),
        DEFINE_TOKEN_NAME(AND),
        DEFINE_TOKEN_NAME(OR),
    };

    ENA_ASSERT(type < ENA_TOKEN_MAX_NUM);
    return names[type];
}

void ena_dump_tokens(struct ena_vm *vm, const char *script) {
    // Initialize the lexer.
    vm->lexer.next_pos = 0;
    DEBUG("dump tokens");
    vm->lexer.current_line = 1;
    vm->lexer.filepath = NULL;
    vm->lexer.script = script;

    for (;;) {
        if (ena_setjmp(vm->panic_jmpbuf) == 0) {
            struct ena_token *token = ena_get_next_token(vm);
            ena_token_type_t type = token->type;
            DEBUG("%s: '%s'", ena_get_token_name(token->type), token->str);
            ena_destroy_token(token);
            if (type == ENA_TOKEN_EOF) {
                break;
            }
        } else {
            fprintf(stderr, "%s", ena_get_error_cstr(vm));
        }
    }
}

char ena_get_next_char(struct ena_vm *vm) {
    char ch = vm->lexer.script[vm->lexer.next_pos];
    if (ch == '\0') {
        // Reached to the end of string.
        return 0;
    }

    vm->lexer.next_pos++;
    return ch;
}

static inline void pushback_chars(struct ena_vm *vm, size_t num) {
    vm->lexer.next_pos -= num;
}

static inline void skip_chars(struct ena_vm *vm, size_t num) {
    vm->lexer.next_pos += num;
}

static inline char fetch_next_char(struct ena_vm *vm) {
    char nextc = ena_get_next_char(vm);
    PUSHBACK_CHARS(1);
    return nextc;
}

/// Pushs back a token into the stream.
/// @note `token` remains alive after the operation.
/// @warning `token` must be most recently generated one.
void ena_pushback_token(struct ena_vm *vm, struct ena_token *token) {
    vm->lexer.next_pos -= ena_strlen(token->str);
    vm->lexer.current_line = token->line;
}

struct ena_token *ena_fetch_next_token(struct ena_vm *vm) {
    struct ena_token *token = ena_get_next_token(vm);
    ena_pushback_token(vm, token);
    return token;
}

ena_token_type_t ena_fetch_next_token_type(struct ena_vm *vm) {
    struct ena_token *token = ena_get_next_token(vm);
    ena_token_type_t type;
    if (token) {
        type = token->type;
        ena_pushback_token(vm, token);
    } else {
        type = ENA_TOKEN_EOF;
    }

    ena_destroy_token(token);
    return type;
}

void ena_skip_tokens(struct ena_vm *vm, int num) {
    for (int i = 0; i < num; i++) {
        ena_destroy_token(ena_get_next_token(vm));
    }
}

struct ena_token *ena_expect_token(struct ena_vm *vm, ena_token_type_t expected_type) {
    struct ena_token *token = ena_get_next_token(vm);
    if (token->type != expected_type) {
        SYNTAX_ERROR(
            "unexpected %s (expected %s)",
            ena_get_token_name(token->type),
            ena_get_token_name(expected_type)
        );
    }

    return token;
}

struct ena_token *ena_get_next_token(struct ena_vm *vm) {
    ena_token_type_t type = ENA_TOKEN_UNKNOWN;

retry:;
    size_t start = vm->lexer.next_pos;
    size_t str_len = 0;

    char nextc;
    if((nextc = ena_get_next_char(vm)) == 0) {
        type = ENA_TOKEN_EOF;
        goto return_token;
    }

    // Identifier or keyword
    if (ena_isalpha(nextc) || nextc == '_') {
        str_len = 0;
        for (;;) {
            str_len++;
            if((nextc = ena_get_next_char(vm)) == 0) {
                goto return_token;
            }

            if (ena_isalnum(nextc) || nextc == '_') {
                continue;
            }

            // Reached to the end of ID. Push back the character
            // because `nextc` points to the beginning of next token.
            PUSHBACK_CHARS(1);

#define SPECIAL_ID_TOKEN(name, type_name, num) \
    if (!ena_strncmp(&vm->lexer.script[start], name, num)) { \
        type = type_name; \
        break; \
    }

            type = ENA_TOKEN_ID;
            switch (str_len) {
                case 2:
                    SPECIAL_ID_TOKEN("if", ENA_TOKEN_IF, 2);
                    SPECIAL_ID_TOKEN("or", ENA_TOKEN_OR, 2);
                    break;
                case 3:
                    SPECIAL_ID_TOKEN("var", ENA_TOKEN_VAR, 3);
                    SPECIAL_ID_TOKEN("not", ENA_TOKEN_NOT, 3);
                    SPECIAL_ID_TOKEN("and", ENA_TOKEN_AND, 3);
                    break;
                case 4:
                    SPECIAL_ID_TOKEN("if", ENA_TOKEN_IF, 4);
                    SPECIAL_ID_TOKEN("null", ENA_TOKEN_NULL, 4);
                    SPECIAL_ID_TOKEN("true", ENA_TOKEN_TRUE, 4);
                    SPECIAL_ID_TOKEN("func", ENA_TOKEN_FUNC, 4);
                    break;
                case 5:
                    SPECIAL_ID_TOKEN("false", ENA_TOKEN_FALSE, 5);
                    SPECIAL_ID_TOKEN("while", ENA_TOKEN_WHILE, 5);
                    SPECIAL_ID_TOKEN("break", ENA_TOKEN_BREAK, 5);
                    SPECIAL_ID_TOKEN("class", ENA_TOKEN_CLASS, 5);
                    break;
                case 6:
                    SPECIAL_ID_TOKEN("return", ENA_TOKEN_RETURN, 6);
                    break;
                case 8:
                    SPECIAL_ID_TOKEN("continue", ENA_TOKEN_CONTINUE, 8);
                    break;
            }

            goto return_token;
        }
    }

    // Integer literal
    if (ena_isdigit(nextc)) {
        type = ENA_TOKEN_INT_LIT;
        str_len = 0;
        for (;;) {
            str_len++;
            if((nextc = ena_get_next_char(vm)) == 0) {
                goto return_token;
            }

            if (ena_isdigit(nextc)) {
                continue;
            }

            // Reached to the end of the literal. Push back the character
            // because `nextc` points to the beginning of next token.
            PUSHBACK_CHARS(1);
            goto return_token;
        }
    }

    // String literal
    if (nextc == '"') {
        type = ENA_TOKEN_STRING_LIT;
        str_len = 0;
        char prevc = 0;
        for (;;) {
            str_len++;
            if((nextc = ena_get_next_char(vm)) == 0) {
                goto return_token;
            }

            if (nextc == '"') {
                if (prevc != '\\') {
                    str_len++; // include the trailing '"'
                    goto return_token;
                }
            }

            prevc = nextc;
        }
    }

#define NEXTC_CASE(symbol, token_type) \
       case symbol: \
            type = ENA_TOKEN_##token_type; \
            break;

    str_len = 1;
    switch (nextc) {
        NEXTC_CASE(';', SEMICOLON)
        NEXTC_CASE(':', DOUBLECOLON)
        NEXTC_CASE('(', LPAREN)
        NEXTC_CASE(')', RPAREN)
        NEXTC_CASE('[', LBRACKET)
        NEXTC_CASE(']', RBRACKET)
        NEXTC_CASE('{', LBRACE)
        NEXTC_CASE('}', RBRACE)
        NEXTC_CASE('.', DOT)
        NEXTC_CASE(',', COMMA)
        case '!':
            if (NEXT_CHAR() == '=') {
                type = ENA_TOKEN_NEQ;
                str_len = 2;
                SKIP_CHARS(1);
            }
            break;
        case '+':
            if (NEXT_CHAR() == '=') {
                type = ENA_TOKEN_ADD_ASSIGN;
                str_len = 2;
                SKIP_CHARS(1);
            } else {
                type = ENA_TOKEN_PLUS;
            }
            break;
        case '-':
            if (NEXT_CHAR() == '=') {
                type = ENA_TOKEN_SUB_ASSIGN;
                str_len = 2;
                SKIP_CHARS(1);
            } else {
                type = ENA_TOKEN_MINUS;
            }
            break;
        case '*':
            if (NEXT_CHAR() == '=') {
                type = ENA_TOKEN_MUL_ASSIGN;
                str_len = 2;
                SKIP_CHARS(1);
            } else {
                type = ENA_TOKEN_ASTERISK;
            }
            break;
        case '%':
            if (NEXT_CHAR() == '=') {
                type = ENA_TOKEN_MOD_ASSIGN;
                str_len = 2;
                SKIP_CHARS(1);
            } else {
                type = ENA_TOKEN_PERCENT;
            }
            break;
        case '<':
            if (NEXT_CHAR() == '=') {
                type = ENA_TOKEN_LTE;
                str_len = 2;
                SKIP_CHARS(1);
            } else {
                type = ENA_TOKEN_LT;
            }
            break;
        case '>':
            if (NEXT_CHAR() == '=') {
                type = ENA_TOKEN_GTE;
                str_len = 2;
                SKIP_CHARS(1);
            } else {
                type = ENA_TOKEN_GT;
            }
            break;
        case '=':
            if (NEXT_CHAR() == '=') {
                type = ENA_TOKEN_DOUBLE_EQ;
                str_len = 2;
                SKIP_CHARS(1);
            } else {
                type = ENA_TOKEN_EQ;
            }
            break;

        //
        //  One-line comment or slash operator
        //
        case '/':
            switch (NEXT_CHAR()) {
                case '*': {
                    // Block comment. Nested comments are allowed.
                    int depth = 0;
                    char prevc = '/';
                    for (;;) {
                        if ((nextc = ena_get_next_char(vm)) == 0) {
                            type = ENA_TOKEN_EOF;
                            goto return_token;
                        }

                        if (nextc == '\n') {
                            vm->lexer.current_line++;
                        }

                        if (prevc == '/' && nextc == '*') {
                            depth++;
                        }

                        if (prevc == '*' && nextc == '/') {
                            depth--;
                            if (!depth) {
                                // End of the comment.
                                break;
                            }
                        }

                        prevc = nextc;
                    }
                    goto retry;
                    break;
                }
                case '/':
                    // Discard until a newline.
                    do {
                        if ((nextc = ena_get_next_char(vm)) == 0) {
                            type = ENA_TOKEN_EOF;
                            goto return_token;
                        }
                    } while (nextc != '\n');
                    vm->lexer.current_line++;
                    goto retry;
                case '=':
                    type = ENA_TOKEN_DIV_ASSIGN;
                    str_len = 2;
                    SKIP_CHARS(1);
                    break;
                default:
                    type = ENA_TOKEN_SLASH;
            }
            break;

        //
        //  Indentation
        //
        case '\n':
            vm->lexer.current_line++;
            goto retry;
        case ' ':
        case '\t':
            goto retry;
        default:
            SYNTAX_ERROR("unexpected '%c'", nextc);
    }

return_token:;
    struct ena_token *token = ena_malloc(sizeof(*token));
    token->type = type;
    token->str = (char *) ena_strndup(&vm->lexer.script[start], str_len);
    token->line = vm->lexer.current_line;
    return token;
}

struct ena_token *ena_copy_token(struct ena_token *token) {
    struct ena_token *new_token = ena_malloc(sizeof(*new_token));
    new_token->type = token->type;
    new_token->str = (char *) ena_strdup(token->str);
    new_token->line = token->line;
    return new_token;
}

void ena_destroy_token(struct ena_token *token) {
    if (!token) {
        DEBUG("%s: token is NULL", __func__);
        return;
    }

    ena_free(token->str);
    ena_free(token);
}
