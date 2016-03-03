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

#ifndef PLAN_LOG_H
#define PLAN_LOG_H

/*
 * log for debug
 */

void print_resource_str(resource_req* res);

void print_resource_val(resource_req* res);

void log_schedule(sched* sched);

void log_print_ffs(first_free_slot **first_free_slots, int num_cpu);

void log_print_ffms(first_free_slot **first_free_slots, int num_cpu);

void log_print_ffnot(int first_free_slot_not[], int num_cpu);

void log_server_jobs(server_info *sinfo);

#endif
