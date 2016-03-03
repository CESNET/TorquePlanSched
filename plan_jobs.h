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

#ifndef PLAN_JOBS_H
#define PLAN_JOBS_H

/*
 * create new job
 */
plan_job* job_create();

/*
 * init job with some values
 */
void* job_fillin(plan_job* job, JobInfo *assigned_job, time_t start_time, time_t due_date);

/*
 * add running jobs to the plan - before any other job
 */
int add_already_running_jobs(sched* schedule, JobInfo** new_jobs);

/*
 * try to find first gap for each new job
 */
int try_to_schedule_new_jobs(int pbs_sd, server_info *p_info, sched* schedule, JobInfo** new_jobs, time_t time_now);

/*
 * run jobs according to the plan
 */
int run_jobs(sched* schedule, int sd, time_t time_now);

/*
 * create nodespec
 */
int job_create_nodespec(plan_job* job);

#endif
