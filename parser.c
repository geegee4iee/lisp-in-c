#include <stdio.h>
#include "mpc.h"

#define LASSERT(args, cond, err) \
    if (!(cond)) { lval_del(args); return lval_err(err); }

static char input[2048];
// Declaration

typedef struct lval
{
    int type;
    long num;
    char* err;
    char* sym;
    int count;
    struct lval** cell;
} lval;

lval* lval_add(lval* v, lval* x);
void lval_print(lval* v);
lval* lval_eval(lval* v);
lval* lval_eval_sexpr(lval* v);
lval* builtin(lval* a, char* func);

// Create Enumeration of Possible lval Types
enum
{
    LVAL_NUM,
    LVAL_ERR,
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_QEXPR
};

// Create Enumeration of Possible Error Types
enum
{
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM
};

// Construct a pointer to a new Number lval
lval* lval_num(long x)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

// Construct a pointer to a new Error lval
lval* lval_err(char* m)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
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

// Free memory from lval
void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_NUM: 
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
        default:
            break;
    }
}

void lval_println(lval* v)
{
    lval_print(v);
    putchar('\n');
}

lval* lval_eval(lval* v) {
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(v);
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

lval* builtin_op(lval* a, char* op) {
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number you moron!");
        }
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

lval* lval_eval_sexpr(lval* v) {
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]); 
    }

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    if (v->count == 0) return v;
    
    if (v->count == 1) return lval_take(v, 0);

    lval* symbol = lval_pop(v, 0);
    if (symbol->type != LVAL_SYM) {
        lval_del(symbol);
        lval_del(v);
        return lval_err("S-expression does not start with symbol!");
    }

    lval* result = builtin(v, symbol->sym);
    lval_del(symbol);
    return result;
}

lval* builtin_head(lval* a) {
    LASSERT(a, a->count == 1, "Func 'head' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Func 'head' passed incorrect types!");
    LASSERT(a, a->cell[0]->count != 0, "Func 'head' passed {}!");

    // Take the first argument
    lval* v = lval_take(a, 0);

    // Delete all elemenents that are not head and return
    while (v->count > 1) {
        lval_del(lval_pop(v, 1));
    }
    return v;
}

lval* builtin_tail(lval* a) {
    LASSERT(a, a->count == 1, "Func 'tail' passed too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Func 'tail' passed incorrect type");
    LASSERT(a, a->cell[0]->count != 0, "Func 'tail' passed {}");

    // Take first argument and only argument from a as a contains only one qexpr
    lval* v = lval_take(a, 0);

    // Delete first element and return
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lval* a) {
    LASSERT(a, a->count == 1, "Func 'eval' passed too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Func 'eval' passed incorrect type");

    lval* v = lval_take(a, 0);
    v->type = LVAL_SEXPR;
    return lval_eval(v);
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
lval* builtin_join(lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "Function 'join' passed incorrect type");
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin(lval* a, char* func) {
    if (strcmp("list", func) == 0) return builtin_list(a);
    if (strcmp("join", func) == 0) return builtin_join(a);
    if (strcmp("head", func) == 0) return builtin_head(a);
    if (strcmp("tail", func) == 0) return builtin_tail(a);
    if (strcmp("eval", func) == 0) return builtin_eval(a);
    if (strstr("+-/*", func)) return builtin_op(a, func);
    lval_del(a);
    return lval_err("Unknown function!");
}

int main(int argc, char **argv)
{
    puts("Kei Lispy Version 0.0.1");
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
            symbol: \"list\" | \"head\" | \"tail\"                  \
                    | \"join\" | \"eval\" | '+' | '-' | '*' | '/';  \
            sexpr: '(' <expr>* ')';                                 \
            qexpr: '{' <expr>* '}';                                 \
            expr: <number> | <symbol> | <sexpr> | <qexpr> ;         \
            lispy: /^/ <expr>* /$/;                                 \
        ",
              number, symbol, sexpr, qexpr, expr, lispy);

    while (1)
    {
        fputs("kei> ", stdout);

        fgets(input, 2048, stdin);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, lispy, &r))
        {
            lval* x = lval_eval(lval_read(r.output));
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
