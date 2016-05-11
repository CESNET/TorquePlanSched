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
#include "job_info.h"
#include "constant.h"
#include "fairshare.h"
#include "globals.h"
#include "misc.h"
#include "constant.h"
#include "config.h"
#include "utility.h"
#include "api.hpp"

#include <string>
#include <stdexcept>
#include <sstream>
using namespace std;
using namespace Scheduler;
using namespace Logic;

/* multiple trees support */

map<string,FairshareTree> fairshare_trees;

FairshareTree& get_tree(const string& tree)
  {
  map<string,FairshareTree>::iterator i = fairshare_trees.find(tree);
  if (i != fairshare_trees.end())
    return i->second;

  pair<map<string,FairshareTree>::iterator,bool> it = fairshare_trees.insert(make_pair(tree,FairshareTree(tree)));

  return it.first->second;
  }

void free_fairshare_trees()
  {
  fairshare_trees.clear();
  }

void dump_all_fairshare()
  {
  map<string,FairshareTree>::iterator it = fairshare_trees.begin();
  while (it != fairshare_trees.end())
    {
    it->second.dump_to_cache();
    ++it;
    }
  }

int init_fairshare()
  {
  fairshare_trees.insert(make_pair(string("default"),FairshareTree("default")));
  return 0;
  }

void write_usages()
  {
  map<string,FairshareTree>::iterator it = fairshare_trees.begin();
  while (it != fairshare_trees.end())
    {
    it->second.dump_to_file();
    it->second.dump_to_ini();
    ++it;
    }
  }

void decay_fairshare_trees()
  {
  map<string,FairshareTree>::iterator it = fairshare_trees.begin();
  while (it != fairshare_trees.end())
    {
    it->second.decay();
    ++it;
    }
  }

/*
 *
 * update_usage_on_run - update a users usage information when a
 *         job is run
 *
 *   jinfo - the job which just started running
 *
 * returns nothing
 *
 */
void update_usage_on_run(JobInfo *jinfo) // TODO zaintegrovat mem
  {
  resource_req *tmp = find_resource_req(jinfo->resreq, "procs");
  jinfo -> ginfo -> temp_usage += calculate_usage_value(jinfo -> resreq)*tmp->amount;
  }


JobInfo *extract_fairshare(server_info *sinfo)
  {
  JobInfo *max = nullptr;
  int max_priority = -1;
  double max_value = -1;
  double curr_value = -1;

  for (auto user : sinfo->jobs_by_owner)
    {
    // first remove all jobs that cannot run, or are already running
    while (user.second.size() > 0 &&
           ((!user.second.front()->suitable_for_run()) ||
            (user.second.front()->state == JobRunning)))
      user.second.pop_front();

    if (user.second.size() == 0)
      continue;

    // if the highest priority job of this user does not have high enough priority
    // we can safely skip the entire user
    if (user.second.front()->queue->priority < max_priority)
      continue;

    // if we are upgrading queue priority, we need to reset fairshare
    if (user.second.front()->queue->priority > max_priority)
      {
      max_value = -1;
      max_priority = user.second.front()->queue->priority;
      }

    curr_value = user.second.front()->ginfo->percentage / user.second.front()->ginfo->temp_usage;

    if (max_value < curr_value)
      {
      max = user.second.front();
      max_value = curr_value;
      }
    }

  return max;
  }

JobInfo *extract_fairshare(const std::vector<JobInfo*>& jobs)
  {
  JobInfo *max = nullptr; /* job with the max shares / percentage */
  float max_value = -1;   /* max shares / percentage */
  int max_priority = -1;
  float cur_value;        /* the current shares / percentage value */

  for (auto j : jobs)
    {
    if (!j->suitable_for_run() || j->state == JobRunning) continue;

    if (conf.priority_fairshare)
      {
      if (j->queue->priority < max_priority) continue;

      // reset max fairshare when queue priority increased
      if (j->queue->priority > max_priority)
        {
        max_value = -1;
        }

      max_priority = j->queue->priority;
      }

    cur_value = j -> ginfo -> percentage / j -> ginfo -> temp_usage;

    if (max_value < cur_value)
      {
      max = j;
      max_value = cur_value;
      }
    }

  return max;
  }


/*
 *
 * calculate_usage_value - calcualte a value that represents the usage
 *    information
 *
 *   resreq - a resource_req list that holds the resource info
 *
 * returns the calculated value
 *
 * NOTE: currently it will only return the number of cpu seconds used.
 *       This function can be more complicated
 *
 */
usage_t calculate_usage_value(resource_req *resreq)
  {
  resource_req *tmp;

  if (resreq != NULL)
    {
    tmp = find_resource_req(resreq, "walltime");

    if (tmp != NULL)
      return tmp -> amount;
    }

  return 0L;
  }

void dump_all_fairshare_remote(char* fqdn)
  {
  map<string,FairshareTree>::iterator it = fairshare_trees.begin();
  while (it != fairshare_trees.end())
    {
    it->second.dump_to_cache(fqdn);
     ++it;
     }
   }

 /*
  *
 * compare_fairshare - extract the first job from the user with the
 *       least percentage / usage ratio
 *
 *   jobs - array of jobs to search through
 *
 * return the found job or NULL on error
 *
 */

bool compare_fairshare(JobInfo *job_i, JobInfo *job_j)
  {
  float cur_value_i;
  float cur_value_j;

  if (job_i != NULL && job_j != NULL)
    {

	if (job_i->state == JobRunning && job_j->state != JobRunning)
		return true;

	if (job_i->state != JobRunning && job_j->state == JobRunning)
		return false;

	if (job_i->state == JobRunning && job_j->state == JobRunning) {
		if (atoi(job_i->job_id.c_str()) < atoi(job_j->job_id.c_str())) {
			return true;
                }
		else {
			return false;
                }
        }

	if (job_i->state == JobQueued && job_j->state != JobQueued)
		return true;

	if (job_i->state != JobQueued && job_j->state == JobQueued)
		return false;

	if (job_i->state != JobQueued && job_j->state != JobQueued){
		if (atoi(job_i->job_id.c_str()) < atoi(job_j->job_id.c_str())) {
			return true;
                }
		else {
			return false;
                }
        }

	cur_value_i = job_i -> ginfo -> percentage / job_i -> ginfo -> temp_usage;
	cur_value_j = job_j -> ginfo -> percentage / job_j -> ginfo -> temp_usage;

	if (cur_value_i > cur_value_j)
		return true;

	if (job_i->job_id.c_str()[0] == ' ')
		sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "ERROR", "job name %s - first char space!!!!!!!!!! call Simon:-)", job_i->job_id.c_str());

	if (job_j->job_id.c_str()[0] == ' ')
		sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "ERROR", "job name %s - first char space!!!!!!!!!! call Simon:-)", job_j->job_id.c_str());

	if (cur_value_i == cur_value_j)
	  if (atoi(job_i->job_id.c_str()) < atoi(job_j->job_id.c_str()))
		  return true;
    }

  return false;
  }
