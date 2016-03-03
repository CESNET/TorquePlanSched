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
#include <limits.h>
#include "misc.h"

#include <fstream>
#include "plan_list_operations.h"
#include "plan_limits.h"

using namespace std;

int num_grap_out=0;

void print_resource_val(resource_req* res)
  {
  if (res != NULL)
    {
	fprintf(stderr, "%s %ld\n", res -> name, res -> amount );
	print_resource_val(res -> next);
    }
  }

void print_resource_str(resource_req* res)
  {
  if (res != NULL)
    {
	fprintf(stderr, "%s %s\n", res -> name, res -> res_str);
	print_resource_str(res -> next);
    }
  }

char* prop_to_string(char** sarray, int num_prop)
  {
  char *joined;

  if ((joined = (char*) malloc(sizeof(char))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  strcpy(joined,"");
  if (sarray != NULL)
    for (int i = 0; i < num_prop; i++)
      {
      if ((joined = (char*) realloc(joined, (strlen(joined) + strlen(sarray[i]) + 2) * sizeof(char))) == NULL)
        {
        perror("Memory Allocation Error");
        return NULL;
        }

      joined = strcat(joined, sarray[i]);
      joined = strcat(joined, " ");
	   }

  return joined;
  }

void log_job(plan_job* job)
 {
 char buffer_start[300];
 char buffer_end[300];
 char buffer_nodes[5000];

 strftime(buffer_start, 30, "%Y:%m:%d %H:%M:%S", localtime(&job -> start_time));
 strftime(buffer_end, 30, "%Y:%m:%d %H:%M:%S", localtime(&job -> completion_time));

 if (job -> jinfo -> state == JobRunning || job -> jinfo -> state == JobExiting)
   {
   strcpy(buffer_nodes, "");

   for (int i = 0; i < job -> usage ;i++)
     sprintf(buffer_nodes, "%s %d", buffer_nodes, job -> cpu_indexes[i]);

   sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "running job", "id: %d usage: %d req_nodes: %d req_ppn: %d estim_proces: %ld start_time: %s completion_time: %s is running on: %s",
		   job -> job_id,
		   job -> usage,
		   job -> req_num_nodes,
		   job -> req_ppn,
		   job -> estimated_processing,
		   buffer_start,
		   buffer_end,
		   buffer_nodes
		   );
   } else
   {
   strcpy(buffer_nodes, "");

   if (job -> ninfo_arr != NULL)
	 {
	 sprintf(buffer_nodes, "nodes:");

	 for (int i = 0; i < job -> req_num_nodes; i++)
	   sprintf(buffer_nodes, "%s %s", buffer_nodes, job -> ninfo_arr[i] -> get_name());

	 sprintf(buffer_nodes, "%s, planned on:", buffer_nodes);

	 for (int i = 0; i < job -> usage; i++)
	   sprintf(buffer_nodes, "%s %d", buffer_nodes, job -> cpu_indexes[i]);

     }

   sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "queued job", "id: %d usage: %d req_nodes: %d req_ppn: %d estim_proces: %ld start_time: %s completion_time: %s %s",
		   job -> job_id,
    	   job -> usage,
    	   job -> req_num_nodes,
    	   job -> req_ppn,
    	   job -> estimated_processing,
    	   buffer_start,
    	   buffer_end,
    	   buffer_nodes
    	   );
   }
 }

void log_gap(plan_gap* gap)
  {
  char buffer_start[300];
  char buffer_end[300];
  char buffer_following[5000];

  strcpy(buffer_following, "");

  strftime(buffer_start, 30, "%Y:%m:%d %H:%M:%S", localtime(&gap -> start_time));

  if (gap -> end_time < LONG_MAX)
    strftime(buffer_end, 30, "%Y:%m:%d %H:%M:%S", localtime(&gap -> end_time));

  if (gap -> end_time == LONG_MAX)
	strcpy(buffer_end, "infim");

  if (gap -> following_job !=NULL)
	sprintf(buffer_following, ", following job: %d", gap -> following_job -> job_id);

  if (gap -> end_time < LONG_MAX)
  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "gap", "total usage: %d duration: %ld start_time: %s end_time: %s%s",
		  gap -> usage,
		  gap -> duration,
		  buffer_start,
		  buffer_end,
		  buffer_following);

  if (gap -> end_time == LONG_MAX)
  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "gap", "total usage: %d duration: infim start_time: %s end_time: %s%s",
		  gap -> usage,
		  buffer_start,
		  buffer_end,
		  buffer_following);

  for (int i = 0; i < gap -> num_nodes; i++)
	  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "gap", "      node: %s, usage: %d, mem: %ld, scratch_local: %ld",
			gap -> nodes_memory[i] -> ninfo->get_name(),
			gap -> nodes_memory[i] -> num_cpus,
			gap -> nodes_memory[i] -> available_mem,
			gap -> nodes_memory[i] -> available_scratch_local);
 }

