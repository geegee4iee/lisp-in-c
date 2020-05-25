#include <stdio.h>
#include "mpc.h"

// Global variables
// Store user's input
static char input[2048];

// Declaration
// Forward Declarations
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;
typedef lval*(*lbuiltin)(lenv*, lval*);

// Our defined structs
typedef struct lval
{
    int type;
    long num;
    char* err;
    char* sym;

    /* Function */
    lbuiltin builtin_func;
    lenv* env;
    lval* formals;
    lval* body;

    /* Expression */
    int count;
    struct lval** cell;
} lval;

struct lenv {
    int count;
    char **syms;
    lval **vals;
};

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
        lval* err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args);  \
        return err; \
    } 

lval* lval_add(lval* v, lval* x);
lval* lval_copy(lval* v);
lval* lval_eval(lenv* e, lval* v);
void lval_print(lval* v);
lval* lval_eval_sexpr(lenv* e, lval* v);
lval* builtin(lval* a, char* func);

// Create Enumeration of Possible lval Types
enum
{
    LVAL_NUM,
    LVAL_ERR,
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_QEXPR,
    LVAL_FUNC
};

// Create Enumeration of Possible Error Types
enum
{
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM
};

char* ltype_name(int t) {
    switch(t) {
        case LVAL_FUNC: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

// Construct a pointer to a new Number lval
lval* lval_num(long x)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

// Construct a pointer to a new Error lval
lval* lval_err(char* fmt, ...)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    va_list va;
    va_start(va, fmt);

    v->err = malloc(512);

    // printf the error string with a maximum 511 characters to err
    vsnprintf(v->err, 511, fmt, va);

    // Reallocate to number of bytes actually used
    v->err = realloc(v->err, strlen(v->err) + 1);
    va_end(va);
    return v;
}

// Construct a pointer to a new Symbol lval
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval* lval_func(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUNC;
    v->builtin_func = func;
    return v;
}

lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUNC;

    v->builtin_func = NULL;

    v->env = lenv_new();

    v->formals = formals;
    v->body = body;
    return v;
}

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

// Free memory from lval
void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_NUM: 
            break;
        case LVAL_FUNC:
            if (!v->builtin_func) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
            break;
        case LVAL_ERR:  
            free(v->err); 
            break;
        case LVAL_SYM:  
            free(v->sym); 
            break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i = 0; i < v-> count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
        break;
    }

    free(v);
}

// Free memory from lenv
void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }

    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }

    return lval_err("unbound symbol '%s'!", k->sym);
}

void lenv_put(lenv* e, lval* k, lval* v) {
    for (int i = 0; i < e->count; i++) {
        // If variable is found the delete item at that position
        // Replace with a copy of user-supplied one.
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    e->vals[e->count - 1] = lval_copy(v);
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}

// Construct a pointer to a new SExpression lval
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

// Construct a pointer to a new Q-Expression lval
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) return lval_read_num(t); 
    if (strstr(t->tag, "symbol")) return lval_sym(t->contents);

    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) x = lval_sexpr();
    if (strstr(t->tag, "sexpr")) x = lval_sexpr();
    if (strstr(t->tag, "qexpr")) x = lval_qexpr();

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) continue;
        if (strcmp(t->children[i]->contents, ")") == 0) continue;
        if (strcmp(t->children[i]->contents, "{") == 0) continue;
        if (strcmp(t->children[i]->contents, "}") == 0) continue; 
        if (strcmp(t->children[i]->tag, "regex") == 0) continue;
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);

        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }

    putchar(close);
}

void lval_print(lval* v)
{
    switch (v->type)
    {
        case LVAL_NUM:
            printf("%li", v->num);
            break;
        case LVAL_ERR:
            printf("Error: %s", v->err); 
            break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
        case LVAL_FUNC:
            printf("<function>");
            break;
        default:
            break;
    }
}

void lval_println(lval* v)
{
    lval_print(v);
    putchar('\n');
}

lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }

    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(e, v);
    }

    return v;
}

lval* lval_pop(lval* v, int i) {
    lval* x = v->cell[i];

    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count - i - 1));

    v->count--;

    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* builtin_op(lenv* e, lval* a, char* op) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(a->cell[i], a->cell[i]->type == LVAL_NUM,
            "Function '%s' passed incorrect type for argument %i. "
            "Got %s, Expected %s",
            op, i, ltype_name(a->cell[i]->type), ltype_name(LVAL_NUM)
        );
    }

    // Pop the first element
    lval* x = lval_pop(a, 0);

    // If no arguments and sub then perform unary operation
    if ((strcmp(op, "-") ==0) && a->count == 0) {
        x-> num = -x->num;
    }

    while (a->count > 0) {
        lval* y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) x->num += y->num;
        if (strcmp(op, "-") == 0) x->num -= y->num;
        if (strcmp(op, "*") == 0) x->num *= y->num;
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division by Zero!");
                break;
            }
            x->num /= y->num;
        }

        lval_del(y);
    }

    lval_del(a);
    return x;
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]); 
    }

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    if (v->count == 0) return v;
    
    if (v->count == 1) return lval_take(v, 0);

    // Ensure first element is a function after evaluation
    lval* func = lval_pop(v, 0);
    if (func->type != LVAL_FUNC) {
        lval_del(func);
        lval_del(v);
        return lval_err("first element is not a function!");
    }

    lval* result = func->builtin_func(e, v);
    lval_del(func);
    return result;
}

