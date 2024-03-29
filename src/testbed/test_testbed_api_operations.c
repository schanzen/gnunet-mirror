/*
      This file is part of GNUnet
      (C) 2008--2012 Christian Grothoff (and other contributing authors)

      GNUnet is free software; you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published
      by the Free Software Foundation; either version 3, or (at your
      option) any later version.

      GNUnet is distributed in the hope that it will be useful, but
      WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
      General Public License for more details.

      You should have received a copy of the GNU General Public License
      along with GNUnet; see the file COPYING.  If not, write to the
      Free Software Foundation, Inc., 59 Temple Place - Suite 330,
      Boston, MA 02111-1307, USA.
 */

/**
 * @file testbed/test_testbed_api_operations.c
 * @brief tests cases for testbed_api_operations.c
 * @author Sree Harsha Totakura
 */

#include "platform.h"
#include "gnunet_util_lib.h"
#include "testbed_api_operations.h"

/**
 * Generic logging shortcut
 */
#define LOG(kind,...)				\
  GNUNET_log (kind, __VA_ARGS__)

/**
 * Queue A. Initially the max active is set to 2 and then reduced to 0 - this
 * should block op2 even after op1 has finished. Later the max active is set to
 * 1 and this should start op2
 */
struct OperationQueue *q1;

/**
 * Queue B. Max active set to 1
 */
struct OperationQueue *q2;

/**
 * This operation should go into both queues and block op2 until it is done
 */
struct GNUNET_TESTBED_Operation *op1;

/**
 * This operation should go into q1 and q2
 */
struct GNUNET_TESTBED_Operation *op2;

/**
 * This operation should go into both queues and should be started after op2 has
 * been released.
 */
struct GNUNET_TESTBED_Operation *op3;

/**
 * The delay task identifier
 */
GNUNET_SCHEDULER_TaskIdentifier step_task;


/**
 * Enumeration of test stages
 */
enum Test
{
    /**
     * Initial stage
     */
  TEST_INIT,

    /**
     * op1 has been started
     */
  TEST_OP1_STARTED,

    /**
     * op1 has been released
     */
  TEST_OP1_RELEASED,

    /**
     * op2 has started
     */
  TEST_OP2_STARTED,

    /**
     * op2 released
     */
  TEST_OP2_RELEASED,

  /**
   * Temporary pause where no operations should start as we set max active in q1
   * to 0
   */
  TEST_PAUSE,

  /**
   * op3 has started
   */
  TEST_OP3_STARTED,

  /**
   * op3 has finished
   */
  TEST_OP3_RELEASED
};

/**
 * The test result
 */
enum Test result;


/**
 * Function to call to start an operation once all
 * queues the operation is part of declare that the
 * operation can be activated.
 */
static void
start_cb (void *cls);


/**
 * Function to cancel an operation (release all associated resources).  This can
 * be because of a call to "GNUNET_TESTBED_operation_cancel" (before the
 * operation generated an event) or AFTER the operation generated an event due
 * to a call to "GNUNET_TESTBED_operation_done".  Thus it is not guaranteed that
 * a callback to the 'OperationStart' preceeds the call to 'OperationRelease'.
 * Implementations of this function are expected to clean up whatever state is
 * in 'cls' and release all resources associated with the operation.
 */
static void
release_cb (void *cls);


/**
 * Task to simulate artificial delay and change the test stage
 *
 * @param cls NULL
 * @param tc the task context
 */
static void
step (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  GNUNET_assert (GNUNET_SCHEDULER_NO_TASK != step_task);
  step_task = GNUNET_SCHEDULER_NO_TASK;
  switch (result)
  {
  case TEST_OP1_STARTED:
    GNUNET_TESTBED_operation_release_ (op1);
    GNUNET_TESTBED_operation_queue_reset_max_active_ (q1, 0);
    op3 = GNUNET_TESTBED_operation_create_ (&op3, &start_cb, &release_cb);    
    GNUNET_TESTBED_operation_queue_insert_ (q1, op3);
    GNUNET_TESTBED_operation_queue_insert_ (q2, op3);
    GNUNET_TESTBED_operation_begin_wait_ (op3);
    break;
  case TEST_OP2_STARTED:
    GNUNET_TESTBED_operation_release_ (op2);
    break;
  case TEST_OP2_RELEASED:
    result = TEST_PAUSE;
    GNUNET_TESTBED_operation_queue_reset_max_active_ (q1, 1);
    break;
  case TEST_OP3_STARTED:
    GNUNET_TESTBED_operation_release_ (op3);
    break;
  default:
    GNUNET_assert (0);
  }
}


