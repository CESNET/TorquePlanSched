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

#ifndef PLAN_EVALUATION_H
#define PLAN_EVALUATION_H

/*
 * get response time from job structure
 */
double job_response_time(plan_job* job, bool real);

/*
 * get wait time from job structure
 */
double job_wait_time(plan_job* job);

/*
 * get slowdown time from job structure
 */
double job_slowdown_time(plan_job* job, bool real);

/*
 * prepare evaluation structure for new evaluation process
 */
plan_eval* evaluation_reset(plan_eval* evaluation);

/*
 * get overall wait time, response time and slowdown
 */
plan_eval* evaluation_calc_criterion(plan_eval* evaluation);

/*
 * add job to during evaluation
 */
plan_eval* evaluation_add_job(plan_eval* evaluation, plan_job* job, bool jobfinnished);

/*
 * evaluate plan after all jobs added
 */
plan_eval evaluate_schedule(sched* schedule);

/*
 * debug log
 */
void evaluation_log(plan_eval* evaluation, const char* evaluation_description);

#endif
