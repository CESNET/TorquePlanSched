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

#include <string.h>
#include "globals.h"

#include "plan_config.h"
#include "plan_data_types.h"
#include "plan_list_operations.h"
#include "plan_first_free_slot.h"
#include "plan_free_data.h"
#include "plan_gaps.h"
#include "plan_gaps_memory.h"
#include "plan_log.h"
#include "plan_limits.h"
#include "plan_schedule.h"

using namespace std;

int ith_smallest(node_info *considered_node,
				int attempt_count,
				first_free_slot **first_free_slots,
				int num_ffs,
				int *ffs_availability,
				int node_latest_cpu_index,
				int total_latest_cpu_index)
  {
  int possible_min;

  int result = -1;
  time_t min = LONG_MAX;

  for (int i = 0; i < num_ffs; i++)
    {
	possible_min = first_free_slots[i] -> time;

	//podstrcim cas pro zakazany index pokud by byl problem jen kvuli nedostatku pameti drive kam se ale job nevejde
	if (ffs_availability[i] == -4 &&
		node_latest_cpu_index != -1 &&
		first_free_slots[i] -> ninfo == considered_node &&
		first_free_slots[node_latest_cpu_index] -> ninfo == first_free_slots[i] -> ninfo &&
		first_free_slots[node_latest_cpu_index] -> time > first_free_slots[i] -> time
	   )
	  {
	  ffs_availability[i] = -5;
	  }

	if (ffs_availability[i] == -4 &&
		node_latest_cpu_index == -1 &&
		total_latest_cpu_index != -1 &&
		first_free_slots[total_latest_cpu_index] -> time > first_free_slots[i] -> time
	   )
	  {
	  ffs_availability[i] = -5;
	  }

    if (possible_min < min &&
        (ffs_availability[i] == 0 || ffs_availability[i] == -5 ||
         (ffs_availability[i] > 0 && ffs_availability[i] < attempt_count && first_free_slots[i] -> ninfo == considered_node)
        )
       )
      {
      min = possible_min;
      result = i;
      }

    if (ffs_availability[i] == -5)
    	ffs_availability[i] = -4;
    }

  if (result == -1)
    return result;

  ffs_availability[result] = attempt_count;

  return result;
  }

long int find_earliest_node(int counter, plan_job* job, int num_first_free_slot, first_free_slot **first_free_slots, int *first_free_slot_availability)
  {
  int node_found;
  node_info* node=NULL;
  node_info* best_node=NULL;

  long int overall_earliest_time = 7226578800;
  long int latest_node_time =0;

  if (counter == 0) {
	  for (int j = 0; j<job->req_num_nodes; j++)
		  job -> ninfo_arr[j] == NULL;
  }

  for (int i = 0; i < num_first_free_slot; i++)
    {
	if (first_free_slots[i]->ninfo->get_cores_total() < job->req_ppn || first_free_slots[i]->ninfo-> is_down() || first_free_slots[i]->ninfo->is_offline()  || first_free_slots[i]->ninfo->is_notusable())
	  continue;

	if (first_free_slot_availability[i] == -3 || first_free_slot_availability[i] == -8 || first_free_slot_availability[i] == -9)
	  continue;

	node_found = false;
    for (int j = 0; j<counter; j++)
      if (job -> ninfo_arr[j] == first_free_slots[i]->ninfo)
      		node_found = true;
    if (node_found)
      continue;

    if (best_node == NULL)
      {
      //overall_earliest_time = first_free_slots[i]->time;
      best_node=first_free_slots[i]->ninfo;
      }

    if (node == NULL)
    	node = first_free_slots[i]->ninfo;

    //hledam nejpozdejsi ffs prave kontrolovaneho uzlu
    if (node == first_free_slots[i]->ninfo && latest_node_time < first_free_slots[i]->time)
    	latest_node_time = first_free_slots[i]->time;

    //z latest_node_time si zapamatuju ten nejposlednejsi a tak dostanu nejdrivejsi uzel ale jeho nejposlednejsi cas ve ffs
    if (node != first_free_slots[i]->ninfo || i == num_first_free_slot-1)
      {
    	if (latest_node_time <= overall_earliest_time)
    	  {
    	  overall_earliest_time = latest_node_time;
    	  best_node = node;
    	  }
    	node = first_free_slots[i]->ninfo;
    	latest_node_time =0;
      }
    }

  job -> ninfo_arr[counter] = best_node;
  return overall_earliest_time;
  }

