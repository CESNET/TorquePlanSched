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
#include <string.h>
#include <errno.h>
#include <new>
#include <stdexcept>
using namespace std;

#include "torque.h"
#include "queue_info.h"
#include "job_info.h"
#include "constant.h"
#include "misc.h"
#include "config.h"
#include "globals.h"
#include "fairshare.h"
#include "node_info.h"
#include "utility.h"
#include "global_macros.h"

#include "fairshare.h"

#include "base/MiscHelpers.h"


bool query_jobs(int pbs_sd, queue_info *qinfo, vector<JobInfo*>& result)
  {
  /* pbs_selstat() takes a linked list of attropl structs which tell it
   * what information about what jobs to return.  We want all jobs which are
   * in a specified queue
   */

  struct attropl opl =
    {
      NULL, const_cast<char*>(ATTR_q), NULL, NULL, EQ
    };

  /* linked list of jobs returned from pbs_selstat() */

  struct batch_status *jobs;

  /* current job in jobs linked list */

  struct batch_status *cur_job;

  /* current job in jinfo_arr array */
  JobInfo *jinfo;

  /* number of jobs in jinfo_arr */
  int i;

  opl.value = qinfo -> name;

  if ((jobs = pbs_selstat(pbs_sd, &opl, NULL)) == NULL)
    {
    if (pbs_errno > 0)
      {
      fprintf(stderr, "pbs_selstat failed: %d\n", pbs_errno);
      return false;
      }

    return true;
    }

  cur_job = jobs;

  for (i = 0; cur_job != NULL; i++)
    {
    if ((jinfo = new JobInfo(cur_job,qinfo)) == NULL)
      {
      pbs_statfree(jobs);

      for (auto j : result)
        delete j;
      result.clear();

      return false;
      }

    /* get the fair share group info node */
    Scheduler::Logic::FairshareTree &tmp = get_tree(jinfo->queue->fairshare_tree);
    jinfo -> ginfo = tmp.find_alloc_ginfo(jinfo -> account.c_str());

    /* if the job is not in the queued state, don't even allow
     * it to be considered to be run.
     */
    if (jinfo -> state != JobQueued)
      jinfo -> can_not_run = true;

    result.push_back(jinfo);

    cur_job = cur_job -> next;
    }

  pbs_statfree(jobs);
  return true;
  }

/*
 *
 * new_resource_req - allocate and initalize new resoruce_req
 *
 * returns the new resource_req
 *
 */

resource_req *new_resource_req()
  {
  resource_req *resreq;

  if ((resreq = (resource_req *) malloc(sizeof(resource_req))) == NULL)
    return NULL;

  resreq -> name = NULL;

  resreq -> res_str = NULL;

  resreq -> amount = 0;

  resreq -> next = NULL;

  return resreq;
  }

/*
 *
 *  find_alloc_resource_req - find resource_req by name or allocate
 *      and initalize a new resource_req
 *      also adds new one to the list
 *
 *   name - resource_req to find
 *   reqlist - list to look through
 *
 * returns found or newly allocated resource_req
 *
 */
resource_req *find_alloc_resource_req(char *name, resource_req *reqlist)
  {
  resource_req *resreq;  /* used to find or create resource_req */
  resource_req *prev = NULL; /* previous resource_req in list */

  resreq = reqlist;

  while (resreq != NULL && strcmp(resreq -> name, name))
    {
    prev = resreq;
    resreq = resreq -> next;
    }

  if (resreq == NULL)
    {
    if ((resreq = new_resource_req()) == NULL)
      return NULL;

    retnull_on_null(resreq -> name = strdup(name));

    if (prev != NULL)
      prev -> next = resreq;
    }

  return resreq;
  }

/*
 *
 * find_resource_req - find a resource_req from a resource_req list
 *
 *   reqlist - the resource_req list
 *   name - resource to look for
 *
 * returns found resource_req or NULL
 *
 */
resource_req *find_resource_req(resource_req *reqlist, const char *name)
  {
  resource_req *resreq;

  resreq = reqlist;

  while (resreq != NULL && strcmp(resreq -> name, name))
    resreq = resreq -> next;

  return resreq;
  }


/** Clone one resource req structure
 *
 * @param resource Source for the clone
 * @returns Valid pointer to cloned resource on success, NULL otherwise
 */
