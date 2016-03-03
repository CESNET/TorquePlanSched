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

#ifndef PLAN_CONFIG_H
#define PLAN_CONFIG_H

/*
 * TODO move everything to sched_config
 */

#define OPTIM_CYCLES_LIMIT (num_queued * 10)
//#define OPTIM_CYCLES_LIMIT 0

//#define OPTIM_TIMEOUT 30

//#define OPTIM_DURATION_LIMIT 5
//#define OPTIM_DURATION_LIMIT 10

//#define NEW_JOBS_LIMIT_PER_CYCLE 5000

//minimalni pocet jobu v seznamu aby se optimalizovalo, nesmi byt min nez 3
//#define OPTIM_MINIMAL_QUEUED 3

//#define LOG_PLAN_EVALUATION

#define DEFAULT_WALLTIME 86400

#endif
