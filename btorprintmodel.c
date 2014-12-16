/*  Boolector: Satisfiablity Modulo Theories (SMT) solver.
 *
 *  Copyright (C) 2014 Mathias Preiner.
 *  Copyright (C) 2014 Aina Niemetz.
 *
 *  All rights reserved.
 *
 *  This file is part of Boolector.
 *  See COPYING for more information on using this software.
 */

#include "btorprintmodel.h"
#include "btorconst.h"
#include "btorhash.h"
#include "btoriter.h"
#include "btormodel.h"
#include "dumper/btordumpsmt.h"

const char *
btor_get_bv_model_str (Btor *btor, BtorNode *exp)
{
  assert (btor);
  assert (exp);

  const char *res;
  const BitVector *bv;

  exp = btor_simplify_exp (btor, exp);
  if (!(bv = btor_get_bv_model (btor, exp)))
    return btor_x_const_3vl (btor->mm, BTOR_REAL_ADDR_NODE (exp)->len);
  res = btor_bv_to_char_bv (btor, bv);
  return res;
}

void
btor_get_fun_model_str (
    Btor *btor, BtorNode *exp, char ***args, char ***values, int *size)
{
  assert (btor);
  assert (exp);
  assert (args);
  assert (values);
  assert (size);
  assert (BTOR_IS_REGULAR_NODE (exp));

  char *arg, *tmp, *bv;
  int i, j, len;
  BtorHashTableIterator it;
  const BtorPtrHashTable *model;
  BitVector *value;
  BitVectorTuple *t;

  exp = btor_simplify_exp (btor, exp);
  assert (BTOR_IS_FUN_NODE (exp));

  model = btor_get_fun_model (btor, exp);

  if ((BTOR_IS_LAMBDA_NODE (exp) && ((BtorLambdaNode *) exp)->num_params > 1)
      || !btor->fun_model || !model)
  {
    *size = 0;
    return;
  }

  assert (model->count > 0);

  *size = (int) model->count;
  BTOR_NEWN (btor->mm, *args, *size);
  BTOR_NEWN (btor->mm, *values, *size);

  i = 0;
  init_hash_table_iterator (&it, (BtorPtrHashTable *) model);
  while (has_next_hash_table_iterator (&it))
  {
    value = (BitVector *) it.bucket->data.asPtr;

    /* build assignment string for all arguments */
    t   = (BitVectorTuple *) next_hash_table_iterator (&it);
    len = t->arity;
    for (j = 0; j < t->arity; j++) len += t->bv[j]->width;
    BTOR_NEWN (btor->mm, arg, len);
    tmp = arg;

    bv = (char *) btor_bv_to_char_bv (btor, t->bv[0]);
    strcpy (tmp, bv);
    btor_release_bv_assignment_str (btor, bv);

    for (j = 1; j < t->arity; j++)
    {
      bv = (char *) btor_bv_to_char_bv (btor, t->bv[j]);
      strcat (tmp, " ");
      strcat (tmp, bv);
      btor_release_bv_assignment_str (btor, bv);
    }
    assert ((int) strlen (arg) == len - 1);

    (*args)[i]   = arg;
    (*values)[i] = (char *) btor_bv_to_char_bv (btor, value);
    i++;
  }
}

static void
print_fmt_bv_model_btor (Btor *btor, int base, char *assignment, FILE *file)
{
  assert (btor);
  assert (assignment);
  assert (file);

  BtorCharPtrStack values;
  char *fmt, *ground, *tok;
  int i, len, orig_len;

  BTOR_INIT_STACK (values);

  /* assignment may contain multiple values (in case of function args) */
  len      = 0;
  fmt      = btor_strdup (btor->mm, assignment);
  orig_len = strlen (fmt) + 1;
  tok      = strtok (fmt, " ");
  do
  {
    if (base == BTOR_OUTPUT_BASE_HEX || base == BTOR_OUTPUT_BASE_DEC)
    {
      ground = btor_ground_const_3vl (btor->mm, tok);
      if (base == BTOR_OUTPUT_BASE_HEX)
        tok = btor_const_to_hex (btor->mm, ground);
      else
      {
        assert (base == BTOR_OUTPUT_BASE_DEC);
        tok = btor_const_to_decimal (btor->mm, ground);
      }
      btor_delete_const (btor->mm, ground);
    }
    else
      tok = btor_copy_const (btor->mm, tok);
    len += strlen (tok) + 1;
    BTOR_PUSH_STACK (btor->mm, values, tok);
  } while ((tok = strtok (0, " ")));
  btor_free (btor->mm, fmt, orig_len * sizeof (char));

  /* concat formatted assignment strings */
  BTOR_NEWN (btor->mm, fmt, len);
  assert (!BTOR_EMPTY_STACK (values));
  for (i = 0; i < BTOR_COUNT_STACK (values); i++)
  {
    tok = BTOR_PEEK_STACK (values, i);
    if (i == 0)
      strcpy (fmt, tok);
    else
    {
      strcat (fmt, " ");
      strcat (fmt, tok);
    }
    btor_freestr (btor->mm, tok);
  }
  BTOR_RELEASE_STACK (btor->mm, values);
  fprintf (file, "%s", fmt);
  btor_freestr (btor->mm, fmt);
}

