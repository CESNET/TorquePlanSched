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

#ifndef PLAN_OPERATIONS_H
#define PLAN_OPERATIONS_H

/*
 * Update schedule - probably the most important operation
 */
int update_sched(int pbs_sd, sched* schedule, int k, time_t time);

/*
 * inform the server about planned start times
 */
int update_planned_start(int pbs_sd, sched* schedule, int k);

/*
 * inform the server about planned nodes
 */
int update_planned_nodes(int pbs_sd, sched* schedule, int k);

#endif
