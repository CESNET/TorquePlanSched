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
#include "misc.h"
#include <limits.h>

#include "plan_gaps_memory.h"
#include "plan_list_operations.h"
#include "plan_limits.h"
#include "plan_schedule.h"

plan_gap** considered_gaps = NULL;


plan_gap* gap_create()
  {
  plan_gap *gap;

  if ((gap = (plan_gap*) malloc(sizeof(plan_gap))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  gap -> num_nodes = 0;

  gap -> successor = NULL;

  gap -> predeccessor = NULL;

  gap -> nodes_memory=NULL;

  return gap;
  }

void* gap_fillin(plan_gap* gap, time_t start_time, time_t end_time, time_t duration,int usage,plan_job* following_job, plan_gap* successor, plan_gap* predeccessor)
  {
  if (gap == NULL)
    gap=gap_create();

  gap -> start_time = start_time;

  gap -> end_time = end_time;

  gap -> duration = duration;

  gap -> usage = usage;

  gap -> following_job = following_job;

  gap -> successor = successor;

  gap -> predeccessor = predeccessor;

  return (void*) gap;
  }



int gaps_ith_smallest(first_free_slot **first_free_slots, int num_ffs, int *ffs_availability)
  {
  int result=-1;
  time_t min=LONG_MAX;

  for (int i=0; i<num_ffs; i++)
    {
    if (first_free_slots[i] -> time < min && ffs_availability[i] == 0)
      {
      min = first_free_slots[i] -> time;
      result = i;
      }
    }

  if (result != -1)
    ffs_availability[result] = 1;

  return result;
  }

plan_list* gap_create_not_the_last(int num_cpus, first_free_slot **first_free_slots, time_t max_completion_time)
  {
  int cpu_index;
  int gap_usage;
  time_t gap_start = -1;
  time_t gap_end = -1;
  plan_gap* new_gap;
  plan_list* gaps_list;
  
  int *first_free_slot_availability;
  first_free_slot_availability = (int*) malloc(num_cpus * sizeof(int));

  for (int i = 0; i < num_cpus; i++)
    if (first_free_slots[i] -> time == -1)
      {
      first_free_slot_availability[i] = 1;
      }
    else
      {
      first_free_slot_availability[i] = 0;
      }

  gaps_list = list_create(Gaps);

  cpu_index = -1;
  gap_usage = 0;

  for (int i = 0; i < num_cpus; i++)
    {
    cpu_index = gaps_ith_smallest(first_free_slots, num_cpus, first_free_slot_availability);

    if (cpu_index == -1)
      continue;

    if (gap_start == -1)
      {
      gap_start = first_free_slots[cpu_index] -> time;
      }

    if (gap_start != first_free_slots[cpu_index] -> time)
      {
      gap_end = first_free_slots[cpu_index] -> time;

      if (gap_end > max_completion_time)
        {
        gap_end = max_completion_time;
        }

	  //create gap
      if (gap_end > gap_start)
        {
        new_gap = (plan_gap*) list_add_end(gaps_list, gap_fillin((plan_gap*)NULL, gap_start, gap_end, gap_end - gap_start, gap_usage, (plan_job*)NULL, (plan_gap*)NULL, (plan_gap*)NULL));
        new_gap -> nodes_memory = gap_memory_create_last(new_gap, num_cpus, first_free_slots);
        }

      gap_start = first_free_slots[cpu_index] -> time;
      }
    }

  gap_usage++;

  free(first_free_slot_availability);
  return gaps_list;
  }

void* gap_create_last(time_t start_time, plan_cluster* cluster_k)
  {
  int num_nodese_online;
  plan_gap* gap;

  gap = gap_create();

  gap -> start_time = start_time;

  gap -> end_time = LONG_MAX;

  gap -> duration = LONG_MAX;

  gap -> following_job = NULL;

  gap -> successor = NULL;

  gap -> predeccessor = NULL;

  gap -> nodes_memory = NULL;

  num_nodese_online = 0;

  if (cluster_k == NULL)
    {
    gap -> usage = 0;
    }
  else
    {
    gap -> usage = cluster_k -> num_running_cpus;

    if ((gap -> nodes_memory = (plan_gap_mem**)malloc(cluster_k -> num_nodes * sizeof(plan_gap_mem*))) == NULL)
      {
      perror("Memory Allocation Error");
      return NULL;
      }

    gap -> num_nodes = cluster_k -> num_nodes;

    for (int i = 0; i < cluster_k -> num_nodes; i++)
      {
      if (cluster_k -> nodes[i] ->is_down() ||  cluster_k -> nodes[i] ->is_offline() || cluster_k -> nodes[i] ->is_notusable())
        {
        continue;
        }

      if ((gap -> nodes_memory[num_nodese_online] = (plan_gap_mem*)malloc(sizeof(plan_gap_mem))) == NULL)
        {
        perror("Memory Allocation Error");
        return NULL;
        }

      gap -> nodes_memory[num_nodese_online] -> ninfo = cluster_k -> nodes[i];
      gap -> nodes_memory[num_nodese_online] -> num_cpus = cluster_k -> nodes[i] -> get_cores_total();
      gap -> nodes_memory[num_nodese_online] -> available_mem = get_node_mem(cluster_k -> nodes[i]);
      gap -> nodes_memory[num_nodese_online] -> available_scratch_local = get_node_scratch_local(cluster_k -> nodes[i]);

      num_nodese_online++;

      }

    gap -> num_nodes = num_nodese_online;

    if (num_nodese_online == 0)
      {
      free(gap -> nodes_memory);
      gap -> nodes_memory = NULL;
      }
    else
      {
      if ((gap -> nodes_memory = (plan_gap_mem**)realloc(gap -> nodes_memory, gap -> num_nodes * sizeof(plan_gap_mem*))) == NULL)
        {
        perror("Memory Allocation Error");
        return NULL;
        }
      }

    }

  return (void*) gap;
  }

int check_node_in_gap(plan_gap* gap, node_info* ninfo, int req_usage, sch_resource_t req_mem, sch_resource_t req_scratch_local)
  {
  int res = 0;
  int i;

  i = 0;
  while (i < gap -> num_nodes && gap -> nodes_memory[i] -> ninfo != ninfo)
    i++;

  //uzel v dire neni
  if (i == gap -> num_nodes)
    return 0;

  res = gap -> nodes_memory[i] -> num_cpus / req_usage;

  if (req_mem > 0 && res > gap -> nodes_memory[i] -> available_mem / req_mem)
    res = gap -> nodes_memory[i] -> available_mem / req_mem;

  if (req_scratch_local > 0 && res > gap -> nodes_memory[i] -> available_scratch_local / req_scratch_local)
    res = gap -> nodes_memory[i] -> available_scratch_local / req_scratch_local;

  return res;
  }

int add_gap_to_considered(int num_considered_gaps, plan_gap* gap)
  {
  num_considered_gaps++;
  if (num_considered_gaps == 1)
    {
    if (considered_gaps != NULL)
      free(considered_gaps);
          
    if ((considered_gaps = (plan_gap**)malloc(num_considered_gaps*sizeof(plan_gap*))) == NULL)
      {
      perror("Memory Allocation Error");
      return 0;
      }
    }
  else
    {
    if ((considered_gaps = (plan_gap**)realloc(considered_gaps,num_considered_gaps*sizeof(plan_gap*))) == NULL)
      {
      perror("Memory Allocation Error");
      return 0;
      }
    }
  considered_gaps[num_considered_gaps-1]=gap;

  return num_considered_gaps;
  }

int check_considered_gaps(int num_considered_gaps, plan_job* job, sched* schedule, int k)
  {
  update_limits_values(schedule, k);
  for (int i=0; i<num_considered_gaps; i++)
    if (!limits_is_gap_ok(schedule -> limits[k], job, considered_gaps[i], schedule->clusters[k]))
      return 0;

  return 1;
  }

plan_gap* find_first_gap(sched* schedule, int k, plan_job* job, bool fix_job)
  {
  int plan_num_nodes;
  int tmp_max_nodes_ppn;
  int tmp_max_ppn;
  int curr_node;
  unsigned int gap_ok;
  
  if (considered_gaps != NULL)
    free(considered_gaps);

  considered_gaps=NULL;
  int num_considered_gaps = 0;

  time_t duration;
  plan_gap* current_gap;
  plan_gap* next_gap;
  node_info* considered_node;
  
  Resource *res;

    schedule -> gaps[k] -> current = NULL;
  while (list_get_next(schedule -> gaps[k]) != NULL)
    {
    if (num_considered_gaps > 0)
      {
      free(considered_gaps);
      considered_gaps = NULL;
      }
        
    num_considered_gaps = 0;
    current_gap=(plan_gap*)schedule -> gaps[k] -> current;

    if (current_gap -> usage < job -> usage)
      continue;

    for (int j = 0; j < job -> req_num_nodes; j++)
      if (job -> fixed_nname_arr[j] != NULL)
        {
        free(job -> fixed_nname_arr[j]);
        job -> fixed_nname_arr[j] = NULL;
        }

    plan_num_nodes = 0;
    tmp_max_nodes_ppn = 0;

    curr_node = 0;
    while (curr_node < current_gap -> num_nodes)
      {
		/*
      if (job->jinfo->is_exclusive && current_gap ->nodes_memory[curr_node]->ninfo->get_cores_total() >= job->req_ppn){
    	 job->original_req_ppn = job->req_ppn;
    	 job->req_ppn=current_gap -> nodes_memory[curr_node]->ninfo->get_cores_total();
         }
     */
      while (curr_node < current_gap -> num_nodes &&
            ((current_gap -> nodes_memory[curr_node]->ninfo ->is_down() ||  current_gap -> nodes_memory[curr_node]->ninfo ->is_offline() || current_gap -> nodes_memory[curr_node]->ninfo ->is_notusable())||
            (current_gap -> nodes_memory[curr_node]->ninfo->get_avail_before() != 0 && current_gap -> nodes_memory[curr_node]->ninfo->get_avail_before() < (current_gap->start_time + job ->estimated_processing))||
            current_gap -> nodes_memory[curr_node] -> num_cpus < job -> req_ppn ||
            current_gap -> nodes_memory[curr_node] -> available_mem < job -> req_mem ||
            (current_gap -> nodes_memory[curr_node] -> available_scratch_local > -1 && current_gap -> nodes_memory[curr_node] -> available_scratch_local < job -> req_scratch_local ) ||
            (get_walltime_limit_min(current_gap -> nodes_memory[curr_node]->ninfo->get_phys_prop()) != 0 && get_walltime_limit_min(current_gap -> nodes_memory[curr_node]->ninfo->get_phys_prop()) > job->estimated_processing   ) ||
            (get_walltime_limit_max(current_gap -> nodes_memory[curr_node]->ninfo->get_phys_prop()) != 0 && get_walltime_limit_max(current_gap -> nodes_memory[curr_node]->ninfo->get_phys_prop()) < job->estimated_processing   )))
        curr_node++;
          
/*
      if (job->jinfo->is_exclusive && current_gap ->nodes_memory[curr_node]->ninfo->get_cores_total() >= job->req_ppn){
    	 job->req_ppn=job->original_req_ppn;
    	 job->original_req_ppn = 0;
         }
*/
      if (curr_node >= current_gap -> num_nodes)
        {
        break; //skok do hlavniho while jdu na dalsi diru
        }
          
      if (job->req_gpu > 0)
        {
        res = current_gap -> nodes_memory[curr_node]->ninfo->get_resource("gpu");
        if (res == NULL || job->req_gpu > res->get_capacity())
          {
          curr_node++;
          continue;
          }
        }          
 
      duration = current_gap -> duration;

      considered_node = current_gap -> nodes_memory[curr_node] -> ninfo;

      //kolikrad se tam req_ppn vleze? (uz vim ze aspon jednou)

      tmp_max_nodes_ppn = check_node_in_gap(current_gap, considered_node, job -> req_ppn, job -> req_mem, job -> req_scratch_local);

      if (job->jinfo->is_exclusive() && tmp_max_nodes_ppn>0)
        tmp_max_nodes_ppn = 1;

      gap_ok = 1;

      num_considered_gaps=add_gap_to_considered(num_considered_gaps,current_gap);

      if (duration < job -> estimated_processing)
        {
        schedule -> gaps[k] -> current = (void*)current_gap;
        while (duration < job -> estimated_processing && list_get_next(schedule -> gaps[k]) != NULL)
          {
          next_gap=(plan_gap*)schedule -> gaps[k] -> current;

          if (next_gap -> start_time != next_gap -> predeccessor -> end_time)
            {
            gap_ok = 0;
            break;
            }

          tmp_max_ppn = check_node_in_gap(next_gap, considered_node, job -> req_ppn, job -> req_mem, job -> req_scratch_local);

          if (job->jinfo->is_exclusive() && tmp_max_ppn>0)
            tmp_max_ppn = 1;

          if (tmp_max_ppn == 0)
            {
            gap_ok = 0;
            break;
            }

          if (tmp_max_ppn < tmp_max_nodes_ppn)
            tmp_max_nodes_ppn = tmp_max_ppn;

          if (next_gap -> duration == LONG_MAX)
            {
            duration = LONG_MAX;
            }
          else
            {
            duration += next_gap -> duration;
            }

          if (duration >= job -> estimated_processing)
            {
            plan_num_nodes +=tmp_max_nodes_ppn;
            if (plan_num_nodes > job -> req_num_nodes)
              plan_num_nodes = job -> req_num_nodes;

            if (fix_job)
              for (int j = 0; j < plan_num_nodes; j++)
                if (job -> fixed_nname_arr[j] == NULL)
                  job -> fixed_nname_arr[j] = strdup(considered_node -> get_name());
            }

          num_considered_gaps=add_gap_to_considered(num_considered_gaps,next_gap);

          if (duration >= job -> estimated_processing && plan_num_nodes == job -> req_num_nodes)
            {
            if (check_considered_gaps(num_considered_gaps, job, schedule, k))
              {
              return current_gap;
              }
            else
              {
              continue;
              }
            }
          }

        if (next_gap != NULL && next_gap -> start_time != next_gap -> predeccessor -> end_time)
          {
          gap_ok = 0;
          break;//dira nenavezuje jdu na dalsi diru od zacatku
          }
        }
      else
        {
        plan_num_nodes += tmp_max_nodes_ppn;
        if (plan_num_nodes > job -> req_num_nodes)
          plan_num_nodes = job -> req_num_nodes;

        if (fix_job)
          for (int j = 0; j < plan_num_nodes; j++)
            if (job -> fixed_nname_arr[j] == NULL)
              job -> fixed_nname_arr[j] = strdup(considered_node -> get_name());
        }

      if (gap_ok && duration >= job -> estimated_processing && plan_num_nodes >= job -> req_num_nodes)
        if (check_considered_gaps(num_considered_gaps, job, schedule, k))
          return current_gap;
	  //zkus dalsi uzel v dire
    
      curr_node++;
      }

    /*vratime i=i aby mohl vnejsi while korektne pokracovat*/
    schedule -> gaps[k] -> current = (void*)current_gap;
    }

  return NULL;
  }


int job_put_in_gap(sched* schedule, int k, plan_job* job, plan_gap* gap)
  {
  if (job -> req_ppn > gap -> usage)
    return 1;

  job -> start_time = gap -> start_time;
  job -> completion_time = job -> start_time + job -> estimated_processing;

  if ( gap -> following_job != NULL)
    {
    list_add_infrontof(schedule -> jobs[k], gap -> following_job, (void*)job);
    }
  else
    {
    list_add_end(schedule -> jobs[k], (void*)job);
    }

  return 0;
  }

plan_gap* gap_duplicate(plan_gap *gap)
  {
  plan_gap* gap_2;

  if ((gap_2 = (plan_gap*) malloc(sizeof(plan_gap))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  gap_2 -> start_time = gap -> start_time;
  gap_2 -> end_time = gap -> end_time;
  gap_2 -> duration = gap -> duration;
  gap_2 -> following_job = gap -> following_job;
  gap_2 -> usage = gap -> usage;
  gap_2 -> predeccessor = NULL;
  gap_2 -> successor = NULL;
  gap_2 -> num_nodes = gap -> num_nodes;
  if ((gap_2 -> nodes_memory = (plan_gap_mem**)malloc(gap_2 -> num_nodes * sizeof(plan_gap_mem*))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  for (int i = 0;i < gap_2 -> num_nodes; i++)
    {
    if ((gap_2 -> nodes_memory[i] = (plan_gap_mem*)malloc(sizeof(plan_gap_mem))) == NULL)
      {
      perror("Memory Allocation Error");
      return NULL;
      }

    gap_2 -> nodes_memory[i] -> available_mem = gap -> nodes_memory[i] -> available_mem;
    gap_2 -> nodes_memory[i] -> available_scratch_local = gap -> nodes_memory[i] -> available_scratch_local;
    gap_2 -> nodes_memory[i] -> num_cpus = gap -> nodes_memory[i] -> num_cpus;
    gap_2 -> nodes_memory[i] -> ninfo = gap -> nodes_memory[i] -> ninfo;
    }

  return gap_2;
  }


void* insert_new_gap(plan_list* list, plan_gap* last_gap, plan_gap *gap_new)
  {
  if (list == NULL)
    return NULL;

  plan_gap* gap_copy;
  plan_gap* gap_previouse;

  if (last_gap == NULL)
    last_gap = (plan_gap*)list -> last;

  if (last_gap == NULL)
    {
    gap_copy = gap_duplicate(gap_new);
    return list_add_end(list, (void*)gap_copy);
    }

  if (last_gap -> end_time <= gap_new -> start_time)
    {
    if (last_gap -> end_time == gap_new -> start_time)
      adjust_gap_memory(gap_new,last_gap);

    gap_copy = gap_duplicate(gap_new);
    return list_add_end(list, (void*)gap_copy);
    }

  while (1)
    {
    gap_previouse = (plan_gap*)get_predeccessor((void*)last_gap,Gaps);

    if (gap_previouse == NULL)
      break;

    if (gap_previouse -> end_time <= gap_new -> start_time)
      break;

    last_gap = gap_previouse;
    }

  //dira se vleze pred "posledni" diru, sup tam s ni
  if (gap_new -> end_time <= last_gap -> start_time)
    {
    gap_copy = gap_duplicate(gap_new);

    return list_add_infrontof(list, last_gap, gap_copy);
    }

  //dira zacina pred "posledni" dirou
  if (last_gap -> start_time > gap_new -> start_time)
    {
    gap_copy = gap_duplicate(gap_new);

    gap_copy -> end_time = last_gap -> start_time;
    gap_copy -> duration = gap_copy -> end_time - gap_copy -> start_time;

    list_add_infrontof(list, last_gap, gap_copy);

    gap_new -> start_time = last_gap -> start_time;
    gap_new -> duration = gap_new -> end_time - gap_new -> start_time;
    }

  //dira zacina za zacatkem "posledni" diry
  if (last_gap -> start_time < gap_new -> start_time)
    {
    gap_copy = gap_duplicate(last_gap);

    gap_copy -> end_time = gap_new -> start_time;
    gap_copy -> duration = gap_copy -> end_time - gap_copy -> start_time;

    last_gap -> start_time = gap_new -> start_time;
    last_gap -> duration = last_gap -> end_time - last_gap -> start_time;

    list_add_infrontof(list, last_gap, gap_copy);
    }

  //jsem-li zde, tak start posledni diry je stejnej jako start novy diry
  if (last_gap -> start_time != gap_new -> start_time)
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "error", "gap inserting");

  if (last_gap -> end_time == gap_new -> end_time)
    {
    add_mem_gap_to_gap(last_gap,gap_new);

    return last_gap;
    }

  if (gap_new -> end_time < last_gap -> end_time)
    {
    gap_copy = gap_duplicate(last_gap);

    gap_copy -> start_time = gap_new -> end_time;
    gap_copy -> duration = gap_copy -> end_time - gap_copy -> start_time;

    last_gap -> end_time = gap_new -> end_time;
    last_gap -> duration = last_gap -> end_time - last_gap -> start_time;

    list_add_inbackof(list, last_gap,gap_copy );

    add_mem_gap_to_gap(last_gap,gap_new);
    }


  if (last_gap -> end_time < gap_new -> end_time)
    {
    add_mem_gap_to_gap(last_gap,gap_new);

    //jestli nova dira konci az za toutodirou pozor na pamet!
    //prepocitame nove hodnoty pro pamet

    adjust_gap_memory(gap_new,last_gap);

    gap_new -> start_time = last_gap -> end_time;
    gap_new -> duration = gap_new -> end_time - gap_new -> start_time;

    return insert_new_gap(list, last_gap -> successor, gap_new);
    }

  return last_gap;
  }