static void
print_bv_model (Btor *btor, BtorNode *node, char *format, int base, FILE *file)
{
  assert (btor);
  assert (format);
  assert (node);
  assert (BTOR_IS_REGULAR_NODE (node));

  int id;
  char *symbol;
  char *ass;

  ass = (char *) btor_get_bv_model_str (btor, node);
  if (!btor_is_const_2vl (btor->mm, ass)) return;

  symbol = btor_get_symbol_exp (btor, node);

  if (!strcmp (format, "btor"))
  {
    id = ((BtorBVVarNode *) node)->btor_id;
    fprintf (file, "%d ", id ? id : node->id);
    print_fmt_bv_model_btor (btor, base, ass, file);
    fprintf (file, "%s%s\n", symbol ? " " : "", symbol ? symbol : "");
  }
  else
  {
    if (symbol)
      fprintf (file, "%2c(define-fun %s () ", ' ', symbol);
    else
      fprintf (file,
               "%2c(define-fun v%d () ",
               ' ',
               ((BtorBVVarNode *) node)->btor_id
                   ? ((BtorBVVarNode *) node)->btor_id
                   : node->id);

    btor_dump_sort_smt_node (node, 2, file);
    fprintf (file, " ");
    btor_dump_const_value_smt (btor, ass, base, 2, file);
    fprintf (file, ")\n");
  }
  btor_release_bv_assignment_str (btor, (char *) ass);
}

static void
print_param_smt2 (char *symbol, int param_index, BtorSort *sort, FILE *file)
{
  assert (symbol);
  assert (sort);
  assert (file);

  fprintf (file, "(%s_x%d ", symbol, param_index);
  btor_dump_sort_smt (sort, 2, file);
  fprintf (file, ")");
}

static void
print_fun_model_smt2 (Btor *btor, BtorNode *node, int base, FILE *file)
{
  assert (btor);
  assert (node);
  assert (BTOR_IS_REGULAR_NODE (node));
  assert (file);

  char *s, *symbol, *ass;
  int i, x, n, len;
  BtorPtrHashTable *fun_model;
  BtorHashTableIterator it;
  BitVectorTuple *args;
  BitVector *assignment;
  BtorSort *sort;

  fun_model = (BtorPtrHashTable *) btor_get_fun_model (btor, node);
  if (!fun_model) return;

  if ((symbol = btor_get_symbol_exp (btor, node)))
    s = symbol;
  else
  {
    BTOR_NEWN (btor->mm, s, 40);
    sprintf (s,
             "%s%d",
             BTOR_IS_UF_ARRAY_NODE (node) ? "a" : "uf",
             ((BtorUFNode *) node)->btor_id ? ((BtorUFNode *) node)->btor_id
                                            : node->id);
  }

  fprintf (file, "%2c(define-fun %s (", ' ', s);

  /* fun param sorts */
  x    = 0;
  sort = ((BtorUFNode *) node)->sort;
  if (sort->fun.domain->kind != BTOR_TUPLE_SORT) /* one parameter */
  {
    fprintf (file, "\n%3c", ' ');
    print_param_smt2 (s, x, sort->fun.domain, file);
  }
  else
  {
    for (i = 0; i < sort->fun.domain->tuple.num_elements; i++, x++)
    {
      fprintf (file, "\n%3c", ' ');
      print_param_smt2 (s, x, sort->fun.domain->tuple.elements[i], file);
    }
  }
  fprintf (file, ") ");
  btor_dump_sort_smt (sort->fun.codomain, 2, file);
  fprintf (file, "\n");

  /* fun model as ite over args and assignments */
  n          = 0;
  assignment = 0;
  init_hash_table_iterator (&it, fun_model);
  while (has_next_hash_table_iterator (&it))
  {
    fprintf (file, "%4c(ite ", ' ');
    assignment = it.bucket->data.asPtr;
    args       = next_hash_table_iterator (&it);
    x          = 0;
    if (args->arity > 1)
    {
      fprintf (file, "\n%6c(and ", ' ');
      for (i = 0; i < args->arity; i++, x++)
      {
        ass = btor_bv_to_char_bv (btor, args->bv[i]);
        fprintf (file, "\n%8c(= %s_x%d ", ' ', s, x);
        btor_dump_const_value_smt (btor, ass, base, 2, file);
        fprintf (file, ")%s", i + 1 == args->arity ? "" : " ");
        btor_freestr (btor->mm, ass);
      }
      fprintf (file, ") ");
      fprintf (file, "\n%6c", ' ');
    }
    else
    {
      ass = btor_bv_to_char_bv (btor, args->bv[0]);
      fprintf (file, "(= %s_x%d ", s, x);
      btor_dump_const_value_smt (btor, ass, base, 2, file);
      fprintf (file, ") ");
      btor_freestr (btor->mm, ass);
    }
    ass = btor_bv_to_char_bv (btor, assignment);
    btor_dump_const_value_smt (btor, ass, base, 2, file);
    fprintf (file, "\n");
    btor_freestr (btor->mm, ass);
    n += 1;
  }

  assert (assignment);
  if (assignment) /* get rid of compiler warning */
  {
    fprintf (file, "%6c", ' ');
    len = assignment->width + 1;
    BTOR_NEWN (btor->mm, ass, len);
    memset (ass, '0', len - 1);
    ass[len - 1] = 0;
    btor_dump_const_value_smt (btor, ass, base, 2, file);
    BTOR_DELETEN (btor->mm, ass, len);
  }

  for (i = 0; i < n; i++) fprintf (file, ")");
  fprintf (file, ")\n");

  if (!symbol) BTOR_DELETEN (btor->mm, s, 40);
}