plan_list* update_job(int pbs_sd, plan_job* job, int num_first_free_slot, first_free_slot **first_free_slots, plan_list* limits, int k, plan_cluster* cluster)
  {
  plan_list* gaps_list;
  plan_gap* new_gap;
  time_t gap_start = -1;
  time_t gap_end = -1;
  node_info* considered_node;
  int cpu_index;
  int gap_usage;
  int job_cpu;
  int tmp_job_cpu;
  int attempt_count;
  int next_time;
  int node_latest_cpu_index;
  int total_latest_cpu_index;
  int *tmp_cpu_arr;
  int plan_num_nodes;
  int excl_job_exception;
  int wipe_fixed_nodes;
  int node_prohibited;
  int *node_ppn_free;
  Resource *res;
  
  int *first_free_slot_availability;
  first_free_slot_availability = (int*) malloc(num_first_free_slot * sizeof(int));

  if (job == NULL || first_free_slots == NULL)
    return NULL;

  /*
   * FIX NODES init
   */

    //je-li nejakej node NULL pak NULLuju vsechny
    wipe_fixed_nodes = 0;
    for (int k = 0; k < job -> req_num_nodes; k++)
	  if (job -> fixed_nname_arr[k] == NULL)
		wipe_fixed_nodes = 1;

    if (wipe_fixed_nodes)
      for (int k = 0; k < job -> req_num_nodes; k++)
        if (job -> fixed_nname_arr[k] != NULL)
        {
        free(job -> fixed_nname_arr[k]);
        job -> fixed_nname_arr[k] = NULL;
        }

    if ((node_ppn_free = (int*)malloc(job -> req_num_nodes * sizeof(int))) == NULL)
      {
      perror("Memory Allocation Error");
      return NULL;
      }

    for (int k = 0; k < job -> req_num_nodes; k++)
      {
	  node_ppn_free[k] = 1;
      }

  /*
   * first_free_slot_availability init
   *
   * -1 node down
   * -2 not enough CPUs on node
   * -3 not enough mem
   * -4 not enough available mem
   * -5 exception for available mem
   * -6 cpu already used for this job
   * -7 prohibited node
   */

  for (int i = 0; i < num_first_free_slot; i++)
    {
	if (first_free_slots[i] -> time == -1)
	  {
	  first_free_slot_availability[i] = -1;
	  continue;
	  }

	if (first_free_slots[i] -> ninfo -> get_avail_before() != 0 && first_free_slots[i] -> ninfo -> get_avail_before() < job->completion_time)
	  {
        first_free_slot_availability[i] = -8;
        continue;
	  }

	if (first_free_slots[i] -> ninfo->walltime_limit_min != 0 && first_free_slots[i] -> ninfo->walltime_limit_min > job->estimated_processing)
	  {
        first_free_slot_availability[i] = -9;
        continue;
	  }

	if (first_free_slots[i] -> ninfo->walltime_limit_max != 0 && first_free_slots[i] -> ninfo->walltime_limit_max < job->estimated_processing)
	  {
        first_free_slot_availability[i] = -9;
        continue;
	  }
        
        if (job->req_gpu > 0)
          {
            if ((res = first_free_slots[i] -> ninfo->get_resource("gpu")) == NULL)
            {
            first_free_slot_availability[i] = -9;
            continue;                
            }
            
            if (res->get_capacity() < job->req_gpu)
            {
            first_free_slot_availability[i] = -9;
            continue;                
            }                
          }

	// prohibited node is something like node down
	node_prohibited = 1;
    if (!job->jinfo->is_exclusive())
	if (!wipe_fixed_nodes)
	  {
	  for (int k = 0; k < job -> req_num_nodes; k++)
	    if (strcmp(job -> fixed_nname_arr[k], first_free_slots[i] -> ninfo -> get_name()) == 0)
	      node_prohibited = 0;

	  if (node_prohibited)
      {
        first_free_slot_availability[i] = -7;
        continue;
      }
	  }

	if (job -> req_ppn > first_free_slots[i] -> ninfo -> get_cores_total())
	  {
	  first_free_slot_availability[i] = -2;
	  continue;
	  }

	if (job -> req_mem > first_free_slots[i] -> mem ||
		job -> req_scratch_local > first_free_slots[i] -> scratch_local)
	  {
	  first_free_slot_availability[i] = -3;
	  continue;
	  }

	if (job -> req_mem > first_free_slots[i] -> available_mem ||
		job -> req_scratch_local > first_free_slots[i] -> available_scratch_local)
	  {
	  first_free_slot_availability[i] = -4;
	  continue;
	  }

	first_free_slot_availability[i] = 0;
    }


  /*
   *
   * GapList creation
   *
   */
  gaps_list = list_create(Gaps);
  if ((tmp_cpu_arr = (int*)malloc(sizeof(int) * job -> req_ppn)) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  /*
   * job initialization
   * and other init
   */

  considered_node = NULL;
  plan_num_nodes = 0;
  attempt_count = 1;
  gap_usage = 0;
  cpu_index = -1;
  total_latest_cpu_index = -1;
  node_latest_cpu_index = -1;

  job -> start_time = -1;
  job -> completion_time = -1;

  if (job->jinfo->is_exclusive())
	  excl_job_exception = 1;
  else
	  excl_job_exception = 0;

  for (int i = 0; i < job -> usage; i++)
	job -> cpu_indexes[i] = -1;

  for (int i = 0; i < job -> req_num_nodes; i++)
	job -> latest_ncpu_index[i] = -1;

  /*
   * main while start
   * i=0!!!!!!!!
   */
  tmp_job_cpu = 0;
  job_cpu = 0;

  long int latest_considered_node_ffs_time;
  long int min_considered_node_ffs_time;
  long int excl_start_time = 4091930266;
  node_info* best_node;
  int node_found;

  if (job->jinfo->is_exclusive())
    {

	latest_considered_node_ffs_time = 0;
	min_considered_node_ffs_time = 0;

	best_node = NULL;
	considered_node = NULL;

	for (int excl_nodes_counter=0; excl_nodes_counter<job->req_num_nodes; excl_nodes_counter++)
		job -> start_time = find_earliest_node(excl_nodes_counter,job, num_first_free_slot, first_free_slots, first_free_slot_availability);

	if (job -> start_time == 7226578800)
		return NULL;

	for (int i = 0; i < num_first_free_slot; i++)
	    for (int j = 0; j<job->req_num_nodes; j++)
	      if (job -> ninfo_arr[j] == first_free_slots[i]->ninfo)
	    	  if (job -> start_time < first_free_slots[i]->time)
	    		  first_free_slots[i]->time = job -> start_time;

	job -> completion_time = job -> start_time + job -> estimated_processing;

	int index_counter=0;
	int ppn_counter;
	for (int excl_nodes_counter=0; excl_nodes_counter<job->req_num_nodes; excl_nodes_counter++)
	  {
	  ppn_counter = 0;
	  for (int i = 0; i < num_first_free_slot; i++)
			if (job->ninfo_arr[excl_nodes_counter]==first_free_slots[i]->ninfo && ppn_counter < job->req_ppn && index_counter< job->usage)
			{
			job->cpu_indexes[index_counter]=i;
			index_counter++;
			ppn_counter++;
			}
	  }
    }

  if (!job->jinfo->is_exclusive())
  while (job_cpu < job -> usage)
    {
	//pokud najdu nejdrivejsi procesor nastavim mu first_free_slot_availability na true


	cpu_index = ith_smallest(considered_node,
							attempt_count,
							first_free_slots,
							num_first_free_slot,
							first_free_slot_availability,
							node_latest_cpu_index,
							total_latest_cpu_index);

	//ith_smallest failed
	if (cpu_index==-1)
	  {
		/*
	  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "ith smallest cpu not found!!!", "%d, latest_cpu_index: %d, attempt_count: %d",job -> job_id, node_latest_cpu_index, attempt_count);
	  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, job -> jinfo -> name, "req_nodes: %ld req_ppn: %d req_mem: %ld",job -> req_num_nodes, job -> req_ppn, job -> req_mem);
	  for (int m = 0; m < job -> req_num_nodes; m++)
		  if (job -> fixed_nname_arr[m] == NULL)
			  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, job -> jinfo -> name, "fixednode: null");
		  else
			  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, job -> jinfo -> name, "fixednode: %s", job -> fixed_nname_arr[m]);

	  log_print_ffms(first_free_slots,num_first_free_slot);
	  log_print_ffnot(first_free_slot_availability, num_first_free_slot);
	  */
      if (job -> fixed_nname_arr[0] != NULL)
        {
        free(job -> fixed_nname_arr[0]);
        job -> fixed_nname_arr[0] = NULL;
        }
	  free(tmp_cpu_arr);

	  return NULL;
	  }

    if (considered_node != NULL && considered_node != first_free_slots[cpu_index] -> ninfo)
	  {
      job_cpu = plan_num_nodes * job -> req_ppn;
	  first_free_slot_availability[cpu_index] = ++attempt_count;
	  }

	considered_node = first_free_slots[cpu_index] -> ninfo;
/*
	if (job->jinfo->is_exclusive){
   	 job->original_req_ppn = job->req_ppn;
   	 job->req_ppn=considered_node->get_cores_total();
     }
*/

	/*
	 * k tomuto by nemelo dochazet moc casto: kvuli vyjimkam zpusobene pameti je mozne ze indexy nebudou serazeny od nejdrivejsiho dostupneho
	 * zajistim vlozeni na spravne misto jinak se mohou chybne vytvorit diry a nastavit spatny starttime
	 *
	 */
	tmp_job_cpu = job_cpu;
	while (tmp_job_cpu > 0 && first_free_slots[job -> cpu_indexes[tmp_job_cpu - 1]] -> time > first_free_slots[cpu_index] -> time)
	 {
	 job -> cpu_indexes[tmp_job_cpu] = job -> cpu_indexes[tmp_job_cpu - 1];
	 tmp_job_cpu--;
	 }
	job -> cpu_indexes[tmp_job_cpu] = cpu_index;

	if (node_latest_cpu_index != -1 && first_free_slots[node_latest_cpu_index] -> time <= first_free_slots[cpu_index] -> time)
		node_latest_cpu_index = cpu_index;

	if (node_latest_cpu_index == -1)
		node_latest_cpu_index = cpu_index;

	if (total_latest_cpu_index != -1 && first_free_slots[total_latest_cpu_index] -> time <= first_free_slots[cpu_index] -> time)
		total_latest_cpu_index = cpu_index;

	if (total_latest_cpu_index == -1)
		total_latest_cpu_index = cpu_index;

	tmp_cpu_arr[job_cpu - plan_num_nodes * job -> req_ppn] = cpu_index;

	//ppn nalezeno oznacit aby se uz nepouzilo a jdem na dalsi ppn
	if (job_cpu + 1 == (plan_num_nodes + 1) * job -> req_ppn)
	  {

      //do start_time dame index s poslednim starttimem
	  job -> start_time = first_free_slots[job -> cpu_indexes[job_cpu]] -> time;
	  job -> completion_time = job -> start_time + job -> estimated_processing;

	  job -> ninfo_arr[plan_num_nodes] = considered_node;
	  job -> latest_ncpu_index[plan_num_nodes] = node_latest_cpu_index;

	  for (int i = 0; i < job -> req_ppn; i++)
		  first_free_slot_availability[tmp_cpu_arr[i]] = -6;

	  if (wipe_fixed_nodes)
	    {
		job -> fixed_nname_arr[plan_num_nodes] = strdup(considered_node -> get_name());
	    }else
	    {
	    //uzel bude pouzit, pokud je to naposled co muze byt pouzit, tak zbytek zakazat
	    int not_anymore;
	    not_anymore = 0;
	    for (int k = 0; k < job -> req_num_nodes; k++)
		  if (node_ppn_free[k] == 1 && strcmp(job -> fixed_nname_arr[k], considered_node -> get_name()) == 0)
		    {
			if (not_anymore == 1 && !job->jinfo->is_exclusive())
			  {//kdzy tu jsem podruhe tak sem job budu moc dat jeste jednou, nezakazovat jeste tenhle uzel
			  not_anymore = 0;
			  break;
			  }
			not_anymore = 1;
			node_ppn_free[k] = 0;
		    }

	    if (not_anymore == 1)
			for (int i = 0; i < num_first_free_slot; i++)
			  if (first_free_slots[i] -> ninfo == considered_node && first_free_slot_availability[i] != -6)
				  first_free_slot_availability[i] = -7;
	    }

	  considered_node = NULL;

	  if (job->jinfo->is_exclusive() && job->original_req_ppn != 0){
		job->req_ppn = job->original_req_ppn;
	   	job->original_req_ppn=0;
	    }

	  node_latest_cpu_index = -1;
	  plan_num_nodes++;

	  if (plan_num_nodes == job->req_num_nodes)
		  excl_job_exception = 0;
	  }

	job_cpu++;
    }

  if (job->jinfo->is_exclusive() && job->original_req_ppn != 0){
	job->req_ppn = job->original_req_ppn;
   	job->original_req_ppn=0;
    }

  /*
   * main while ended
   *
   */

  adjust_job_starttime_according_to_limits(limits, job, cluster);

 // for (i=0; i < num_first_free_slot; i++)
	//if (first_free_slots[i] -> time != -1)
		//first_free_slot_availability[i] = 0;

  /*
   * new gaps creation
   */

  gap_usage = 0;
  cpu_index = -1;
  gap_start = -1;
  gap_end = -1;
  job -> run_me = 1;
  job_cpu = 0;
  if (job->jinfo->is_exclusive())
{

  int excl_index_num = 0;
  if (job->cpu_indexes != NULL)
    free(job->cpu_indexes);

  if ((job->cpu_indexes = (int*)malloc(num_first_free_slot * sizeof(int))) == NULL)
			       {
			       perror("Memory Allocation Error");
			       return NULL;
			       }

  for (int i = 0; i < num_first_free_slot; i++)
  {
  for (int j = 0; j < job->req_num_nodes; j++)
	  if (first_free_slots[i]->ninfo == job->ninfo_arr[j]) {
		  int tmp_index=excl_index_num;
		  while (tmp_index > 0 && first_free_slots[job->cpu_indexes[tmp_index-1]]->time > first_free_slots[i]->time)
		      {
			  job->cpu_indexes[tmp_index]=job->cpu_indexes[tmp_index-1];
		    	  tmp_index--;
		      }

		  job->cpu_indexes[tmp_index]=i;
		  excl_index_num++;
	  }
  }

  job->usage = excl_index_num;

  for (int i = 0; i < excl_index_num; i++)
  {
    {
		cpu_index = job->cpu_indexes[i];

		//nastavime zda muze byt job spusten...
		if (first_free_slots[cpu_index] -> free_to_run == 0)
		  job -> run_me = 0;

		first_free_slots[cpu_index] -> free_to_run = 0;

		if (gap_start == -1)
		  {
		  gap_start = first_free_slots[cpu_index] -> time;
		  }

		if (gap_start != first_free_slots[cpu_index] -> time)
		  {
		  gap_end = first_free_slots[cpu_index] -> time;

		  new_gap = (plan_gap*) list_add_end(gaps_list, gap_fillin((plan_gap*) NULL, gap_start, gap_end, gap_end - gap_start, gap_usage, job, (plan_gap*)NULL, (plan_gap*)NULL));
		  new_gap -> nodes_memory = gap_memory_create(new_gap, job, num_first_free_slot, first_free_slots);

		  gap_start = first_free_slots[cpu_index] -> time;
		  }

		gap_usage++;
		job_cpu++;
    }

  }
}

  if (!job->jinfo->is_exclusive())
  while (job_cpu < job -> usage)
    {
	cpu_index = job -> cpu_indexes[job_cpu];

	//nastavime zda muze byt job spusten...
	if (first_free_slots[cpu_index] -> free_to_run == 0)
	  job -> run_me = 0;

	first_free_slots[cpu_index] -> free_to_run = 0;

	if (gap_start == -1)
	  {
	  gap_start = first_free_slots[cpu_index] -> time;
	  }

	if (gap_start != first_free_slots[cpu_index] -> time)
	  {
	  gap_end = first_free_slots[cpu_index] -> time;

	  new_gap = (plan_gap*) list_add_end(gaps_list, gap_fillin((plan_gap*) NULL, gap_start, gap_end, gap_end - gap_start, gap_usage, job, (plan_gap*)NULL, (plan_gap*)NULL));
	  new_gap -> nodes_memory = gap_memory_create(new_gap, job, num_first_free_slot, first_free_slots);

	  gap_start = first_free_slots[cpu_index] -> time;
	  }

	gap_usage++;
	job_cpu++;
    }

  if (job->jinfo->is_exclusive())
      first_free_slots_update_exclusive(job, num_first_free_slot, first_free_slots);

  if (!job->jinfo->is_exclusive())
    first_free_slots_update(job, num_first_free_slot, first_free_slots);

  free(tmp_cpu_arr);
  free(node_ppn_free);
  return gaps_list;
  }


int update_sched(int pbs_sd, sched* schedule, int k, time_t time)
  {
  plan_list* gaps_job;
  plan_job* job_to_update;
  plan_cluster* cluster_k = schedule -> clusters[k];
  void* last_gap;
  first_free_slot **first_free_slots;
  time_t max_completion_time;

  max_completion_time = time;

  first_free_slots = first_free_slots_create(schedule -> clusters[k]);

  first_free_slots_initialize(schedule -> clusters[k], first_free_slots, time);

  free_list(schedule ->limits[k]);

  schedule ->limits[k] = list_create(Limits);

  //log_print_ffms(first_free_slots,schedule -> clusters[k]->num_cpus);

  //sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "number of jobs", "in cluster: %d queued: %d", schedule -> jobs[k] -> count, num_running_cpus);

  free_list(schedule -> gaps[k]);

  schedule -> gaps[k] = list_create(Gaps);

  schedule -> gaps[k] -> sinfo=schedule -> jobs[k] -> sinfo;

  schedule -> clusters[k] -> num_queued_jobs = 0;

  update_limits_values(schedule, k);

  schedule -> jobs[k] -> current = NULL;
  
  while (list_get_next(schedule -> jobs[k]) != NULL)
    {
    job_to_update = (plan_job*)schedule -> jobs[k] -> current;
    
    if (job_to_update->available_after > time)
        continue;
    
    if (job_to_update->jinfo ->state != JobQueued)
        continue;
    
    if (job_to_update->jinfo->priority == 56)
        continue;
    
    if (strcmp((char*)job_to_update->jinfo->account.c_str(), "galaxyelixir") == 0)
        job_to_update->jinfo->priority = 42;
            
    if ((job_to_update->jinfo->priority == 42) || (job_to_update->req_gpu > 0)){
    	list_move_item(schedule -> jobs[k], job_to_update, NULL);
    	sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "job_priority", "job: %s", job_to_update -> jinfo -> job_id.c_str());
        job_to_update->jinfo->priority = 56;
      }
    }

  schedule -> jobs[k] -> current = NULL;
  while (list_get_next(schedule -> jobs[k]) != NULL)
    {
     
	job_to_update = (plan_job*)schedule -> jobs[k] -> current;
        
        if (job_to_update->jinfo->priority == 56) 
           job_to_update->jinfo->priority = 42;

	if (job_to_update -> jinfo -> state == JobRunning)
		add_job_to_limits(schedule ->limits[k], job_to_update, schedule -> clusters[k]);

	if (job_to_update -> jinfo -> state == JobQueued)
	  {
	  schedule -> clusters[k] -> num_queued_jobs++;

	  gaps_job = update_job(pbs_sd, job_to_update, cluster_k -> num_cpus, first_free_slots, schedule ->limits[k], k, schedule -> clusters[k]);

	  if (gaps_job == NULL) {
		  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "job_update_failed", "job: %s", job_to_update -> jinfo -> job_id.c_str());
		  list_remove_item(schedule -> jobs[k], job_to_update, 1);
		  free_first_free_slots(first_free_slots, schedule -> clusters[k]);
		  return 2;
	  }

	  add_job_to_limits(schedule ->limits[k], job_to_update, schedule -> clusters[k]);

	  if (gaps_job != NULL)
	    {
	    gaps_job -> current = NULL;
	    while (list_get_next(gaps_job) != NULL)
		  insert_new_gap(schedule -> gaps[k], (plan_gap*)NULL ,(plan_gap*)gaps_job -> current);

	    free_list(gaps_job);
	    }

	  job_to_update -> run_me = 1;

	  } else
	  {
	  //for (int i = 0; i < job_to_update -> req_num_nodes; i++)
	    //job_to_update -> ninfo_arr[i] = NULL;
	  job_to_update -> run_me = 0;
	  }

	if (max_completion_time < job_to_update -> completion_time)
	  max_completion_time = job_to_update -> completion_time;
    }