static resource_req *clone_resource_req(resource_req *resource)
  {
  resource_req *resreq;
  if ((resreq = new_resource_req()) == NULL)
    return NULL;

  resreq->amount = resource->amount;

  if (resource->name != NULL)
    {
    if ((resreq->name = strdup(resource->name)) == NULL)
      {/* cleanup on fail */
      free(resreq);
      return NULL;
      }
    }
  else
    resreq->name = NULL;

  if (resource->res_str != NULL)
    {
    if ((resreq->res_str = strdup(resource->res_str)) == NULL)
      {
      free(resreq->name);
      free(resreq);
      return NULL;
      }
    }
  else
    resreq->res_str = NULL;

  resreq->next = NULL;

  return resreq;
  }

/** Clone resource requirements (list)
 *
 * @param list Clone source
 * @returns Valid pointer to the beginning of the list on success, NULL otherwise
 */
resource_req *clone_resource_req_list(resource_req *list)
  {
  resource_req* iter, *newhead, *newiter;

  if (list == NULL)
    return NULL;

  /* init the crawl */
  newhead = clone_resource_req(list);
  newiter = newhead;
  iter = list->next;

  while (iter != NULL)
    {
    newiter->next = clone_resource_req(iter);
    newiter = newiter->next;
    iter = iter->next;
    }

  return newhead;
  }

/*
 *
 * free_resource_req_list - frees memory used by a resource_req list
 *
 *   list - resource_req list
 *
 * returns nothing
 */
void free_resource_req_list(resource_req *list)
  {
  resource_req *resreq, *tmp;

  resreq = list;

  while (resreq != NULL)
    {
    tmp = resreq;
    resreq = resreq -> next;

    free(tmp -> name);
    free(tmp -> res_str);
    free(tmp);
    }
  }

/*
 *
 * set_state - set the state flag in a job_info structure
 *   i.e. the is_* bit
 *
 *   state - the state
 *   jinfo - the job info structure
 *
 * returns nothing
 *
 */
void set_state(char *state, JobInfo *jinfo)
  {
  switch (state[0])
    {

    case 'Q':
      jinfo -> state = JobQueued;
      break;

    case 'R':
      jinfo -> state = JobRunning;
      break;

    case 'T':
      jinfo -> state = JobTransit;
      break;

    case 'H':
      jinfo -> state = JobHeld;
      break;

    case 'W':
      jinfo -> state = JobWaiting;
      break;

    case 'E':
      jinfo -> state = JobExiting;
      break;

    case 'S':
      jinfo -> state = JobSuspended;
      break;

    case 'C':
      jinfo -> state = JobCompleted;
      break;

    case 'X':
      jinfo -> state = JobCrossRun;
    }
  }

/*
 *
 * update_job_on_run - update job information kept in a job_info
 *    when a job is run
 *
 *   pbs_sd - socket connection to pbs_server
 *   jinfo - the job to update
 *
 * returns nothing
 *
 */
void update_job_on_run(int UNUSED(pbs_sd), JobInfo *jinfo)
  {
  jinfo -> state = JobRunning;
  }

/** Update the job information
 *
 * Update the job information kept in a job_info
 * when job is moved from the server.
 *
 * @param jinfo Job info to be modified
 */
void update_job_on_move(JobInfo *jinfo)
  {
  jinfo -> state = JobCrossRun;
  }

/*
 *
 * update_job_comment - update a jobs comment attribute
 *
 *   pbs_sd - connection to the pbs_server
 *   jinfo  - job to update
 *   comment - the comment string
 *
 * returns
 *   0: comment needed updating
 *   non-zero: the comment did not need to be updated (same as before etc)
 *
 */
int update_job_comment(int pbs_sd, JobInfo *jinfo, const char *comment)
  {
  /* the pbs_alterjob() call takes a linked list of attrl structures to alter
   * a job.  All we are interested in doing is changing the comment.
   */

  struct attrl attr =
    {
    NULL, const_cast<char*>(ATTR_comment), NULL, NULL, SET
    };

  if (jinfo == NULL)
    return 1;

  /* no need to update the job comment if it is the same */
  if (jinfo->comment != comment)
    {
    jinfo->comment = comment;

    attr.value = const_cast<char*>(comment);

    pbs_alterjob(pbs_sd, const_cast<char*>(jinfo -> job_id.c_str()), &attr, NULL);

    return 0;
    }

  return 1;
  }

