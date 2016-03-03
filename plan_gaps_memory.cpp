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
#include <climits>

sch_resource_t get_node_mem(node_info *ninfo)
  {
  Scheduler::Core::Resource *mem;

  if ((mem = ninfo -> get_resource("mem")) == NULL)
    return -1;

  return mem -> get_capacity();
  }

sch_resource_t get_node_scratch_local(node_info *ninfo)
  {
  Scheduler::Core::Resource *scratch_local;

  if ((scratch_local = ninfo -> get_resource("scratch_local")) == NULL)
    return -1;

  return scratch_local -> get_capacity();
  }

int adjust_mem_gap_position(plan_gap_mem** nodes_memory, int num_nodes,int position_to_adjust)
  {
  plan_gap_mem* tmp_gap_mem;
#ifdef MEM_GAP_ASC
#undef MEM_GAP_DEC
  /*
   * asc
   */
  while (position_to_adjust > 0 && nodes_memory[position_to_adjust] -> num_cpus < nodes_memory[position_to_adjust - 1] -> num_cpus)
    {
    tmp_gap_mem = nodes_memory[position_to_adjust - 1];
    nodes_memory[position_to_adjust - 1] = nodes_memory[position_to_adjust];
    nodes_memory[position_to_adjust] = tmp_gap_mem;
    position_to_adjust--;
    }

  while (position_to_adjust < num_nodes - 1 && nodes_memory[position_to_adjust] -> num_cpus > nodes_memory[position_to_adjust + 1] -> num_cpus)
    {
    tmp_gap_mem = nodes_memory[position_to_adjust + 1];
    nodes_memory[position_to_adjust + 1] = nodes_memory[position_to_adjust];
    nodes_memory[position_to_adjust] = tmp_gap_mem;
    position_to_adjust++;
    }
#endif

#ifdef MEM_GAP_DEC
#undef MEM_GAP_ASC
  /*
   * dec
   */
  while (position_to_adjust > 0 && nodes_memory[position_to_adjust] -> num_cpus > nodes_memory[position_to_adjust - 1] -> num_cpus)
    {
    tmp_gap_mem = nodes_memory[position_to_adjust - 1];
    nodes_memory[position_to_adjust - 1] = nodes_memory[position_to_adjust];
    nodes_memory[position_to_adjust] = tmp_gap_mem;
    position_to_adjust--;
    }

  while (position_to_adjust < num_nodes - 1 && nodes_memory[position_to_adjust] -> num_cpus < nodes_memory[position_to_adjust + 1] -> num_cpus)
    {
    tmp_gap_mem = nodes_memory[position_to_adjust + 1];
    nodes_memory[position_to_adjust + 1] = nodes_memory[position_to_adjust];
    nodes_memory[position_to_adjust] = tmp_gap_mem;
    position_to_adjust++;
    }
#endif
  return 0;
  }

