#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

// Windows compiler preprocessor
#ifdef _WIN32
#include <string.h>

static char input[2048];

// Fake readline function
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}

void add_history(char* unused) {}
#else
#include <editline/readline.h>
#endif

// Declaration
// Forward Declarations
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;
typedef lval*(*lbuiltin)(lenv*, lval*);

// Create Enumeration of Possible lval Types
enum LVAL_TYPE
{
    LVAL_NUM,
    LVAL_ERR,
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_QEXPR,
    LVAL_FUNC
};

// Create Enumeration of Possible Error Types
enum LERR_TYPE
{
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM
};

// Our defined structs
typedef struct lval
{
    enum LVAL_TYPE type;
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
    // Parent environment
    lenv* parent;
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

#define LASSERT_TYPE(func, args, index, expect) \
    LASSERT(args, args->cell[index]->type == expect, \
        "Function '%s' passed incorrect type for argument %i. " \
        "Got %s, Expected %s.", \
        func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num) \
    LASSERT(args, args->count == num, \
        "Function '%s' passed incorrect number of arguments. " \
        "Got %i, Expected %i.", \
        func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
    LASSERT(args, args->cell[index]->count != 0, \
        "Function '%s' passed {} for argument %i", \
        func, index)

lval* builtin_eval(lenv* e, lval* a);
lval* builtin_var(lenv* env, lval* a, char* func);
lval* lval_add(lval* v, lval* x);
lval* lval_copy(lval* v);
lval* lval_eval(lenv* e, lval* v);
void lval_print(lval* v);
lval* lval_eval_sexpr(lenv* e, lval* v);
void lenv_del(lenv* e);
lenv* lenv_new(void);
lval* builtin(lval* a, char* func);

char* ltype_name(enum LVAL_TYPE t) {
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
    e->parent = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

lenv* lenv_copy(lenv* v) {
    lenv* e = malloc(sizeof(lenv));

    e->parent = v->parent;
    e->count = v->count;
    e->syms = malloc(sizeof(char*) * e->count);
    for (int i = 0; i < e->count; i++) {
        e->syms[i] = malloc(strlen(v->syms[i]) + 1);
        strcpy(e->syms[i], v->syms[i]);
    }

    e->vals = malloc(sizeof(lval*) * e->count);
    for (int i = 0; i < e->count; i++) {
        e->vals[i] = lval_copy(v->vals[i]);
    }

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

    // If no symbol found, check parent environment, otherwise return lval error
    if (e->parent) {
        return lenv_get(e->parent, k);
    } else {
        return lval_err("unbound symbol '%s'!", k->sym);
    }

}

// Define local variable
void lenv_put(lenv* e, lval* k, lval* v) {
    for (int i = 0; i < e->count; i++) {
        // If variable is found, delete item at that position
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

// Define global variable
void lenv_def(lenv* e, lval* symbol, lval* value) {
    // Iterate to the environment which has no parent
    while (e->parent) {
        e = e->parent;
    }

    lenv_put(e, symbol, value);
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

// Convert AST to an expression tree
lval* lval_read(mpc_ast_t* abstract_syntax_tree) {
    if (strstr(abstract_syntax_tree->tag, "number")) return lval_read_num(abstract_syntax_tree); 
    if (strstr(abstract_syntax_tree->tag, "symbol")) return lval_sym(abstract_syntax_tree->contents);

    lval* x = NULL;
    if (strcmp(abstract_syntax_tree->tag, ">") == 0) x = lval_sexpr();
    if (strstr(abstract_syntax_tree->tag, "sexpr")) x = lval_sexpr();
    if (strstr(abstract_syntax_tree->tag, "qexpr")) x = lval_qexpr();

    for (int i = 0; i < abstract_syntax_tree->children_num; i++) {
        if (strcmp(abstract_syntax_tree->children[i]->contents, "(") == 0) continue;
        if (strcmp(abstract_syntax_tree->children[i]->contents, ")") == 0) continue;
        if (strcmp(abstract_syntax_tree->children[i]->contents, "{") == 0) continue;
        if (strcmp(abstract_syntax_tree->children[i]->contents, "}") == 0) continue; 
        if (strcmp(abstract_syntax_tree->children[i]->tag, "regex") == 0) continue;
        x = lval_add(x, lval_read(abstract_syntax_tree->children[i]));
    }

    return x;
}

// Insert Lisp value x to Lisp value v
lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

lval* lval_call(lenv* env, lval* f, lval* a) {
    if (f->builtin_func) {
        return f->builtin_func(env, a);
    }

    // Record Argument Counts
    int given = a->count;
    int total = f->formals->count;

    // While arguments still remain to be processed
    while (a->count) {
        // if re're ran out of formal arguments to bind
        if (f->formals->count == 0) {
            lval_del(a);
            return lval_err(
                "Function passed too many parguments. "
                "Got %i, Expected %i.", given, total);
        }

        // Pop the first symbol from the formals
        lval* sym = lval_pop(f->formals, 0);

        // Pop the next argument from the list
        lval* val = lval_pop(a, 0);

        // Bind a copy into the function's environment
        lenv_put(f->env, sym, val);

        lval_del(sym);
        lval_del(val);
    }

    // Argument list is now bound so can be cleaned up
    lval_del(a);

    // If all formals have been bound evaluate
    if (f->formals->count == 0) {
        // Set environment parent to evaluation environment
        f->env->parent = env;

        // Evaluate and return
        return builtin_eval(
            f->env, lval_add(lval_sexpr(), lval_copy(f->body))
        );
    }
    else {
        // Otherwise return partially evaluated function
        return lval_copy(f);
    }
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
            if (v->builtin_func) {
                printf("<builtin>");
            } else {
                printf("(\\ "); lval_print(v->formals);
                putchar(' '); lval_print(v->body); putchar(')');
            }
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

// Evaluate Lisp values tree
lval* lval_eval(lenv* env, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(env, v);
        lval_del(v);
        return x;
    }

    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(env, v);
    }

    return v;
}

// Get child lval at specified index from passed in lval
lval* lval_pop(lval* v, int index) {
    lval* x = v->cell[index];
    memmove(&v->cell[index], &v->cell[index+1], sizeof(lval*) * (v->count - index - 1));

    v->count--;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);

    return x;
}

// Get child lval at specified index and free delete parent lval
lval* lval_take(lval* v, int index) {
    lval* x = lval_pop(v, index);
    lval_del(v);
    return x;
}

lval* builtin_operation(lenv* environment, lval* a, char* operation) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE(operation, a, i, LVAL_NUM);
    }

    // Pop the first element
    lval* x = lval_pop(a, 0);

    // If no arguments and sub then perform unary operation
    if ((strcmp(operation, "-") ==0) && a->count == 0) {
        x-> num = -x->num;
    }

    while (a->count > 0) {
        lval* y = lval_pop(a, 0);

        if (strcmp(operation, "+") == 0) x->num += y->num;
        if (strcmp(operation, "-") == 0) x->num -= y->num;
        if (strcmp(operation, "*") == 0) x->num *= y->num;
        if (strcmp(operation, "/") == 0) {
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

lval* lval_eval_sexpr(lenv* environment, lval* v) {
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(environment, v->cell[i]); 
    }

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    if (v->count == 0) return v;
    
    if (v->count == 1) return lval_take(v, 0);

    // Ensure first element is a function after evaluation
    lval* lval_func = lval_pop(v, 0);
    if (lval_func->type != LVAL_FUNC) {
        lval* err = lval_err(
            "S-Expression starts with incorrect type. "
            "Got %s, Expected %s.",
            ltype_name(lval_func->type), ltype_name(LVAL_FUNC)
        );
        lval_del(lval_func);
        lval_del(v);
        return err;
    }

    lval* result = lval_call(environment, lval_func, v);
}

lval* builtin_head(lenv* e, lval* a) {
    LASSERT_NUM("head", a, 1);
    LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head", a, 0);

    // Take the first argument
    lval* v = lval_take(a, 0);

    // Delete all elemenents that are not head and return
    while (v->count > 1) {
        lval_del(lval_pop(v, 1));
    }
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_NUM("tail", a, 1);
    LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("tail", a, 0);

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
    LASSERT_NUM("eval", a, 1);
    LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

    lval* v = lval_take(a, 0);
    v->type = LVAL_SEXPR;
    return lval_eval(e, v);
}

lval* builtin_lambda(lenv* e, lval* a) {
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol. Got %s, Expected %s",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }

    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
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
        LASSERT_TYPE("join", a, i, LVAL_QEXPR);
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin_add(lenv* e, lval* a) {
    return builtin_operation(e, a, "+");
}

lval* builtin_subtract(lenv* e, lval* a) {
    return builtin_operation(e, a, "-");
}

lval* builtin_multiply(lenv* e, lval* a) {
    return builtin_operation(e, a, "*");
}

lval* builtin_divide(lenv* e, lval* a) {
    return builtin_operation(e, a, "/");
}

lval* builtin_def(lenv* e, lval* a) {
    return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
    return builtin_var(e, a, "=");
}

lval* builtin_var(lenv* env, lval* a, char* func) {
    LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

    lval* syms = a->cell[0];
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
            "Function '%s' cannot define non-symbol. "
            "Got %s, Expected %s.", func,
            ltype_name(syms->cell[i]->type),
            ltype_name(LVAL_SYM));
    }

    LASSERT(a, (syms->count == a->count-1), 
        "Function '%s' passed too many arguments for symbols. "
        "Got %i, Expected %i.", func, syms->count, a->count-1);

    for (int i = 0; i < syms->count; i++) {
        // If 'def', define the variable globally
        if (strcmp(func, "def") == 0) {
            lenv_def(env, syms->cell[i], a->cell[i+1]);
        }

        // If '=' define the variable locally according to passed in env
        if (strcmp(func, "=") == 0) {
            lenv_put(env, syms->cell[i], a->cell[i+1]);
        }
    }

    lval_del(a);
    return lval_sexpr();
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* symbol = lval_sym(name);
    lval* func_def = lval_func(func);
    lenv_put(e, symbol, func_def);
    lval_del(symbol);
    lval_del(func_def);
}

void lenv_add_builtins(lenv* environment) {
    lenv_add_builtin(environment, "list", builtin_list);
    lenv_add_builtin(environment, "head", builtin_head);
    lenv_add_builtin(environment, "tail", builtin_tail);
    lenv_add_builtin(environment, "join", builtin_join);
    lenv_add_builtin(environment, "eval", builtin_eval);
    lenv_add_builtin(environment, "def", builtin_def);
    lenv_add_builtin(environment, "=", builtin_put);
    lenv_add_builtin(environment, "\\", builtin_lambda);

    lenv_add_builtin(environment, "+", builtin_add);
    lenv_add_builtin(environment, "-", builtin_subtract);
    lenv_add_builtin(environment, "*", builtin_multiply);
    lenv_add_builtin(environment, "/", builtin_divide);
}

lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch(v->type) {
        case LVAL_FUNC: 
            if (v->builtin_func) {
                x->builtin_func = v->builtin_func; 
            } else {
                x->builtin_func = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
            break;
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

    // Initialise root environment with builtin functions
    lenv* e = lenv_new();
    lenv_add_builtins(e);

    while (1)
    {
        char* input = readline("keii> ");
        add_history(input);

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

        free(input);
    }

    lenv_del(e);
    mpc_cleanup(6, number, symbol, sexpr, qexpr, expr, lispy);

    return 0;
}
