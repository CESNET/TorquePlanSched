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

#ifndef PLAN_FREE_DATA_H
#define PLAN_FREE_DATA_H
/*
 * free_* - free useless structures
 * TODO - use C++
 */

void free_job(plan_job* job);

void free_gap(plan_gap* gap);

void free_limit(plan_limit* limit);

int free_list(plan_list* list_to_free);

int free_first_free_slots(first_free_slot** first_free_slots, plan_cluster* cluster_k);

int free_gap_memory(plan_gap* gap);

void free_limit_account(plan_limit* limit);

void free_server_completed_jobs(server_info *sinfo, int free_objs_too);

#endif