/* serazani jobu nedelame
  schedule -> jobs[k] -> current=NULL;
  while (list_get_next(schedule -> jobs[k]) != NULL)
	  list_adjust_job_position(schedule -> jobs[k],(plan_job*)schedule -> jobs[k] -> current);
*/

  if (schedule -> jobs[k] -> num_items > 0)
    {
    gaps_job = gap_create_not_the_last(cluster_k -> num_cpus, first_free_slots, max_completion_time);

    if (gaps_job != NULL)
      {
      gaps_job -> current = NULL;
      while (list_get_next(gaps_job) != NULL)
        {
    	//sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "not last gap", "usage: %d start_time: %i:%i:%i end_time: %i:%i:%i duration: %ld foll job id %d", ((plan_gap*)gaps_job -> current) -> usage,  localtime(&((plan_gap*)gaps_job -> current) -> start_time) -> tm_hour,localtime(&((plan_gap*)gaps_job -> current) -> start_time) -> tm_min,localtime(&((plan_gap*)gaps_job -> current) -> start_time) -> tm_sec, localtime(&((plan_gap*)gaps_job -> current) -> end_time) -> tm_hour,localtime(&((plan_gap*)gaps_job -> current) -> end_time) -> tm_min,localtime(&((plan_gap*)gaps_job -> current) -> end_time) -> tm_sec, ((plan_gap*)gaps_job -> current) -> duration, NULL);
        insert_new_gap(schedule -> gaps[k], (plan_gap*)NULL ,(plan_gap*)gaps_job -> current);
        }
      free_list(gaps_job);
      }
    }

  //sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "adding last gap", "start %ld usage: %ld",max_completion_time,num_running_cpus );
  last_gap = gap_create_last(max_completion_time, cluster_k);
  list_add_end(schedule -> gaps[k], last_gap);
  //sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, " final gaps number", "count: %d", schedule -> gaps[k] -> count);

  free_first_free_slots(first_free_slots, schedule -> clusters[k]);

  schedule -> updated[k] = 1;

  return 0;
  }


