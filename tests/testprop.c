/*  Boolector: Satisfiablity Modulo Theories (SMT) solver.
 *
 *  Copyright (C) 2015 Aina Niemetz.
 *
 *  All rights reserved.
 *
 *  This file is part of Boolector.
 *  See COPYING for more information on using this software.
 */

#include "testprop.h"
#include "btorbitvec.h"
#include "btorcore.h"
#include "btorexp.h"
#include "btormodel.h"
#include "btorprop.h"
#include "btorsls.h"
#include "testrunner.h"
#include "utils/btorutil.h"

static Btor *g_btor;
static BtorMemMgr *g_mm;
static BtorRNG *g_rng;

#define TEST_PROP_ONE_COMPLETE_BW 4
#define TEST_PROP_ONE_COMPLETE_N_TESTS 1000

#define TEST_PROP_ONE_COMPLETE_BINARY_INIT(fun)                        \
  do                                                                   \
  {                                                                    \
    g_btor                            = btor_new_btor ();              \
    g_btor->slv                       = btor_new_prop_solver (g_btor); \
    g_btor->options.engine.val        = BTOR_ENGINE_PROP;              \
    g_btor->options.rewrite_level.val = 0;                             \
    g_btor->options.sort_exp.val      = 0;                             \
    g_btor->options.incremental.val   = 1;                             \
    /*g_btor->options.loglevel.val = 1; \*/                            \
    g_mm  = g_btor->mm;                                                \
    g_rng = &g_btor->rng;                                              \
    bw0 = bw1 = TEST_PROP_ONE_COMPLETE_BW;                             \
  } while (0)

#define TEST_PROP_ONE_COMPLETE_BINARY_FINISH(fun) \
  do                                              \
  {                                               \
    btor_delete_btor (g_btor);                    \
  } while (0)

static inline void
prop_one_complete_binary_eidx (
    int eidx,
    uint32_t bw0,
    uint32_t bw1,
    BtorBitVector *bve,
    BtorBitVector *bvres,
    BtorBitVector *bvexp,
    BtorNode *(*create_exp) (Btor *, BtorNode *, BtorNode *),
    BtorBitVector *(*create_bv) (BtorMemMgr *,
                                 BtorBitVector *,
                                 BtorBitVector *),
    BtorBitVector *(*inv_bv) (
        Btor *, BtorNode *, BtorBitVector *, BtorBitVector *, int) )
{
  int i, sat_res;
  BtorNode *e[2], *exp, *val, *eq;
  BtorBitVector *bvetmp, *bvexptmp, *res, *tmp;

  e[0] = btor_var_exp (g_btor, bw0, 0);
  e[1] = btor_var_exp (g_btor, bw1, 0);
  exp  = create_exp (g_btor, e[0], e[1]);
  val  = btor_const_exp (g_btor, bvexp);
  eq   = btor_eq_exp (g_btor, exp, val);

  bvetmp = btor_new_random_bv (g_mm, g_rng, eidx ? bw1 : bw0);
  bvexptmp =
      eidx ? create_bv (g_mm, bve, bvetmp) : create_bv (g_mm, bvetmp, bve);
  /* init bv model */
  btor_init_bv_model (g_btor, &g_btor->bv_model);
  btor_init_fun_model (g_btor, &g_btor->fun_model);
  btor_add_to_bv_model (g_btor, g_btor->bv_model, e[eidx ? 0 : 1], bve);
  btor_add_to_bv_model (g_btor, g_btor->bv_model, e[eidx], bvetmp);
  btor_add_to_bv_model (g_btor, g_btor->bv_model, exp, bvexptmp);

  /* -> first test local completeness  */
  /* we must find a solution within one move */
  res = inv_bv (g_btor, exp, bvexp, bve, eidx);
  assert (res);
  /* Note: this is also tested within the inverse function(s) */
  tmp = eidx ? create_bv (g_mm, bve, res) : create_bv (g_mm, res, bve);
  assert (!btor_compare_bv (tmp, bvexp));
  btor_free_bv (g_mm, tmp);
  btor_free_bv (g_mm, res);
  /* try to find the exact given solution */
  for (i = 0, res = 0; i < TEST_PROP_ONE_COMPLETE_N_TESTS; i++)
  {
    res = inv_bv (g_btor, exp, bvexp, bve, eidx);
    assert (res);
    if (!btor_compare_bv (res, bvres)) break;
    btor_free_bv (g_mm, res);
    res = 0;
  }
  assert (res);
  assert (!btor_compare_bv (res, bvres));
  btor_free_bv (g_mm, res);

  /* -> then test completeness of the whole propagation algorithm
   *    (we must find a solution within one move) */
  ((BtorPropSolver *) g_btor->slv)->stats.moves = 0;
  btor_assume_exp (g_btor, eq);
  btor_init_bv_model (g_btor, &g_btor->bv_model);
  btor_init_fun_model (g_btor, &g_btor->fun_model);
  btor_add_to_bv_model (g_btor, g_btor->bv_model, e[eidx ? 0 : 1], bve);
  btor_add_to_bv_model (g_btor, g_btor->bv_model, e[eidx], bvetmp);
  btor_add_to_bv_model (g_btor, g_btor->bv_model, exp, bvexptmp);
  btor_free_bv (g_mm, bvetmp);
  btor_free_bv (g_mm, bvexptmp);
  btor_release_exp (g_btor, eq);
  btor_release_exp (g_btor, val);
  btor_release_exp (g_btor, exp);
  btor_release_exp (g_btor, e[0]);
  btor_release_exp (g_btor, e[1]);
  sat_res = sat_prop_solver_aux (g_btor, -1, -1);
  assert (sat_res == BTOR_SAT);
  assert (((BtorPropSolver *) g_btor->slv)->stats.moves <= 1);
}