int update_job_planned_nodes(int pbs_sd, JobInfo *jinfo, const std::string& nodes)
  {
  struct attrl attr =
    {
    NULL, const_cast<char*>(ATTR_planned_nodes), NULL, NULL, SET
    };

  if (jinfo == NULL)
    return 1;

  if (jinfo->p_planned_nodes != nodes)
    {
    attr.value = const_cast<char*>(nodes.c_str());

    return pbs_alterjob(pbs_sd, const_cast<char*>(jinfo -> job_id.c_str()), &attr, NULL);
    }

  return 0;
  }

int update_job_waiting_for(int pbs_sd, JobInfo *jinfo, const std::string& waiting)
  {
  struct attrl attr =
    {
    NULL, const_cast<char*>(ATTR_waiting_for), NULL, NULL, SET
    };

  if (jinfo == NULL)
    return 1;

  if (jinfo->p_waiting_for != waiting)
    {
    attr.value = const_cast<char*>(waiting.c_str());

    return pbs_alterjob(pbs_sd, const_cast<char*>(jinfo -> job_id.c_str()), &attr, NULL);
    }

  return 0;
  }

int update_job_earliest_start(int pbs_sd, JobInfo *jinfo, time_t earliest_start)
  {
  struct attrl attr =
    {
    NULL, const_cast<char*>(ATTR_planned_start), NULL, NULL, SET
    };

  if (jinfo == NULL)
    return 1;

  if (jinfo->p_planned_start != earliest_start)
    {
    char value[80] = { 0 };

    sprintf(value,"%ld",earliest_start);
    attr.value = value;

    return pbs_alterjob(pbs_sd, const_cast<char*>(jinfo -> job_id.c_str()), &attr, NULL);
    }

  return 0;
  }

int update_job_fairshare(int pbs_sd, JobInfo *jinfo, double fairshare)
  {
  /* the pbs_alterjob() call takes a linked list of attrl structures to alter
   * a job.  All we are interested in doing is changing the comment.
   */

  struct attrl attr =
    {
    NULL, const_cast<char*>(ATTR_fairshare_cost), NULL, NULL, SET
    };

  if (jinfo == NULL)
    return 1;

  char value[80] = { 0 };

  sprintf(value,"%.3f",fairshare);
  attr.value = value;

  pbs_alterjob(pbs_sd, const_cast<char*>(jinfo -> job_id.c_str()), &attr, NULL);

  return 0;
  }

/*
 *
 * update_jobs_cant_run - update an array of jobs which can not run
 *
 *   pbs_sd - connection to the PBS server
 *   jinfo_arr - the array to update
 *   start - the job which couldn't run
 *   comment - the comment to update
 *
 * returns nothing;
 *
 */
void update_jobs_cant_run(int pbs_sd, vector<JobInfo*>& jinfo_arr, JobInfo *start,
                          const char *comment, int start_where)
  {
  size_t i = 0;
  /* We are not starting at the front of the array, so we need to find the
   * element to start with.
   */
  if (start != NULL)
    {
    for (; i < jinfo_arr.size() && jinfo_arr[i] != start; i++);
    }
  else
    i = 0;

  if (i < jinfo_arr.size())
    {
    if (start_where == START_BEFORE_JOB)
      i--;
    else if (start_where == START_AFTER_JOB)
      i++;

    for (; i < jinfo_arr.size(); i++)
      {
      jinfo_arr[i] -> can_not_run = true;
      update_job_comment(pbs_sd, jinfo_arr[i], comment);
      }
    }
  }

/*
 *
 * translate_job_fail_code - translate the failure code of
 *    is_ok_to_run_job into a comment and log message
 *
 *   fail_code        - return code from is_ok_to_run_job()
 *   OUT: comment_msg - translated comment
 *   OUT: log_msg     - translated log message
 *
 * returns
 *    1: comment and log messages were set
 *    0: comment and log messages were not set
 *
 */
