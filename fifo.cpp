/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "torque.h"
#include <time.h>
#include <string.h>
#include "queue_info.h"
#include "server_info.h"
#include "check.h"
#include "constant.h"
#include "job_info.h"
#include "node_info.h"
#include "misc.h"
#include "fifo.h"
#include "config.h"
#include "sort.h"
#include "parse.h"
#include "globals.h"
#include "prev_job_info.h"
#include "fairshare.h"
#include "prime.h"
#include "dedtime.h"
//#include "token_acct.h"
#include "global_macros.h"

extern "C" {
//#include "dis.h"
#include <pbs_ifl.h>
}


/* plan-based scheduler */
#include "plan_config.h"
#include "plan_data_types.h"
#include "plan_operations.h"
#include "plan_schedule.h"
#include "plan_first_free_slot.h"
#include "plan_list_operations.h"
#include "plan_free_data.h"
#include "plan_optimization.h"
#include "plan_log.h"
#include "plan_jobs.h"
#include "node_info.h"

#include "api.hpp"
#include <sstream>
#include <stdexcept>
using namespace std;

/* new scheduler */
sched* new_sched;
time_t optimization_time = 0;
int update_qstat = 0;

static time_t last_decay;
time_t last_sync;

extern void dump_current_fairshare(group_info *root);


/*
 *
 * schedinit - initialize conf struct and parse conf files
 *
 *   argc - passed in from main
 *   argv - passed in from main
 *
 * Returns Success/Failure
 *
 *
 */
int schedinit(int UNUSED(argc), char * UNUSED(argv[]))
  {
  init_config();
  parse_config(CONFIG_FILE);
  parse_holidays(HOLIDAYS_FILE);

  time(&(cstat.current_time));
  
  srand( time (NULL));
  new_sched = schedule_create();
  optimization_time=0;

  if (is_prime_time())
    init_prime_time();
  else
    init_non_prime_time();

  parse_ded_file(DEDTIME_FILE);

  /* preload the static members to the fairshare tree */
  init_fairshare();

  if (conf.prime_fs || conf.non_prime_fs)
    {
    /*
     * initialize the last_decay to the current time, since we do not know when
     * the last decay happened
     */
    last_sync = last_decay = cstat.current_time;
    }

//  token_acct_open((char *)0);

  return 0;
  }

/*
 *
 * update_cycle_status - update global status structure which holds
 *         status information used by the scheduler which
 *         can change from cycle to cycle
 *
 * returns nothing
 *
 */
void
update_cycle_status(void)
  {
  char dedtime;    /* boolean: is it dedtime? */
  enum prime_time prime;  /* current prime time status */

  time(&(cstat.current_time));
  dedtime = is_ded_time();

  /* it was dedtime last scheduling cycle, and it is not dedtime now */

  if (cstat.is_ded_time && !dedtime)
    {
    /* since the current dedtime is now passed, set it to zero and sort it to
     * the end of the dedtime array so the next dedtime is at the front
     */
    conf.ded_time[0].from = 0;
    conf.ded_time[0].to = 0;
    qsort(conf.ded_time, MAX_DEDTIME_SIZE, sizeof(struct timegap), cmp_ded_time);
    }

  cstat.is_ded_time = dedtime;


  /* if we have changed from prime to nonprime or nonprime to prime
   * init the status respectivly
   */
  prime = is_prime_time();

  if (prime == PRIME && !cstat.is_prime)
    init_prime_time();
  else if (prime == NON_PRIME && cstat.is_prime)
    init_non_prime_time();
  }

int job_is_movable(JobInfo* job)
  {
  if (!job->suitable_for_run() || (!job->queue->is_global) || (job->state != JobQueued))
    return 0;
  else
    return 1;
}

/*
 *
 * next_job - find the next job to be run by the scheduler
 *
 *   sinfo - the server the jobs are on
 *   init  - whether or not to initialize
 *
 * returns
 *     - the next job to run
 *     - NULL on error or if there are no more jobs to run
 *
 */
JobInfo *next_job(server_info *sinfo, int init)
  {
  /* last_queue is the index into a queue array of the last time
   * the function was called
   */
  static int last_queue;

  /* last_job is the index into a job array of the last time
   * the function was called
   */
  static int last_job;

  JobInfo *rjob = NULL;  /* the job to return */

  if (cstat.round_robin)
    {
    throw invalid_argument("Round robin job scheduling no longer supported.");
    }
  else if (cstat.by_queue)
    {
    if (init == INITIALIZE)
      {
      last_job = -1;
      last_queue = 0;
      }
    else
      {
      if (cstat.fair_share)
        {
        do
          {
          rjob = extract_fairshare(sinfo->queues[last_queue]->jobs);
          if (rjob == NULL) last_queue++;
          }
        while (rjob == NULL && last_queue < sinfo->num_queues);
        }
      else
        {
        last_job++;

        /* check if we have reached the end of a queue and need to find another */

        while (last_queue < sinfo -> num_queues &&
               (sinfo->queues[last_queue]->jobs.size() == static_cast<size_t>(last_job) ||
                sinfo->queues[last_queue]->jobs[last_job]->can_not_run))
          {
          if (static_cast<size_t>(last_job) == sinfo->queues[last_queue]->jobs.size())
            {
            last_queue++;
            last_job = 0;
            }
          else if (sinfo->queues[last_queue]->jobs[last_job]->can_not_run)
            {
            last_job++;
            }
          }

        if (last_queue == sinfo -> num_queues || sinfo -> queues[last_queue] == NULL)
          rjob = NULL;
        else
          {
          if (last_job < sinfo -> queues[last_queue] -> sc.total)
            rjob = sinfo -> queues[last_queue] -> jobs[last_job];
          }
        }
      }
    }
  else /* treat the entire system as one large queue */
    {
    if (init == INITIALIZE)
      {
      last_job = -1;
      }
    else
      {
      if (cstat.fair_share)
        rjob = extract_fairshare(sinfo);
      else
        {
        last_job++;

        while (sinfo->jobs.size() > static_cast<size_t>(last_job) && sinfo->jobs[last_job]->can_not_run)
          ++last_job;

        if (static_cast<size_t>(last_job) == sinfo->jobs.size())
          rjob = NULL;
        else
          rjob =  sinfo->jobs[last_job];
        }
      }
    }

  return rjob;
  }
