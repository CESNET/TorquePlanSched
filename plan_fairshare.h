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

#ifndef PLAN_FAIRSHRE_H
#define PLAN_FAIRSHRE_H

/*
 * add past jobs to fairshare
 */
void fairshare_add_job(plan_job *job, int num_running_cpus, long int available_ram);

/*
 * add plan to fairshare
 */
void fairshare_get_planned_from_schedule(sched *schedule);

/*
 * we use "max fairshare"
 */
double fairshare_result_max();
double fairshare_result_common();

/*
 * debug log
 */
void fairshare_log();

#endif