int translate_job_fail_code(int fail_code, char *comment_msg, char *log_msg)
  {
  int rc = 1;

  if (fail_code < 1000)
    {
    //strcpy(comment_msg, res_to_check[fail_code].comment_msg);
    //strcpy(log_msg, res_to_check[fail_code].debug_msg);
    throw runtime_error("Unexpected code path.");
    }
  else
    {
    switch (fail_code)
      {

      case QUEUE_JOB_LIMIT_REACHED:
        strcpy(comment_msg, COMMENT_QUEUE_JOB_LIMIT);
        strcpy(log_msg, INFO_QUEUE_JOB_LIMIT);
        break;

      case SERVER_JOB_LIMIT_REACHED:
        strcpy(comment_msg, COMMENT_SERVER_JOB_LIMIT);
        strcpy(log_msg, INFO_SERVER_JOB_LIMIT);
        break;

      case SERVER_USER_LIMIT_REACHED:
        strcpy(comment_msg, COMMENT_SERVER_USER_LIMIT);
        strcpy(log_msg, INFO_SERVER_USER_LIMIT);
        break;

      case QUEUE_USER_LIMIT_REACHED:
        strcpy(comment_msg, COMMENT_QUEUE_USER_LIMIT);
        strcpy(log_msg, INFO_QUEUE_USER_LIMIT);
        break;

      case QUEUE_GROUP_LIMIT_REACHED:
        strcpy(comment_msg, COMMENT_QUEUE_GROUP_LIMIT);
        strcpy(log_msg, INFO_QUEUE_GROUP_LIMIT);
        break;

      case SERVER_GROUP_LIMIT_REACHED:
        strcpy(comment_msg, COMMENT_SERVER_GROUP_LIMIT);
        strcpy(log_msg, INFO_SERVER_GROUP_LIMIT);
        break;

      case CROSS_DED_TIME_BOUNDRY:
        strcpy(comment_msg, COMMENT_CROSS_DED_TIME);
        strcpy(log_msg, INFO_CROSS_DED_TIME);
        break;

      case NO_AVAILABLE_NODE:
        strcpy(comment_msg, COMMENT_NO_AVAILABLE_NODE);
        strcpy(log_msg, INFO_NO_AVAILABLE_NODE);
        break;

      case NOT_QUEUED:
        /* do we really care if the job is not in a queued state?  there is
         * no way it'll be run in this state, why spam the log about it
         */
        log_msg[0] = '\0';
        comment_msg[0] = '\0';
        rc = 0;
        break;

      case NOT_ENOUGH_NODES_AVAIL:
        strcpy(comment_msg, COMMENT_NOT_ENOUGH_NODES_AVAIL);
        strcpy(log_msg, INFO_NOT_ENOUGH_NODES_AVAIL);
        break;

      case JOB_STARVING:
        strcpy(comment_msg, COMMENT_JOB_STARVING);
        sprintf(log_msg, INFO_JOB_STARVING, cstat.starving_job -> job_id.c_str());
        break;

      case SCHD_ERROR:
        strcpy(comment_msg, COMMENT_SCHD_ERROR);
        strcpy(log_msg, INFO_SCHD_ERROR);
        break;

      case SERVER_TOKEN_UTILIZATION:
        strcpy(comment_msg, COMMENT_TOKEN_UTILIZATION);
        sprintf(log_msg, INFO_TOKEN_UTILIZATION);
        break;

      case JOB_FAILED_MOVE:
        strcpy(comment_msg, COMMENT_JOB_COULDNT_MOVE);
        sprintf(log_msg, INFO_JOB_COULDNT_MOVE);
        break;

      case NODESPEC_NOT_ENOUGH_NODES_TOTAL:
        strcpy(comment_msg, COMMENT_NODESPEC_TOTAL);
        sprintf(log_msg, INFO_NODESPEC_TOTAL);
        break;

      case NODESPEC_NOT_ENOUGH_NODES_INTERSECT:
        strcpy(comment_msg, COMMENT_NODESPEC_INTERSECT);
        sprintf(log_msg, INFO_NODESPEC_INTERSECT);
        break;

      case CLUSTER_RUNNING:
        strcpy(comment_msg, COMMENT_CLUSTER_RUNNING);
        sprintf(log_msg, INFO_CLUSTER_RUNNING);
        break;

      case CLUSTER_PERMISSIONS:
        strcpy(comment_msg, COMMENT_CLUSTER_PERMISSIONS);
        sprintf(log_msg, INFO_CLUSTER_PERMISSIONS);
        break;

      case INSUFICIENT_SERVER_RESOURCE:
        strcpy(comment_msg, COMMENT_INSUFICIENT_SERVER_RESOURCE);
        sprintf(log_msg, INFO_INSUFICIENT_SERVER_RESOURCE);
        break;

      case INSUFICIENT_QUEUE_RESOURCE:
        strcpy(comment_msg, COMMENT_INSUFICIENT_QUEUE_RESOURCE);
        sprintf(log_msg, INFO_INSUFICIENT_QUEUE_RESOURCE);
        break;

      case INSUFICIENT_DYNAMIC_RESOURCE:
        strcpy(comment_msg, COMMENT_INSUFICIENT_DYNAMIC_RESOURCE);
        sprintf(log_msg, INFO_INSUFICIENT_DYNAMIC_RESOURCE);
        break;

      case NODE_STILL_BOOTING:
        strcpy(comment_msg, COMMENT_NODE_STILL_BOOTING);
        sprintf(log_msg, INFO_NODE_STILL_BOOTING);
        break;

      case QUEUE_PROC_LIMIT_REACHED:
        strcpy(comment_msg, COMMENT_QUEUE_PROC_LIMIT_REACHED);
        sprintf(log_msg, INFO_QUEUE_PROC_LIMIT_REACHED);
        break;

      case QUEUE_USER_PROC_LIMIT_REACHED:
        strcpy(comment_msg, COMMENT_QUEUE_USER_PROC_LIMIT_REACHED);
        sprintf(log_msg, INFO_QUEUE_USER_PROC_LIMIT_REACHED);
        break;

      case QUEUE_GROUP_PROC_LIMIT_REACHED:
        strcpy(comment_msg, COMMENT_QUEUE_GROUP_PROC_LIMIT_REACHED);
        sprintf(log_msg, INFO_QUEUE_GROUP_PROC_LIMIT_REACHED);
        break;

      case REQUEST_NOT_MATCHED:
        strcpy(comment_msg, COMMENT_REQUEST_NOT_MATCHED);
        sprintf(log_msg,INFO_REQUEST_NOT_MATCHED);
        break;

      case SCHEDULER_LOOP_RUN_LIMIT_REACHED:
        strcpy(comment_msg, COMMENT_SCHEDULER_LOOP_RUN_LIMIT_REACHED);
        sprintf(log_msg, INFO_SCHEDULER_LOOP_RUN_LIMIT_REACHED);
        break;

      case UNKNOWN_LOCATION_PROPERTY_REQUEST:
        strcpy(comment_msg, COMMENT_UNKNOWN_LOCATION_PROPERTY_REQUEST);
        sprintf(log_msg, INFO_UNKNOWN_LOCATION_PROPERTY_REQUEST);
        break;

      case JOB_SCHEDULED:
        strcpy(comment_msg, COMMENT_JOB_SCHEDULED);
        sprintf(log_msg, INFO_JOB_SCHEDULED);
        break;

      default:
        rc = 0;
        comment_msg[0] = '\0';
        log_msg[0] = '\0';
        break;
      }
    }

  return rc;
  }

