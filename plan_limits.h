/*Copyright 2014 Václav Chlumský
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

#ifndef PLAN_LIMITS_H
#define PLAN_LIMITS_H

/*
 * if account alredy know get its id
 * if accoutn unknown assign new id to the account and get the id
 */
int set_and_get_account_id(char* account);

/*
 * create new limit, only one limit structure for each time interval exists
 */
plan_limits* limit_create(time_t timestamp);

/*
 * inform the limit about a existing (running, planned) job
 */
void add_job_to_limits(plan_list* limits, plan_job* job, plan_cluster* cluster);

/*
 * check the gap suitibility for a certain job with respect to the limits
 * used during find firs gap
 */
int limits_is_gap_ok(plan_list* limits, plan_job* job, plan_gap* gap, plan_cluster* cluster);

/*
 * adujust a job start time in the plan with respect to the limits (shift job to the future)
 * used during update_plan
 */
time_t adjust_job_starttime_according_to_limits(plan_list* limits, plan_job* job, plan_cluster* cluster);

/*
 * update the limits 
 */
void update_limits_values(sched* schedule, int k);

#endif