static void
prop_one_complete_binary (
    BtorNode *(*create_exp) (Btor *, BtorNode *, BtorNode *),
    BtorBitVector *(*create_bv) (BtorMemMgr *,
                                 BtorBitVector *,
                                 BtorBitVector *),
    BtorBitVector *(*inv_bv) (
        Btor *, BtorNode *, BtorBitVector *, BtorBitVector *, int) )
{
  uint32_t bw0, bw1;
  uint64_t i, j, k;
  BtorBitVector *bve[2], *bvexp;

  TEST_PROP_ONE_COMPLETE_BINARY_INIT (create_exp);
  if (create_exp == btor_sll_exp || create_exp == btor_srl_exp)
    bw1 = btor_log_2_util (bw0);

  for (i = 0; i < (uint32_t) (1 << bw0); i++)
  {
    bve[0] = btor_uint64_to_bv (g_mm, i, bw0);
    for (j = 0; j < (uint32_t) (1 << bw1); j++)
    {
      bve[1] = btor_uint64_to_bv (g_mm, j, bw1);
      bvexp  = create_bv (g_mm, bve[0], bve[1]);
      for (k = 0; k < bw0; k++)
      {
        prop_one_complete_binary_eidx (
            1, bw0, bw1, bve[0], bve[1], bvexp, create_exp, create_bv, inv_bv);
        prop_one_complete_binary_eidx (
            0, bw0, bw1, bve[1], bve[0], bvexp, create_exp, create_bv, inv_bv);
      }
      btor_free_bv (g_mm, bve[1]);
      btor_free_bv (g_mm, bvexp);
    }
    btor_free_bv (g_mm, bve[0]);
  }

  TEST_PROP_ONE_COMPLETE_BINARY_FINISH (fun);
}

static void
test_prop_one_complete_add_bv (void)
{
#ifndef NDEBUG
  prop_one_complete_binary (btor_add_exp, btor_add_bv, inv_add_bv);
#endif
}

static void
test_prop_one_complete_and_bv (void)
{
#ifndef NDEBUG
  prop_one_complete_binary (btor_and_exp, btor_and_bv, inv_and_bv);
#endif
}

static void
test_prop_one_complete_eq_bv (void)
{
#ifndef NDEBUG
  prop_one_complete_binary (btor_eq_exp, btor_eq_bv, inv_eq_bv);
#endif
}

static void
test_prop_one_complete_ult_bv (void)
{
#ifndef NDEBUG
  prop_one_complete_binary (btor_ult_exp, btor_ult_bv, inv_ult_bv);
#endif
}

static void
test_prop_one_complete_sll_bv (void)
{
#ifndef NDEBUG
  prop_one_complete_binary (btor_sll_exp, btor_sll_bv, inv_sll_bv);
#endif
}

static void
test_prop_one_complete_srl_bv (void)
{
#ifndef NDEBUG
  prop_one_complete_binary (btor_srl_exp, btor_srl_bv, inv_srl_bv);
#endif
}

static void
test_prop_one_complete_mul_bv (void)
{
#ifndef NDEBUG
  prop_one_complete_binary (btor_mul_exp, btor_mul_bv, inv_mul_bv);
#endif
}

static void
test_prop_one_complete_udiv_bv (void)
{
#ifndef NDEBUG
  prop_one_complete_binary (btor_udiv_exp, btor_udiv_bv, inv_udiv_bv);
#endif
}

static void
test_prop_one_complete_urem_bv (void)
{
#ifndef NDEBUG
  prop_one_complete_binary (btor_urem_exp, btor_urem_bv, inv_urem_bv);
#endif
}

static void
test_prop_one_complete_concat_bv (void)
{
#ifndef NDEBUG
  prop_one_complete_binary (btor_concat_exp, btor_concat_bv, inv_concat_bv);
#endif
}