plan_gap_mem** gap_memory_create(plan_gap* gap, plan_job* job, int num_first_free_slot, first_free_slot **first_free_slots)
  {
  int i;
  plan_gap_mem** new_gap_mem;
  int num_nodes = 0;
  int known_node = -1;

  new_gap_mem = NULL;

  gap -> usage = 0;

  for (int j = 0; j < job -> usage; j++)
	if (job -> cpu_indexes[j] >= 0)
	if (first_free_slots[job -> cpu_indexes[j]] -> time < gap -> end_time)
	  {
	  gap -> usage++;
	  i = job -> cpu_indexes[j];

	  known_node = -1;
	  for (int k = 0; k < num_nodes; k++)
	    if (new_gap_mem[k] -> ninfo == first_free_slots[i] -> ninfo)
	    	known_node = k;

      if (known_node == -1)
        {
    	if ((new_gap_mem = (plan_gap_mem**)realloc(new_gap_mem,(num_nodes + 1) * sizeof(plan_gap_mem*))) == NULL)
          {
          perror("Memory Allocation Error");
          return NULL;
          }

    	if ((new_gap_mem[num_nodes] = (plan_gap_mem*)malloc(sizeof(plan_gap_mem))) == NULL)
          {
          perror("Memory Allocation Error");
          return NULL;
          }

    	new_gap_mem[num_nodes] -> ninfo=first_free_slots[i] -> ninfo;;
    	new_gap_mem[num_nodes] -> available_mem=first_free_slots[i] -> available_mem;
    	new_gap_mem[num_nodes] -> available_scratch_local=first_free_slots[i] -> available_scratch_local;
    	new_gap_mem[num_nodes] -> num_cpus = 1;

    	num_nodes++;

    	adjust_mem_gap_position(new_gap_mem, num_nodes,num_nodes-1);
        } else
        {
        new_gap_mem[known_node] -> available_mem=first_free_slots[i] -> available_mem;
        new_gap_mem[known_node] -> available_scratch_local=first_free_slots[i] -> available_scratch_local;
        new_gap_mem[known_node] -> num_cpus += 1;

        adjust_mem_gap_position(new_gap_mem, num_nodes,known_node);
        }
      }

  gap -> nodes_memory=new_gap_mem;
  gap -> num_nodes=num_nodes;
  return new_gap_mem;
  }

plan_gap_mem** gap_memory_create_last(plan_gap* gap, plan_job* job, int num_first_free_slot, first_free_slot **first_free_slots)
  {
  plan_gap_mem** new_gap_mem;
  int num_nodes = 0;
  int latest_cpu_index;
  int known_node = -1;

  new_gap_mem = NULL;

  gap -> usage = 0;
  latest_cpu_index = -1;

  for (int i = 0; i < num_first_free_slot; i++)
    if (first_free_slots[i] -> time < gap -> end_time)
      {
      if (first_free_slots[i] -> ninfo ->is_down() ||  first_free_slots[i] -> ninfo ->is_offline() || first_free_slots[i] -> ninfo ->is_notusable())
        continue;

	  gap -> usage++;

      known_node = -1;

	  for (int j = 0; j < num_nodes; j++)
	    if (new_gap_mem[j] -> ninfo == first_free_slots[i] -> ninfo)
	    	known_node = j;

      if (known_node == -1)
        {
    	if ((new_gap_mem = (plan_gap_mem**)realloc(new_gap_mem,(num_nodes + 1) * sizeof(plan_gap_mem*))) == NULL)
          {
          perror("Memory Allocation Error");
          return NULL;
          }

    	if ((new_gap_mem[num_nodes] = (plan_gap_mem*)malloc(sizeof(plan_gap_mem))) == NULL)
          {
          perror("Memory Allocation Error");
          return NULL;
          }

    	new_gap_mem[num_nodes] -> ninfo = first_free_slots[i] -> ninfo;;
    	new_gap_mem[num_nodes] -> available_mem = first_free_slots[i] -> available_mem;
    	new_gap_mem[num_nodes] -> available_scratch_local = first_free_slots[i] -> available_scratch_local;
    	new_gap_mem[num_nodes] -> num_cpus = 1;

    	num_nodes++;

    	adjust_mem_gap_position(new_gap_mem, num_nodes,num_nodes-1);
        } else
        {
        new_gap_mem[known_node] -> available_mem = first_free_slots[i] -> available_mem;
        new_gap_mem[known_node] -> available_scratch_local = first_free_slots[i] -> available_scratch_local;
        new_gap_mem[known_node] -> num_cpus += 1;

        adjust_mem_gap_position(new_gap_mem, num_nodes,known_node);
        }
      }

  gap -> nodes_memory = new_gap_mem;
  gap -> num_nodes = num_nodes;

  return new_gap_mem;
  }

