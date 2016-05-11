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

#ifndef PLAN_GAPS_MEMORY_H
#define PLAN_GAPS_MEMORY_H

/*
 * get available memory of a node
 */
sch_resource_t get_node_mem(node_info *ninfo);

/*
 * get available scratch of a node
 */
sch_resource_t get_node_scratch_local(node_info *ninfo);

/*
 * get available scratch of a node
 */
sch_resource_t get_node_scratch_ssd(node_info *ninfo);

/*
 * not used, mam structure can be sorted
 */
int adjust_mem_gap_position(plan_gap_mem** nodes_memory, int num_nodes,int position_to_adjust);

/*
 * create memory structure
 */
plan_gap_mem** gap_memory_create(plan_gap* gap, plan_job* job, first_free_slot **first_free_slots);

/*
 * create memory structure
 */
plan_gap_mem** gap_memory_create_last(plan_gap* gap, int num_first_free_slot, first_free_slot **first_free_slots);

/*
 * add new memory record to the gap
 */
void* add_mem_gap_to_gap(plan_gap *gap_target, plan_gap *gap_source);

/*
 * adjust gap memory when the surrounding changed
 */
void adjust_gap_memory(plan_gap *gap_new,plan_gap *last_gap);


#endif