lval* builtin_head(lenv* e, lval* a) {
    LASSERT(a, a->count == 1, 
        "Func 'head' passed too many arguments! "
        "Got %i, Expected %i",
        a->count, 1);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, 
        "Func 'head' passed incorrect types for argument 0. "
        "Got %s, Expected %s",
        ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
    LASSERT(a, a->cell[0]->count != 0, "Func 'head' passed {}!");

    // Take the first argument
    lval* v = lval_take(a, 0);

    // Delete all elemenents that are not head and return
    while (v->count > 1) {
        lval_del(lval_pop(v, 1));
    }
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    LASSERT(a, a->count == 1, 
        "Func 'tail' passed too many arguments. ",
        "Got %i, Expected %i.",
        a->count, 1);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, 
        "Func 'tail' passed incorrect type. "
        "Got %s, Expected %s",
        ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
    LASSERT(a, a->cell[0]->count != 0, "Func 'tail' passed {}");

    // Take first argument and only argument from a as a contains only one qexpr
    lval* v = lval_take(a, 0);

    // Delete first element and return
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
    LASSERT(a, a->count == 1, 
        "Func 'eval' passed too many arguments. "
        "Got %i, Expected %i.",
        a->count, 1);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, 
        "Func 'eval' passed incorrect type. "
        "Got %s, Expected %s",
        ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

    lval* v = lval_take(a, 0);
    v->type = LVAL_SEXPR;
    return lval_eval(e, v);
}

lval* lval_join(lval* x, lval* y) {
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    lval_del(y);
    return x;
}

// join {1 2 3} {4 5 6} {7 8}
// {1 2 3 4 5 6 7 8}
lval* builtin_join(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR, 
        "Function 'join' passed incorrect type. "
        "Got %s, Expected %s",
        ltype_name(a->cell[i]->type), ltype_name(LVAL_QEXPR));
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_subtract(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_multiply(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_divide(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

lval* builtin_def(lenv* e, lval* a) {
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, 
        "Function 'def' passed incorrect type. "
        "Got %i, Expected %i", 
        a->count, 1);

    lval* syms = a->cell[0];

    for (int i = 0; i < syms->count; i++) {
        LASSERT(syms, syms->cell[i]->type == LVAL_SYM,
        "Function 'def' cannot define non-symbol");
    }

    LASSERT(a, syms->count == a->count - 1, 
        "Function 'def' defined incorrect"
        " number of values to symbols. "
        "Got %i, Expected %i.",
        syms->count, a->count - 1
    );

    for (int i = 0; i < syms->count; i++) {
        lenv_put(e, syms->cell[i], a->cell[i + 1]);
    }

    lval_del(a);
    return lval_sexpr();
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_func(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv* e) {
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "def", builtin_def);

    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_subtract);
    lenv_add_builtin(e, "*", builtin_multiply);
    lenv_add_builtin(e, "/", builtin_divide);
}

lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch(v->type) {
        case LVAL_FUNC: x->builtin_func = v->builtin_func; break;
        case LVAL_NUM: x->num = v->num; break;

        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;
        
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * v->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }

    return x;
}

int main(int argc, char **argv)
{
    puts("Keii Version 0.0.1");
    puts("Press Ctrl + C to exit\n");

    /* Create Parser */
    mpc_parser_t *number = mpc_new("number");
    mpc_parser_t *symbol= mpc_new("symbol");
    mpc_parser_t *sexpr = mpc_new("sexpr");
    mpc_parser_t *qexpr = mpc_new("qexpr");
    mpc_parser_t *expr = mpc_new("expr");
    mpc_parser_t *lispy = mpc_new("lispy");

    /* Defind the language */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                           \
            number: /-?[0-9]+/;                                     \
            symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/;               \
            sexpr: '(' <expr>* ')';                                 \
            qexpr: '{' <expr>* '}';                                 \
            expr: <number> | <symbol> | <sexpr> | <qexpr> ;         \
            lispy: /^/ <expr>* /$/;                                 \
        ",
              number, symbol, sexpr, qexpr, expr, lispy);

    lenv* e = lenv_new();
    lenv_add_builtins(e);

    while (1)
    {
        fputs("keii> ", stdout);

        fgets(input, 2048, stdin);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, lispy, &r))
        {
            lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);

            mpc_ast_delete(r.output);
        }
        else
        {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
    }

    mpc_cleanup(6, number, symbol, sexpr, qexpr, expr, lispy);

    return 0;
}
