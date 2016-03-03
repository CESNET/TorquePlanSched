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

#ifndef PLAN_SCHEDULE_H
#define PLAN_SCHEDULE_H

#include <set>
#include <string>

/*
 * create the main structure
 */
sched* schedule_create();

/*
 * create the main substructure - each cluster = each plan
 */
plan_cluster* cluster_create();

/*
 * add one cluster/one plan to the main structure
 */
int schedule_add_cluster(sched* sched, server_info* server, const char* cluster_name, const std::set<std::string>& properties);

/*
 * find (jluster, job) property withuin properties list
 */
int find_property(char **properties,char *property);

/*
 * check if the cluster is suitable for a ceratin job
 */
int check_cluster_suitable(server_info* p_info, plan_cluster* cl, plan_job* job);

/*
 * new world state arrived from the server 
 * inform scheduler about new/removed nodes, nodes state changes, etc..
 */
int plan_update_clusters(sched* schedule,  server_info* server);

/*
 * some nodes accept only jobs with limited walltime
 * check if a property with the limitation is present
 */
long get_walltime_limit_min(std::set<std::string> properties);
long get_walltime_limit_max(std::set<std::string> properties);

/*
 * new world state arrived from the server 
 * inform scheduler about new/completed/removed jobs
 */
JobInfo** plan_known_jobs_update(sched* schedule,  server_info* sinfo, time_t time_now);

/*
 * get requested scratch
 * TODO move the function
 */
sch_resource_t job_get_scratch_local(JobInfo *jinfo);

#endif