static void
print_fun_model_btor (Btor *btor, BtorNode *node, int base, FILE *file)
{
  assert (btor);
  assert (node);
  assert (BTOR_IS_REGULAR_NODE (node));
  assert (file);

  char **args, **val, *symbol;
  int i, size, id;

  btor_get_fun_model_str (btor, node, &args, &val, &size);

  if (!size) return;

  for (i = 0; i < size; i++)
  {
    symbol = btor_get_symbol_exp (btor, node);
    id     = ((BtorUFNode *) node)->btor_id;
    // TODO: distinguish between functions and arrays (ma)
    //       needs proper sort handling
    fprintf (file, "%d[", id ? id : node->id);
    print_fmt_bv_model_btor (btor, base, args[i], file);
    fprintf (file, "] ");
    print_fmt_bv_model_btor (btor, base, val[i], file);
    fprintf (file, "%s%s\n", symbol ? " " : "", symbol ? symbol : "");
    btor_freestr (btor->mm, args[i]);
    btor_freestr (btor->mm, val[i]);
  }
  btor_free (btor->mm, args, size * sizeof (*args));
  btor_free (btor->mm, val, size * sizeof (*val));
}

static void
print_fun_model (Btor *btor, BtorNode *node, char *format, int base, FILE *file)
{
  assert (btor);
  assert (node);
  assert (format);
  assert (file);

  if (!strcmp (format, "btor"))
    print_fun_model_btor (btor, BTOR_REAL_ADDR_NODE (node), base, file);
  else
    print_fun_model_smt2 (btor, BTOR_REAL_ADDR_NODE (node), base, file);
}

void
btor_print_model (Btor *btor, char *format, FILE *file)
{
  assert (btor);
  assert (format);
  assert (!strcmp (format, "btor") || !strcmp (format, "smt2"));
  assert (btor->last_sat_result == BTOR_SAT);
  assert (file);

  BtorNode *cur, *simp;
  BtorHashTableIterator it;
  int base;

  base = btor_get_opt_val (btor, BTOR_OPT_OUTPUT_NUMBER_FORMAT);

  if (!strcmp (format, "smt2")) fprintf (file, "(model\n");

  init_node_hash_table_iterator (&it, btor->inputs);
  while (has_next_node_hash_table_iterator (&it))
  {
    cur  = next_node_hash_table_iterator (&it);
    simp = btor_simplify_exp (btor, cur);
    if (BTOR_IS_FUN_NODE (BTOR_REAL_ADDR_NODE (simp)))
      print_fun_model (btor, cur, format, base, file);
    else
      print_bv_model (btor, cur, format, base, file);
  }

  if (!strcmp (format, "smt2")) fprintf (file, ")\n");
}