/**
 * Function to call to start an operation once all
 * queues the operation is part of declare that the
 * operation can be activated.
 */
static void
start_cb (void *cls)
{
  switch (result)
  {
  case TEST_INIT:
    GNUNET_assert (&op1 == cls);
    result = TEST_OP1_STARTED;
    GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == step_task);
    step_task = GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_SECONDS, &step, NULL);
    break;
  case TEST_OP1_RELEASED:
    GNUNET_assert (&op2 == cls);
    result = TEST_OP2_STARTED;
    GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == step_task);
    step_task = GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_SECONDS, &step, NULL);
    break;
  case TEST_PAUSE:
    GNUNET_assert (&op3 == cls);
    result = TEST_OP3_STARTED;
    GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == step_task);
    step_task = GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_SECONDS, &step, NULL);
    break;
  default:
    GNUNET_assert (0);
  }
}


/**
 * Function to cancel an operation (release all associated resources).  This can
 * be because of a call to "GNUNET_TESTBED_operation_cancel" (before the
 * operation generated an event) or AFTER the operation generated an event due
 * to a call to "GNUNET_TESTBED_operation_done".  Thus it is not guaranteed that
 * a callback to the 'OperationStart' preceeds the call to 'OperationRelease'.
 * Implementations of this function are expected to clean up whatever state is
 * in 'cls' and release all resources associated with the operation.
 */
static void
release_cb (void *cls)
{
  switch (result)
  {
  case TEST_OP1_STARTED:
    GNUNET_assert (&op1 == cls);
    result = TEST_OP1_RELEASED;
    op1 = NULL;
    //GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_SECONDS, &step, NULL);
    break;
  case TEST_OP2_STARTED:
    GNUNET_assert (&op2 == cls);
    result = TEST_OP2_RELEASED;
    GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == step_task);
    step_task = GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_SECONDS, &step, NULL);
    break;
  case TEST_OP3_STARTED:
    GNUNET_assert (&op3 == cls);
    result = TEST_OP3_RELEASED;
    GNUNET_TESTBED_operation_queue_destroy_ (q1);
    GNUNET_TESTBED_operation_queue_destroy_ (q2);
    break;
  default:
    GNUNET_assert (0);
  }
}


/**
 * Main run function.
 *
 * @param cls NULL
 * @param args arguments passed to GNUNET_PROGRAM_run
 * @param cfgfile the path to configuration file
 * @param cfg the configuration file handle
 */
static void
run (void *cls, char *const *args, const char *cfgfile,
     const struct GNUNET_CONFIGURATION_Handle *config)
{
  q1 = GNUNET_TESTBED_operation_queue_create_ (1);
  GNUNET_assert (NULL != q1);
  q2 = GNUNET_TESTBED_operation_queue_create_ (2);
  GNUNET_assert (NULL != q2);
  op1 = GNUNET_TESTBED_operation_create_ (&op1, start_cb, release_cb);
  GNUNET_assert (NULL != op1);
  op2 = GNUNET_TESTBED_operation_create_ (&op2, start_cb, release_cb);
  GNUNET_TESTBED_operation_queue_insert_ (q1, op1);
  GNUNET_TESTBED_operation_queue_insert_ (q2, op1);
  GNUNET_TESTBED_operation_begin_wait_ (op1);
  GNUNET_TESTBED_operation_queue_insert_ (q1, op2);
  GNUNET_TESTBED_operation_queue_insert_ (q2, op2);
  GNUNET_TESTBED_operation_begin_wait_ (op2);
  result = TEST_INIT;
}

/**
 * Main function
 */
int
main (int argc, char **argv)
{
  int ret;
  char *const argv2[] =
      { "test_testbed_api_operations", "-c", "test_testbed_api.conf", NULL };
  struct GNUNET_GETOPT_CommandLineOption options[] =
      { GNUNET_GETOPT_OPTION_END };

  ret =
      GNUNET_PROGRAM_run ((sizeof (argv2) / sizeof (char *)) - 1, argv2,
                          "test_testbed_api_operations", "nohelp", options,
                          &run, NULL);
  if ((GNUNET_OK != ret) || (TEST_OP3_RELEASED != result))
    return 1;
  op1 = NULL;
  op2 = NULL;
  op3 = NULL;
  q1 = NULL;
  q2 = NULL;
  return 0;
}

/* end of test_testbed_api_operations.c */
