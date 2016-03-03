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
#include "check.h"

#include "plan_schedule.h"
#include "plan_gaps_memory.h"

first_free_slot** first_free_slots_create(plan_cluster* cluster_k)
  {
  int counter = 0;
  sch_resource_t mem;
  sch_resource_t scratch_local;
  first_free_slot** first_free_slots;

  if ((first_free_slots = (first_free_slot**) malloc(cluster_k -> num_cpus * sizeof(first_free_slot*))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  for (int i= 0; i < cluster_k -> num_cpus;i++)
    {
	if ((first_free_slots[i] = (first_free_slot*) malloc(sizeof(first_free_slot))) == NULL)
      {
      perror("Memory Allocation Error");
      return NULL;
      }

	first_free_slots[i] -> time = -1;
    }

  for (int i = 0; i < cluster_k -> num_nodes; i++)
    {
	mem = get_node_mem(cluster_k -> nodes[i]);
	scratch_local = get_node_scratch_local(cluster_k -> nodes[i]);

	if ((first_free_slots[counter] -> releated_cpus = (int*) malloc(cluster_k -> nodes[i] -> get_cores_total() * sizeof(int))) == NULL)
      {
      perror("Memory Allocation Error");
      return NULL;
      }

	for (int j = 0; j < cluster_k -> nodes[i] -> get_cores_total(); j++)
	  {
	  if (j > 0)
		  first_free_slots[counter] -> releated_cpus = first_free_slots[counter - 1] -> releated_cpus;

	  first_free_slots[counter] -> releated_cpus[j] = counter;
	  first_free_slots[counter] -> ninfo = cluster_k -> nodes[i];
	  first_free_slots[counter] -> mem = mem;
	  first_free_slots[counter] -> scratch_local = scratch_local;
	  first_free_slots[counter] -> available_mem = mem;
	  first_free_slots[counter] -> available_scratch_local = scratch_local;
	  first_free_slots[counter] -> mem_used_per_cpu = 0;
	  first_free_slots[counter] -> scratch_local_used_per_cpu = 0;

	  counter += 1;
	  }
	}
  return first_free_slots;
  }

int first_free_slots_initialize(plan_cluster* cluster_k, first_free_slot **first_free_slots,time_t time)
  {
  int counter;
  time_t job_start_time;
  time_t job_duration;

  sch_resource_t mem_amount;
  sch_resource_t scratch_local_amount;

  struct resource *mem;
  struct resource *scratch_local;
  int job_usage;
  int running_cpu = cluster_k -> num_cpus;

  counter = 0;

  for (int i = 0; i < cluster_k -> num_nodes; i++)
    {
	if (cluster_k -> nodes[i] ->is_down() ||  cluster_k -> nodes[i] ->is_offline() || cluster_k -> nodes[i] ->is_notusable())
      {
	  for (int j = 0; j < cluster_k -> nodes[i] -> get_cores_total(); j++)
	    {
		first_free_slots[counter] -> free_to_run = 0;
		first_free_slots[counter] -> time = -1;
		first_free_slots[counter] -> ninfo-> walltime_limit_min = get_walltime_limit_min(first_free_slots[counter] -> ninfo->get_phys_prop());
		first_free_slots[counter] -> ninfo-> walltime_limit_max = get_walltime_limit_max(first_free_slots[counter] -> ninfo->get_phys_prop());
	    first_free_slots[counter] -> mem_used_per_cpu = 0;
	    first_free_slots[counter] -> scratch_local_used_per_cpu = 0;
		counter++;
	    }

	  running_cpu -= cluster_k -> nodes[i] -> get_cores_total();
	  continue;
      }

	//na uzlu nejsou joby
	if (cluster_k -> nodes[i] -> jobs_on_cpu == NULL)
	  {
	  for (int j = 0; j < cluster_k -> nodes[i] -> get_cores_total(); j++)
	    {
		first_free_slots[counter] -> ninfo-> walltime_limit_min = get_walltime_limit_min(first_free_slots[counter] -> ninfo->get_phys_prop());
		first_free_slots[counter] -> ninfo-> walltime_limit_max = get_walltime_limit_max(first_free_slots[counter] -> ninfo->get_phys_prop());
		first_free_slots[counter] -> free_to_run = 1;
	    first_free_slots[counter] -> mem_used_per_cpu = 0;
	    first_free_slots[counter] -> scratch_local_used_per_cpu = 0;
		first_free_slots[counter] -> time = time;

		counter++;
	    }
	  continue;
	  }

	//na uzlu jsou joby
	for (int j = 0; j < cluster_k -> nodes[i] -> get_cores_total(); j++)
	  if (cluster_k -> nodes[i] -> jobs_on_cpu[j] == NULL)
	    {
		first_free_slots[counter] -> ninfo-> walltime_limit_min = get_walltime_limit_min(first_free_slots[counter] -> ninfo->get_phys_prop());
		first_free_slots[counter] -> ninfo-> walltime_limit_max = get_walltime_limit_max(first_free_slots[counter] -> ninfo->get_phys_prop());
	    first_free_slots[counter] -> time = time;
	    first_free_slots[counter] -> mem_used_per_cpu = 0;
	    first_free_slots[counter] -> scratch_local_used_per_cpu = 0;
	    first_free_slots[counter] -> free_to_run = 1;

	    counter++;
	    } else
	    {
	    first_free_slots[counter] -> ninfo-> walltime_limit_min = get_walltime_limit_min(first_free_slots[counter] -> ninfo->get_phys_prop());
	    first_free_slots[counter] -> ninfo-> walltime_limit_max = get_walltime_limit_max(first_free_slots[counter] -> ninfo->get_phys_prop());
	    //updatuju do first_free_slot kdy job skonci
	    job_start_time = cluster_k -> nodes[i] -> jobs_on_cpu[j] -> stime;
	    job_duration = find_resource_req(cluster_k -> nodes[i] -> jobs_on_cpu[j] -> resreq, "walltime") -> amount;
	    first_free_slots[counter] -> time = job_start_time + job_duration;
	    if (first_free_slots[counter] -> time <= time)
	    	first_free_slots[counter] -> time = time + 1;
	    first_free_slots[counter] -> free_to_run = 0;

	    //updatuju mem_used_per_cpu
	    mem_amount = find_resource_req(cluster_k -> nodes[i] -> jobs_on_cpu[j] -> resreq, "mem") -> amount;
	    scratch_local_amount = job_get_scratch_local(cluster_k -> nodes[i] -> jobs_on_cpu[j]);
	    job_usage = find_resource_req(cluster_k -> nodes[i] -> jobs_on_cpu[j]  -> resreq, "procs") -> amount;

	    first_free_slots[counter] -> mem_used_per_cpu = mem_amount / job_usage;
	    first_free_slots[counter] -> scratch_local_used_per_cpu = scratch_local_amount / job_usage;

	    counter++;
	    }

	mem_amount = 0;
	scratch_local_amount = 0;

	//procesory prave zpracovaneho uzlu jeste projdem a updatujem available_mem tam kde je to treba
	for (int j = cluster_k -> nodes[i] -> get_cores_total(); j > 0; j--)
	  {
	  mem_amount = first_free_slots[counter-j] -> mem;
	  scratch_local_amount = first_free_slots[counter-j] -> scratch_local;

	  for (int k = cluster_k -> nodes[i] -> get_cores_total(); k > 0; k--)
	    if (first_free_slots[counter - j] -> time < first_free_slots[counter - k] -> time)
	      {
	  	  mem_amount -= first_free_slots[counter - k] -> mem_used_per_cpu;
	  	  scratch_local_amount -= first_free_slots[counter - k] -> scratch_local_used_per_cpu;
	      }

	  first_free_slots[counter - j] -> available_mem = mem_amount;
	  first_free_slots[counter - j] -> available_scratch_local = scratch_local_amount;
	  }
    }

  cluster_k -> num_running_cpus = running_cpu;

  return 0;
  }



void first_free_slots_update(plan_job* job, int num_first_free_slot, first_free_slot **first_free_slots)
  {
  int mem_slot_index;
  int mem_slot_releated_index;

  int first_cpu_node;
  int cpu_index;

  sch_resource_t new_mem;
  sch_resource_t new_scratch_local, new_scratch_ssd;

  // j  udava kolikatej uzel updatujeme, pokud je na jeden uzel royvreno vice ppn, pak ho updatujeme vicerkat
  for (int j = 0; j < job -> req_num_nodes; j++)
    {
    //first_cpu_node = job -> cpu_indexes[j*job->req_ppn];

    first_cpu_node = job -> latest_ncpu_index[j];

    //****zacatek updatovani pro jeden uzel

    new_mem = first_free_slots[first_cpu_node] -> mem;
    new_scratch_local = first_free_slots[first_cpu_node] -> scratch_local;

    //inicializace oznaceni vsech cpu patricich uzlu
    for (int i = 0; i < first_free_slots[first_cpu_node] -> ninfo -> get_cores_total(); i++)
      {
	  cpu_index = first_free_slots[first_cpu_node] -> releated_cpus[i];
	  first_free_slots[cpu_index] -> updated = -1;
      }

  //-1 inicializace
  // 0 cpu patri teto uloze
  // 1 cpu je uvolneno pred zacatkem ulohy, nezajimave
  // 2 cpu je uvolnene od doby startu ulohy (vcetne) do dobz pred koncem ulohy
  // 3 cpu je uvolneno ve stejne dobe jako je dokoncena uloha
  // 4 cpu je uvolnene po konci ulohy (vcetne)
  //

    //updatuju completion_time tohoto job v first_free_slot a oznacim ktery cpu patri jobu
    for (int i = 0; i < job -> usage; i++)
      {
  	  first_free_slots[job -> cpu_indexes[i]] -> time = job -> completion_time;
      first_free_slots[job -> cpu_indexes[i]] -> updated = 0;
      }

    //vsechny cpu ktere nepatri jobu rozdelim na 1,2,3
    for (int i = 0; i < first_free_slots[first_cpu_node] -> ninfo -> get_cores_total(); i++)
      {
	  cpu_index = first_free_slots[first_cpu_node] -> releated_cpus[i];

	  if (first_free_slots[cpu_index] -> updated == 0)
        continue;

	  if (first_free_slots[cpu_index] -> time < job -> start_time)
	    first_free_slots[cpu_index] -> updated = 1;

	  if (first_free_slots[cpu_index] -> time >= job -> start_time)
	    first_free_slots[cpu_index] -> updated = 2;

	  if (first_free_slots[cpu_index] -> time == job -> completion_time)
	    first_free_slots[cpu_index] -> updated = 3;

	  if (first_free_slots[cpu_index]->time > job -> completion_time)
	    first_free_slots[cpu_index] -> updated = 4;
      }

    //vsem 2 odeberu pamet jobu
    //a za vsechnz 4 odectup pamet used_per_cpu
    for (int i = 0; i < first_free_slots[first_cpu_node] -> ninfo -> get_cores_total(); i++)
      {
	  cpu_index = first_free_slots[first_cpu_node] -> releated_cpus[i];

	  if (first_free_slots[cpu_index] -> updated == 2)
	    {
	    first_free_slots[cpu_index] -> available_mem -= job -> req_mem;
	    //first_free_slots[cpu_index] -> available_scratch_local -= job -> req_scratch_local;
	    }

	  if (first_free_slots[cpu_index] -> updated==4)
	    {
	    new_mem -= first_free_slots[cpu_index] -> mem_used_per_cpu;
	    new_scratch_local -= first_free_slots[cpu_index] -> scratch_local_used_per_cpu;
	    }
      }

    //vsem 0 a 3 nastavim novou pamet
    for (int i = 0; i < first_free_slots[first_cpu_node] -> ninfo -> get_cores_total(); i++)
      {
 	  cpu_index = first_free_slots[first_cpu_node] -> releated_cpus[i];

 	  if (first_free_slots[cpu_index] -> updated == 0 || first_free_slots[cpu_index] -> updated == 3)
 	    {
 		first_free_slots[cpu_index] -> available_mem = new_mem;
 		first_free_slots[cpu_index] -> available_scratch_local = new_scratch_local;
 		first_free_slots[cpu_index] -> mem_used_per_cpu = job -> req_mem / job -> req_ppn;
 		first_free_slots[cpu_index] -> scratch_local_used_per_cpu = job -> req_scratch_local / job -> req_ppn;
 	    }
      }
    //****konec updatovani pro jeden uzel
    }
  }

void first_free_slots_update_exclusive(plan_job* job, int num_first_free_slot, first_free_slot **first_free_slots)
  {
  int mem_slot_index;
  int mem_slot_releated_index;

  int first_cpu_node;
  int cpu_index;

  sch_resource_t new_mem;
  sch_resource_t new_scratch_local, new_scratch_ssd;

  // j  udava kolikatej uzel updatujeme, pokud je na jeden uzel royvreno vice ppn, pak ho updatujeme vicerkat
  for (int j = 0; j < job -> req_num_nodes; j++)
    {
    //first_cpu_node = job -> cpu_indexes[j*job->req_ppn];

    for (int k = 0; k < num_first_free_slot; k++)
    	  if (first_free_slots[k]->ninfo == job->ninfo_arr[j])
    	  {
    	  first_cpu_node = k;
    	  break;
    	  }

    //****zacatek updatovani pro jeden uzel

    new_mem = first_free_slots[first_cpu_node] -> mem;
    new_scratch_local = first_free_slots[first_cpu_node] -> scratch_local;

    //inicializace oznaceni vsech cpu patricich uzlu
    for (int i = 0; i < first_free_slots[first_cpu_node] -> ninfo -> get_cores_total(); i++)
      {
	  cpu_index = first_free_slots[first_cpu_node] -> releated_cpus[i];
	  first_free_slots[cpu_index] -> updated = -1;
      }

  //-1 inicializace
  // 0 cpu patri teto uloze
  // 1 cpu je uvolneno pred zacatkem ulohy, nezajimave
  // 2 cpu je uvolnene od doby startu ulohy (vcetne) do dobz pred koncem ulohy
  // 3 cpu je uvolneno ve stejne dobe jako je dokoncena uloha
  // 4 cpu je uvolnene po konci ulohy (vcetne)
  //

    //updatuju completion_time tohoto job v first_free_slot a oznacim ktery cpu patri jobu
    for (int i = 0; i < job ->req_num_nodes; i++)
      for (int j = 0; j < num_first_free_slot; j++)
      if (job->ninfo_arr[i] == first_free_slots[j]->ninfo)
      {
  	  first_free_slots[j] -> time = job -> completion_time;
      first_free_slots[j] -> updated = 0;
      }

    //vsechny cpu ktere nepatri jobu rozdelim na 1,2,3
    for (int i = 0; i < first_free_slots[first_cpu_node] -> ninfo -> get_cores_total(); i++)
      {
	  cpu_index = first_free_slots[first_cpu_node] -> releated_cpus[i];

	  if (first_free_slots[cpu_index] -> updated == 0)
        continue;

	  if (first_free_slots[cpu_index] -> time < job -> start_time)
	    first_free_slots[cpu_index] -> updated = 1;

	  if (first_free_slots[cpu_index] -> time >= job -> start_time)
	    first_free_slots[cpu_index] -> updated = 2;

	  if (first_free_slots[cpu_index] -> time == job -> completion_time)
	    first_free_slots[cpu_index] -> updated = 3;

	  if (first_free_slots[cpu_index]->time > job -> completion_time)
	    first_free_slots[cpu_index] -> updated = 4;
      }


    //vsem 2 odeberu pamet jobu
    //a za vsechnz 4 odectup pamet used_per_cpu
    for (int i = 0; i < first_free_slots[first_cpu_node] -> ninfo -> get_cores_total(); i++)
      {
	  cpu_index = first_free_slots[first_cpu_node] -> releated_cpus[i];

	  if (first_free_slots[cpu_index] -> updated == 2)
	    {
	    first_free_slots[cpu_index] -> available_mem -= job -> req_mem;
	    //first_free_slots[cpu_index] -> available_scratch_local -= job -> req_scratch_local;
	    }

	  if (first_free_slots[cpu_index] -> updated==4)
	    {
	    new_mem -= first_free_slots[cpu_index] -> mem_used_per_cpu;
	    new_scratch_local -= first_free_slots[cpu_index] -> scratch_local_used_per_cpu;
	    }
      }

    //vsem 0 a 3 nastavim novou pamet
    for (int i = 0; i < first_free_slots[first_cpu_node] -> ninfo -> get_cores_total(); i++)
      {
 	  cpu_index = first_free_slots[first_cpu_node] -> releated_cpus[i];

 	  if (first_free_slots[cpu_index] -> updated == 0 || first_free_slots[cpu_index] -> updated == 3)
 	    {
 		first_free_slots[cpu_index] -> available_mem = new_mem;
 		first_free_slots[cpu_index] -> available_scratch_local = new_scratch_local;
 		first_free_slots[cpu_index] -> mem_used_per_cpu = job -> req_mem / job -> req_ppn;
 		first_free_slots[cpu_index] -> scratch_local_used_per_cpu = job -> req_scratch_local / job -> req_ppn;
 	    }
      }
    //****konec updatovani pro jeden uzel
    }
  }