void* add_mem_gap_to_gap(plan_gap *gap_target, plan_gap *gap_source)
  {
  int j;
  gap_target -> usage += gap_source -> usage;
  for (int i = 0; i < gap_source -> num_nodes; i++)
    {
	j=0;
	while (j < gap_target -> num_nodes && gap_source -> nodes_memory[i] -> ninfo != gap_target -> nodes_memory[j] -> ninfo)
	  j++;

	if (j < gap_target -> num_nodes)
	  {
	  gap_target -> nodes_memory[j] -> num_cpus += gap_source -> nodes_memory[i] -> num_cpus;

	  adjust_mem_gap_position(gap_target -> nodes_memory, gap_target -> num_nodes, j);
	  } else
	  {
	  gap_target -> num_nodes=++j;

	  if ((gap_target -> nodes_memory = (plan_gap_mem**)realloc(gap_target -> nodes_memory, j * sizeof(plan_gap_mem*))) == NULL)
        {
        perror("Memory Allocation Error");
        return NULL;
        }

	  if ((gap_target -> nodes_memory[j - 1] = (plan_gap_mem*)malloc(sizeof(plan_gap_mem))) == NULL)
        {
        perror("Memory Allocation Error");
        return NULL;
        }

	  gap_target -> nodes_memory[j - 1] -> ninfo = gap_source -> nodes_memory[i] -> ninfo;
	  gap_target -> nodes_memory[j - 1] -> available_mem = gap_source -> nodes_memory[i] -> available_mem;
	  gap_target -> nodes_memory[j - 1] -> available_scratch_local = gap_source -> nodes_memory[i] -> available_scratch_local;
	  gap_target -> nodes_memory[j - 1] -> num_cpus = gap_source -> nodes_memory[i] -> num_cpus;

	  adjust_mem_gap_position(gap_target -> nodes_memory, gap_target -> num_nodes, j-1);
	  }
    }

  return gap_target;
  }

void adjust_gap_memory(plan_gap *gap_new,plan_gap *last_gap)
  {
  plan_job* neighbour_job;
  int j;
  if (last_gap -> following_job==NULL)
    return;

  neighbour_job=last_gap -> following_job;

  //nastavime plnou pamet dire
  for (int i = 0; i < gap_new -> num_nodes; i++)
	gap_new -> nodes_memory[i] -> updated=0;

  //najdem prvni job v tomto case
  while (neighbour_job -> predeccessor != NULL &&
		 neighbour_job -> predeccessor -> start_time == neighbour_job -> start_time)
    neighbour_job = neighbour_job -> predeccessor;

  //pamet vsech jobu v tomto case na uzlu kde je tato dira bude odebrana z cele pameti uzlu
  while (neighbour_job != NULL)
    {
	for (int i = 0; i < gap_new -> num_nodes; i++)
	  for (int j = 0; j < neighbour_job -> req_num_nodes; j++)
	    if (gap_new -> nodes_memory[i] -> ninfo == neighbour_job -> ninfo_arr[j])
	      {
		  //pokud jsem tu jeste nebyl tak nastavim plnou pamet a od ty budu odecitat
		  if (gap_new -> nodes_memory[i] -> updated == 0)
		    {
		    gap_new -> nodes_memory[i] -> available_mem = get_node_mem(gap_new -> nodes_memory[i] -> ninfo);
		    gap_new -> nodes_memory[i] -> available_scratch_local = get_node_scratch_local(gap_new -> nodes_memory[i] -> ninfo);
		    gap_new -> nodes_memory[i] -> updated = 1;
		    }

		  gap_new -> nodes_memory[i] -> available_mem -= neighbour_job -> req_mem;
		  //gap_new -> nodes_memory[i] -> available_scratch_local -= neighbour_job -> req_scratch_local;
	      }

	if (neighbour_job -> successor != NULL && neighbour_job -> start_time == neighbour_job -> successor -> start_time)
	  {
	  neighbour_job = neighbour_job -> successor;
	  } else
	  {
	  neighbour_job=NULL;
	  }
    }
  }

