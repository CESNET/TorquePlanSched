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

#ifndef PLAN_GAPS_H
#define PLAN_GAPS_H

/*
 * gap - a unused space in the plan
 */
plan_gap* gap_create();

/*
 * init gap with some values
 */
void* gap_fillin(plan_gap* gap, time_t start_time, time_t end_time, time_t duration, int usage, plan_job* following_job, plan_gap* successor, plan_gap* predeccessor);

/*
 * find earliest available space in the plan for new job, subsequent gaps can be returned
 */
plan_gap* find_first_gap(sched* schedule, int k, plan_job* job, bool fix_job);

/*
 * you have a gap for the new job - put the job in the gap
 */
int job_put_in_gap(sched* schedule, int k, plan_job* job, plan_gap* gap);

/*
 * add gap to the gap list
 */
void* insert_new_gap(plan_list* list, plan_gap* last_gap, plan_gap *gap_new);

/*
 * after update_plan, when all jobs are updated
 * this will create gaps to even out the plan with the compl_time of the last job
 */
plan_list* gap_create_not_the_last(int num_cpus, first_free_slot **first_free_slots, time_t max_completion_time);

/*
 * when the plan is evened out (with the gap_create_not_the_last)
 * this will create the last gaps, which is infinite rectangle
 */
void* gap_create_last(time_t start_time, plan_cluster* cluster_k);

#endif