void log_limit(plan_limit* limit)
  {
  char buffer_timestamp[300];

  char buffer_cpus[5000];

  strftime(buffer_timestamp, 30, "%Y:%m:%d %H:%M:%S", localtime(&limit -> timestamp));

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "limit", "num accounts : %d timestamp: %s",
		  limit -> num_accounts,
		  buffer_timestamp);

  for (int i = 0; i < limit -> num_accounts; i++)
      {
	  strcpy(buffer_cpus, "");

	  for (int j = 0; j < 8; j++)
	  	  sprintf(buffer_cpus, "%s %d", buffer_cpus, limit->accounts[i]->num_cpus[j]);

	  /*sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "limit", "      account: %s, cpus: %s",
			limit->accounts[i]->account,
			buffer_cpus);*/
      }
 }


void log_list(plan_list* list)
 {
 if (list -> type == Jobs)
   sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "Jobs list", "number of jobs: %d", list -> num_items);

 if (list -> type == Gaps)
   sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "Gaps list", "number of gaps: %d", list -> num_items);

 if (list -> type == Limits)
   sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "Limits list", "number of limits: %d", list -> num_items);

 list -> current = NULL;
 while (list_get_next(list) != NULL)
   {
   if (list -> type == Jobs)
	 log_job((plan_job*) list -> current);

   if (list -> type == Gaps)
	 log_gap((plan_gap*) list -> current);

   if (list -> type == Limits)
	 log_limit((plan_limit*) list -> current);
   }
 }

void log_schedule(sched* sched)
 {
 for (int i = 0; i < sched -> num_clusters; i++)
   {
   sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "cluster", "num_cpus: %d running cpus: %d", sched -> clusters[i] -> num_cpus, sched -> clusters[i] -> num_running_cpus);
   log_list(sched -> jobs[i]);
   }

 for (int i = 0; i < sched -> num_clusters; i++)
	 log_list(sched -> gaps[i]);

 for (int i = 0; i < sched -> num_clusters; i++)
	 log_list(sched -> limits[i]);
 }


void log_print_ffms(first_free_slot **first_free_slots, int num_cpus)
  {
  for (int i = 0; i < num_cpus; i++)
	sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "ffs", "%d: %ld", i, first_free_slots[i] -> time);

  for (int i = 0; i < num_cpus; i++)
	sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "ffms", "%d: %s, %ld, cpus: %d ", i, first_free_slots[i]->ninfo->get_name(),first_free_slots[i]->available_mem, first_free_slots[i]->ninfo->get_cores_total());
  }


void log_print_ffnot(int first_free_slot_not[], int num_cpus)
  {
  for (int i = 0; i < num_cpus; i++)
	sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "ff not", "%d: %d", i, first_free_slot_not[i]);
  }

void log_server_jobs(server_info *sinfo)
  {
  int i;
  int counter;

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "job on node not found", "job not in sinfo -> scheduled_job");

  i=0;
  while (sinfo->jobs[i]!=NULL)
    {
    if (sinfo -> jobs[i] -> state == JobNoState)
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, sinfo -> jobs[i] -> job_id.c_str(), "JobNoState");

     if (sinfo -> jobs[i] -> state == JobQueued)
       sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, sinfo -> jobs[i] -> job_id.c_str(), "JobQueued");

     if (sinfo -> jobs[i] -> state == JobRunning)
       sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, sinfo -> jobs[i] -> job_id.c_str(), "JobRunning");

     if (sinfo -> jobs[i] -> state == JobHeld)
       sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, sinfo -> jobs[i] -> job_id.c_str(), "JobHeld");

     if (sinfo -> jobs[i] -> state == JobWaiting)
       sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, sinfo -> jobs[i] -> job_id.c_str(), "JobWaiting");

     if (sinfo -> jobs[i] -> state == JobTransit)
       sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, sinfo -> jobs[i] -> job_id.c_str(), "JobTransit");

     if (sinfo -> jobs[i] -> state == JobExiting)
       sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, sinfo -> jobs[i] -> job_id.c_str(), "JobExiting");

     if (sinfo -> jobs[i] -> state == JobSuspended)
       sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, sinfo -> jobs[i] -> job_id.c_str(), "JobSuspended");

     if (sinfo -> jobs[i] -> state == JobCompleted)
       sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, sinfo -> jobs[i] -> job_id.c_str(), "JobCompleted");

     i++;
    }
  }