static void
test_prop_one_complete_slice_bv (void)
{
#ifndef NDEBUG
  int sat_res;
  uint32_t bw;
  uint64_t up, lo, i, j, k;
  BtorNode *exp, *e, *val, *eq;
  BtorBitVector *bve, *bvexp, *bvetmp, *bvexptmp, *res, *tmp;

  g_btor                            = btor_new_btor ();
  g_btor->slv                       = btor_new_prop_solver (g_btor);
  g_btor->options.engine.val        = BTOR_ENGINE_PROP;
  g_btor->options.rewrite_level.val = 0;
  g_btor->options.sort_exp.val      = 0;
  g_btor->options.incremental.val   = 1;
  // g_btor->options.loglevel.val = 1;
  g_mm  = g_btor->mm;
  g_rng = &g_btor->rng;
  bw    = TEST_PROP_ONE_COMPLETE_BW;

  for (lo = 0; lo < bw; lo++)
  {
    for (up = lo; up < bw; up++)
    {
      for (i = 0; i < (uint32_t) (1 << bw); i++)
      {
        for (j = 0; j < bw; j++)
        {
          e        = btor_var_exp (g_btor, bw, 0);
          exp      = btor_slice_exp (g_btor, e, up, lo);
          bve      = btor_uint64_to_bv (g_mm, i, bw);
          bvexp    = btor_slice_bv (g_mm, bve, up, lo);
          val      = btor_const_exp (g_btor, bvexp);
          eq       = btor_eq_exp (g_btor, exp, val);
          bvetmp   = btor_new_random_bv (g_mm, g_rng, bw);
          bvexptmp = btor_slice_bv (g_mm, bvetmp, up, lo);
          /* init bv model */
          btor_init_bv_model (g_btor, &g_btor->bv_model);
          btor_init_fun_model (g_btor, &g_btor->fun_model);
          btor_add_to_bv_model (g_btor, g_btor->bv_model, e, bvetmp);
          btor_add_to_bv_model (g_btor, g_btor->bv_model, exp, bvexptmp);

          /* -> first test local completeness
           *    we must find a solution within one move */
          res = inv_slice_bv (g_btor, exp, bvexp);
          assert (res);
          /* Note: this is also tested within inverse function */
          tmp = btor_slice_bv (g_mm, res, up, lo);
          assert (!btor_compare_bv (tmp, bvexp));
          btor_free_bv (g_mm, tmp);
          btor_free_bv (g_mm, res);
          /* try to find exact given solution */
          for (k = 0, res = 0; k < TEST_PROP_ONE_COMPLETE_N_TESTS; k++)
          {
            res = inv_slice_bv (g_btor, exp, bvexp);
            assert (res);
            if (!btor_compare_bv (res, bve)) break;
            btor_free_bv (g_mm, res);
            res = 0;
          }
          assert (res);
          assert (!btor_compare_bv (res, bve));
          btor_free_bv (g_mm, res);

          /* -> then test completeness of the whole propagation algorithm
           *    (we must find a solution within one move) */
          ((BtorPropSolver *) g_btor->slv)->stats.moves = 0;
          btor_assume_exp (g_btor, eq);
          btor_init_bv_model (g_btor, &g_btor->bv_model);
          btor_init_fun_model (g_btor, &g_btor->fun_model);
          btor_add_to_bv_model (g_btor, g_btor->bv_model, e, bvetmp);
          btor_add_to_bv_model (g_btor, g_btor->bv_model, exp, bvexptmp);
          btor_free_bv (g_mm, bvetmp);
          btor_free_bv (g_mm, bvexptmp);
          btor_release_exp (g_btor, eq);
          btor_release_exp (g_btor, val);
          btor_release_exp (g_btor, exp);
          btor_release_exp (g_btor, e);
          sat_res = sat_prop_solver_aux (g_btor, -1, -1);
          assert (sat_res == BTOR_SAT);
          assert (((BtorPropSolver *) g_btor->slv)->stats.moves <= 1);
        }
      }
    }
  }
  btor_delete_btor (g_btor);
}
#endif

/*------------------------------------------------------------------------*/

void
init_prop_tests (void)
{
}

void
run_prop_tests (int argc, char **argv)
{
  (void) argc;
  (void) argv;
  BTOR_RUN_TEST (prop_one_complete_add_bv);
  BTOR_RUN_TEST (prop_one_complete_and_bv);
  BTOR_RUN_TEST (prop_one_complete_eq_bv);
  BTOR_RUN_TEST (prop_one_complete_ult_bv);
  BTOR_RUN_TEST (prop_one_complete_sll_bv);
  BTOR_RUN_TEST (prop_one_complete_srl_bv);
  BTOR_RUN_TEST (prop_one_complete_mul_bv);
  BTOR_RUN_TEST (prop_one_complete_udiv_bv);
  BTOR_RUN_TEST (prop_one_complete_urem_bv);
  BTOR_RUN_TEST (prop_one_complete_concat_bv);
  BTOR_RUN_TEST (prop_one_complete_slice_bv);
}

void
finish_prop_tests (void)
{
}
