/*Copyright 2012 Václav Chlumský
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data_types.h"
#include "job_info.h"
#include "misc.h"

#include "plan_data_types.h"
#include "plan_list_operations.h"

#ifndef max
	#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif

struct user_info
  {
  char *account;

  long int completed_cputime;

  long int completed_waittime;

  int num_completed_jobs;

  long int planned_cputime;

  long int planned_waittime;

  int num_planned_jobs;

  long double completed_Fu;

  long double planned_Fu;

  double normalized_user_waittime;

  double normalized_max_value;

  };

struct fairshare_list
  {
  user_info** uinfo;

  int num_users;
  };

fairshare_list fslist;

int fairshare_add_user(char *account)
  {
  if (fslist.num_users == 0)
    {
	if ((fslist.uinfo = (user_info**)malloc(sizeof(user_info*))) == NULL)
	  {
	  perror("Memory Allocation Error");
	  return -1;
	  }
    } else
    {
      if ((fslist.uinfo = (user_info**)realloc(fslist.uinfo, (fslist.num_users + 1) * sizeof(user_info*))) == NULL)
    	{
    	perror("Memory Allocation Error");
    	return -1;
    	}
    }

  if ((fslist.uinfo[fslist.num_users] = (user_info*)malloc(sizeof(user_info))) == NULL)
	{
	perror("Memory Allocation Error");
	return -1;
	}

  fslist.uinfo[fslist.num_users] -> account = strdup(account);

  fslist.uinfo[fslist.num_users] -> completed_cputime = 0;
  fslist.uinfo[fslist.num_users] -> completed_waittime = 0;
  fslist.uinfo[fslist.num_users] -> num_completed_jobs = 0;
  fslist.uinfo[fslist.num_users] -> planned_cputime = 0;
  fslist.uinfo[fslist.num_users] -> planned_waittime = 0;
  fslist.uinfo[fslist.num_users] -> num_planned_jobs = 0;
  fslist.uinfo[fslist.num_users] -> completed_Fu = 0;
  fslist.uinfo[fslist.num_users] -> planned_Fu = 0;
  fslist.uinfo[fslist.num_users] -> normalized_max_value = 0;

  return fslist.num_users++;
  }


void fairshare_add_completed_values(plan_job *job, long int completed_cputime, long int completed_waittime, int num_running_cpus, long int available_ram)
  {
  int user_found = 0;
  int user_id;
  char *account = (char*)job -> jinfo -> account.c_str();

  for (user_id = 0; user_id < fslist.num_users; user_id++)
	  if (strcmp(fslist.uinfo[user_id] -> account, account) == 0)
	    {
	    user_found = 1;
	    break;
	    }

  if (!user_found)
    user_id = fairshare_add_user(account);

  if (user_id > -1 && num_running_cpus > 0 && available_ram > 0) {
    double res1 = (job -> req_ppn * job -> req_num_nodes * 1.0)/(num_running_cpus * 1.0);
    double res2 = (job->req_mem * job -> req_num_nodes * 1.0)/(available_ram * 1.0);
    double Pj = max(res1, res2);
    Pj = Pj * 1.0 * num_running_cpus * 1.0;
    fslist.uinfo[user_id] -> completed_Fu += Pj * (job -> jinfo ->compl_time - job -> start_time) * 1.0;
    fslist.uinfo[user_id] -> completed_cputime += completed_cputime;
    fslist.uinfo[user_id] -> completed_waittime += completed_waittime;
    fslist.uinfo[user_id] -> num_completed_jobs ++;
    }
  }


void fairshare_add_planned_values(plan_job *job, long int planned_waittime, long int planned_cputime, int num_running_cpus, long int available_ram)
  {
  int user_found = 0;
  int user_id;
  char *account = (char*)job -> jinfo -> account.c_str();

  for (user_id = 0; user_id < fslist.num_users; user_id++)
	  if (strcmp(fslist.uinfo[user_id] -> account, account) == 0)
	    {
	    user_found = 1;
	    break;
	    }

  if (!user_found)
    user_id = fairshare_add_user(account);

  if (user_id > -1 && num_running_cpus > 0 && available_ram > 0) {
    double res1 = (job -> req_ppn * job -> req_num_nodes * 1.0)/(num_running_cpus * 1.0);
    double res2 = (job->req_mem * job -> req_num_nodes * 1.0)/(available_ram * 1.0);
    double Pj = max(res1, res2);
    Pj = Pj * 1.0 * num_running_cpus * 1.0;
    fslist.uinfo[user_id] -> planned_Fu += Pj * job->estimated_processing * 1.0;
    fslist.uinfo[user_id] -> planned_cputime += planned_cputime;
    fslist.uinfo[user_id] -> planned_waittime += planned_waittime;
    fslist.uinfo[user_id] -> num_planned_jobs ++;
    }
  }


void fairshare_add_job(plan_job *job, int num_running_cpus, long int available_ram)
  {
  JobInfo *jinfo = job -> jinfo;

  long int completed_cputime = 0;
  long int completed_waittime = 0;
  long int planned_cputime = 0;
  long int planned_waittime = 0;

  if (jinfo -> state == JobCompleted)
    {
	resource_req *used;
	used = find_resource_req(jinfo -> resused, "walltime");

	if (used != NULL)
		completed_cputime = used -> amount * job -> req_ppn * job -> req_num_nodes;
	completed_waittime = job -> start_time - job -> jinfo -> qtime;

	fairshare_add_completed_values(job, completed_cputime, completed_waittime, num_running_cpus, available_ram);
    } else
    {
    planned_cputime = job -> estimated_processing * job -> req_ppn * job -> req_num_nodes;
    planned_waittime = job -> start_time - job -> jinfo -> qtime;

    fairshare_add_planned_values(job, planned_waittime, planned_cputime, num_running_cpus, available_ram);
    }
  }

void fairshare_reset_planned_values()
  {
  for (int i = 0 ; i < fslist.num_users; i++)
    {
	fslist.uinfo[i] -> planned_cputime = 0;
	fslist.uinfo[i] -> planned_waittime = 0;
	fslist.uinfo[i] -> planned_Fu = 0;
	fslist.uinfo[i] -> num_planned_jobs = 0;
    }
  }

void fairshare_get_planned_from_schedule(sched *schedule)
  {
  plan_job *job;

  fairshare_reset_planned_values();

  for (int cluster = 0; cluster < schedule->num_clusters; cluster++)
    {
    schedule -> jobs[cluster] -> current = NULL;
    while (list_get_next(schedule -> jobs[cluster]) != NULL)
      {
  	  job = (plan_job*)schedule -> jobs[cluster] -> current;
  	  fairshare_add_job(job, schedule -> clusters[cluster] -> num_running_cpus, schedule -> clusters[cluster] -> available_ram);
      }
    }
  }

double fairshare_result_common()
  {
  double res = 0;
  double uwt = 0;

  for (int user = 0; user < fslist.num_users; user++)
    {
	fslist.uinfo[user]->normalized_user_waittime =
			  ((fslist.uinfo[user]->completed_waittime + fslist.uinfo[user]->planned_waittime)*1.0) /
			  ((fslist.uinfo[user]->completed_cputime + fslist.uinfo[user]->planned_cputime)*1.0);

	uwt += fslist.uinfo[user] -> normalized_user_waittime;
    }

  uwt = uwt / (fslist.num_users*1.0);

  for (int user = 0; user < fslist.num_users; user++)
	  res += (1.0 + uwt - fslist.uinfo[user]->normalized_user_waittime*1.0)*
	  	  	 (1.0 + uwt - fslist.uinfo[user]->normalized_user_waittime*1.0);

  return res;
  }

double fairshare_result_max()
  {
  long double res = 0;
  long double total_user_wt = 0;
  long double uwt = 0;

  for (int user = 0; user < fslist.num_users; user++) {
    total_user_wt = (fslist.uinfo[user]->completed_waittime + fslist.uinfo[user]->planned_waittime) * 1.0;
    fslist.uinfo[user] -> normalized_max_value = total_user_wt/(fslist.uinfo[user]->completed_Fu + fslist.uinfo[user]->planned_Fu);
    uwt += fslist.uinfo[user] -> normalized_max_value;
    }

  uwt = uwt / (fslist.num_users*1.0);

  for (int user = 0; user < fslist.num_users; user++)
	  res += (1.0 + uwt - fslist.uinfo[user]->normalized_max_value*1.0)*
	  	  	 (1.0 + uwt - fslist.uinfo[user]->normalized_max_value*1.0);

  return res;
  }

void fairshare_log()
  {
  for (int user = 0; user < fslist.num_users; user++)
	sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "fairshare", "user: %s, number of completed jobs: %d, cFu: %f, pFu: %f, NUMAXo: %f",
			fslist.uinfo[user] -> account,
			fslist.uinfo[user] -> num_completed_jobs,
			fslist.uinfo[user] -> completed_Fu,
			fslist.uinfo[user] -> planned_Fu,
			fslist.uinfo[user] -> normalized_max_value);
  }