/*
 *
 * update_job_planned_start - update a jobs planned_start attribute
 *
 *   pbs_sd - connection to the pbs_server
 *   jinfo  - job to update
 *   comment - the comment string
 *
 * returns
 *   0: comment needed updating
 *   non-zero: the comment did not need to be updated (same as before etc)
 *
 */
int update_job_planned_start(int pbs_sd, plan_job *job)
  {
  /* the pbs_alterjob() call takes a linked list of attrl structures to alter
   * a job.  All we are interested in doing is changing the comment.
   */

  JobInfo* jinfo;

  jinfo = job->jinfo;

  struct attrl attr =
    {
    NULL, (char*)ATTR_planned_start, NULL, NULL, SET
    };

  if (jinfo == NULL)
    return 1;

  /* no need to update the job comment if it is the same */
  if (jinfo -> p_planned_start != job->start_time)
    {
   // if (jinfo -> planned_start != NULL)
     // free(jinfo -> planned_start);

    char buffer[100];

    if (job->start_time > 946681200)
      sprintf(buffer, "%ld", job->start_time);
    else {
      sprintf(buffer, "%ld", 0);
      attr.op = UNSET;
    }

    attr.value = buffer;

    job->jinfo->p_planned_start = job->start_time;

    pbs_alterjob(pbs_sd, (char*)jinfo -> job_id.c_str(), &attr, NULL);

    return 0;
    }

  return 1;
  }
