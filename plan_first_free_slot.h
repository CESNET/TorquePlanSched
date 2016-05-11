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

#ifndef PLAN_FIRST_FREE_SLOT_H
#define PLAN_FIRST_FREE_SLOT_H

#include "plan_data_types.h"

/*
 * first_free_slot is structure used for following profile of the plan during plan_update
 */
first_free_slot** first_free_slots_create(plan_cluster* plan_cluster);

/*
 * first_free_slots - initialization before update_plan
 */
int first_free_slots_initialize(plan_cluster* cluster_k, first_free_slot **first_free_slots,time_t time);

/*
 * update first_free_slots during update_plan
 */
void first_free_slots_update(plan_job* job, first_free_slot **first_free_slots);
void first_free_slots_update_exclusive(plan_job* job, int num_first_free_slot, first_free_slot **first_free_slots);

#endif