int update_planned_start(int pbs_sd, sched* schedule, int k)
  {
  plan_job* job;
  schedule -> jobs[k] -> current=NULL;
  while (list_get_next(schedule -> jobs[k]) != NULL)
    {
	job = (plan_job*)schedule -> jobs[k] -> current;

	if (job -> jinfo -> state != JobQueued)
	  continue;

    if (string(job->jinfo->job_id).find(conf.local_server) == string::npos)
    	continue;

    update_job_planned_start(pbs_sd, job);
    update_job_comment(pbs_sd, job->jinfo, "");
    }

  return 0;
  }


int update_planned_nodes(int pbs_sd, sched* schedule, int k)
  {
  int num_ppn;
  plan_job* job;
  char* planned_nodes;

  planned_nodes = NULL;
  schedule -> jobs[k] -> current=NULL;

  while (list_get_next(schedule -> jobs[k]) != NULL)
    {
	job = (plan_job*)schedule -> jobs[k] -> current;
	if (job -> ninfo_arr == NULL || job -> jinfo -> state != JobQueued)
		continue;

    // jestli je job remote tak nic
    if (string(job->jinfo->job_id).find(conf.local_server) == string::npos)
    	continue;

	num_ppn = 0;
	if ((planned_nodes = (char*)realloc(planned_nodes, 50 * job -> req_num_nodes)) == NULL)
      {
      perror("Memory Allocation Error");
      return 1;
      }

	if (job->start_time > 0)
	{
	for (int i = 0; i < job -> req_num_nodes;i++)
	  {
	  if (i == 0 )
	  {sprintf(planned_nodes, "%s",job -> ninfo_arr[i] -> get_name());}
	  else
	  {sprintf(planned_nodes, "%s, %s",planned_nodes, job -> ninfo_arr[i] -> get_name());}
	  }
    } else {sprintf(planned_nodes, "");}

	update_job_planned_nodes(pbs_sd, job -> jinfo, planned_nodes);
    }
  if (planned_nodes!= NULL)
    free(planned_nodes);

  return 0;
  }
