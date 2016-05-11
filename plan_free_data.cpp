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
#include "misc.h"
#include <limits.h>

#include "node_info.h"

#include "plan_list_operations.h"

int free_gap_memory(plan_gap* gap)
  {
  for (int i=0; i < gap  -> num_nodes;i++)
    free(gap -> nodes_memory[i]);

  free(gap -> nodes_memory);
  return 0;
  }

void free_limit_account(plan_limit* limit)
  {
  for (int i = 0; i < limit -> num_accounts; i++)
    {
	free(limit-> accounts[i]->num_cpus);
	free(limit -> accounts[i]);
    }
  free(limit -> accounts);
  limit -> num_accounts = 0;
  }

void free_job(plan_job* job)
  {
  //if (job -> ninfo_arr != NULL)
  //  free_nodes(job -> ninfo_arr);
  if (job -> ninfo_arr != NULL)
    free(job -> ninfo_arr);

  if (job -> cpu_indexes != NULL)
    free(job -> cpu_indexes);

  if (job -> latest_ncpu_index != NULL)
  	free(job -> latest_ncpu_index);

  if (job->fixed_nname_arr != NULL)
  	  for (int i=0; i< job->req_num_nodes; i++)
  		  if (job->fixed_nname_arr[i]!= NULL)
  			  free(job->fixed_nname_arr[i]);
  free(job->fixed_nname_arr);

  free(job);
  }

void free_gap(plan_gap* gap)
  {
  free_gap_memory(gap);
  free(gap);
  }

void free_limit(plan_limit* limit)
  {
  free_limit_account(limit);
  free(limit);
  }

int free_list(plan_list* list_to_free)
  {
  plan_job *job;
  plan_gap *gap;
  plan_limit * limit;
  void *next_item;

  list_to_free -> sinfo = NULL;
  if (list_to_free == NULL)
    return 1;

  next_item = list_to_free -> first;

  while (next_item != NULL)
    {
    list_to_free -> current = next_item;
    next_item = get_successor(list_to_free -> current, list_to_free -> type);

    if (list_to_free -> type == Jobs)
      {
      job = (plan_job*)list_to_free -> current;
      free_job(job);
      }
    if (list_to_free -> type == Gaps)
      {
      gap = (plan_gap*)list_to_free -> current;
      free_gap(gap);
      }
    if (list_to_free -> type == Limits)
      {
      limit = (plan_limit*)list_to_free -> current;
      free_limit(limit);
      }
    }

  free(list_to_free);
  list_to_free = NULL;
  return 0;
  }

int free_first_free_slots(first_free_slot** first_free_slots, plan_cluster* cluster_k)
  {
  int counter;

  counter=0;
  for (int i = 0; i < cluster_k -> num_nodes; i++)
    {
	free(first_free_slots[counter] -> releated_cpus);
	counter += cluster_k -> nodes[i] -> get_cores_total();
	}

  for (int i= 0; i < cluster_k -> num_cpus; i++)
	  free(first_free_slots[i]);

  free(first_free_slots);

  return 0;
  }

void free_jobs_completed_jobs(JobInfo **jarr)
  {
  if (jarr == NULL)
    return;

 // for (i = 0; jarr[i] != NULL; i++)
//	if (jarr[i]->state == JobCompleted)
  //    free_job_info(jarr[i]);

  free(jarr);
  }

void free_queues_completed_jobs(queue_info **qarr, char free_jobs_too)
  {
  int i;

  if (qarr == NULL)
    return;

  for (i = 0; qarr[i] != NULL; i++)
    {
    if (free_jobs_too)
      qarr[i] -> jobs.clear();

    free_queue_info(qarr[i]);
    }

  free(qarr);

  }


void free_server_completed_jobs(server_info *sinfo, int free_objs_too)
  {
  if (sinfo == NULL)
    return;

  if (free_objs_too)
    {
    free_queues_completed_jobs(sinfo -> queues, 1);
    //free_nodes(sinfo -> nodes);
    free(sinfo -> nodes);
    }

  free_token_list(sinfo -> tokens);

  free_server_info(sinfo);
  }
