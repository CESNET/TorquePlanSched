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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "misc.h"
#include "node_info.h"

#include "plan_config.h"
#include "plan_data_types.h"
#include "plan_list_operations.h"
#include "globals.h"

using namespace std;


#define NUM_LIMIT_ARRAYS 2
#define NUM_LIMITS 8
// 2h, 4h, 1d, 2d, 4d, 1w, 2w, 2m
const int limits_tresholds[NUM_LIMITS] = { 7200, 14400, 86400, 172800, 345600, 604800, 1209600, 5356800 };

float limits_percentages[NUM_LIMIT_ARRAYS][NUM_LIMITS] = {
                { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 },
                { 0.6, 0.55, 0.5, 0.5, 0.5, 0.6, 0.5, 0.5 }
};

float limits_percentages_global[NUM_LIMIT_ARRAYS][NUM_LIMITS] = {
                { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 },
                { 0.8, 0.7, 0.6, 0.6, 0.6, 0.6, 0.6, 1.0 }
};

int account_table_size = 0;
char **account_table = {};

vector<int> limits_values;
vector<int> limits_values_global;

int set_and_get_account_id(char* account)
  {
  int i = 0;
  for (i = 0; i < account_table_size; i++)
    if (strcmp(account_table[i], account) == 0)
      return i;

  if (   (account_table = (char**) realloc(account_table, ++account_table_size * sizeof(char*)) ) == NULL)
    {
    perror("Memory Allocation Error");
    return -1;
    }

  account_table[account_table_size-1] = strdup(account);
  return account_table_size-1;
  }

struct cluster_limits_arrays* find_limits_for_cluster (char* cluster, int running_cpus)
  {
  if(NULL==cluster)
    {
    cluster=strdup("");
    }
  
  std::map<long, struct cluster_limits_arrays>* limit_array;
  //printf ("DEBUG: looking for cluster %s in %d clusters.\n", cluster,	  conf.limits_for_clusters->size ());
  if (conf.limits_for_clusters->find (cluster) != conf.limits_for_clusters->end ())
    {
    limit_array = &(conf.limits_for_clusters->find (cluster)->second);
    //printf ("DEBUG: cluster found.\n");
    }
  else
    {
    //unknown name, use default
    limit_array = &(conf.limits_for_clusters->find ("")->second);
    //printf ("DEBUG: cluster not found.\n");
    }
  
  struct cluster_limits_arrays* limits;
  //map is sorted by keys, so iterating from end means going from highest number of cpus
  //printf ("DEBUG: looking for limit in %d arrays.\n", limit_array->size ());
  for (std::map<long, struct cluster_limits_arrays>::reverse_iterator it = limit_array->rbegin (); limit_array->rend () != it; it++)
    {
    if (running_cpus >= it->first)
      {
      limits = &(it->second);
      //printf ("%d cpus", it->first);
      break;
      }
    }
  
  return limits;
  }

plan_limit* limit_create(time_t timestamp)
  {
  plan_limit* limit_new;

  if ((limit_new = (plan_limit*) malloc(sizeof(plan_limit))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  limit_new -> num_accounts = 0;

  limit_new -> capacity = 0;

  limit_new -> accounts = NULL;

  limit_new -> predeccessor = NULL;

  limit_new -> successor = NULL;

  limit_new -> timestamp = timestamp;

  return limit_new;
  }

int limit_add_account(plan_limit* limit, int account_id)
  {
  int new_position = limit -> num_accounts++;

  if (limit -> num_accounts > limit -> capacity)
    {
    limit -> capacity = limit -> capacity + 10;
    
    if ((limit -> accounts = (plan_limit_account**) realloc(limit -> accounts,(limit -> capacity) * sizeof(plan_limit_account*))) == NULL)
      {
      perror("Memory Allocation Error");
      return -1;
      }
    }

  while (new_position-1 > 0 && limit -> accounts[new_position-1] -> account_id > account_id)
    {
    limit -> accounts[new_position] = limit -> accounts[new_position-1];
    new_position--;
    }

  if ((limit -> accounts[new_position] = (plan_limit_account*) malloc(sizeof(plan_limit_account))) == NULL)
    {
    perror("Memory Allocation Error");
    return -1;
    }

  if ((limit -> accounts[new_position] -> num_cpus = (int*) malloc(NUM_LIMITS * sizeof(int))) == NULL)
    {
    perror("Memory Allocation Error");
    return -1;
    }

  for (int i = 0; i < NUM_LIMITS; i++)
    limit -> accounts[new_position] -> num_cpus[i] = 0;

  limit -> accounts[new_position] -> account_id = account_id;

  return new_position;
  }

int bin_search_account(struct plan_limit_account **accounts, int a, int b, int id )
  {
  if (b < a || a > b)
    return -1;

  int middle=(a+b)/2;
  if (accounts[middle]->account_id == id )
    return middle;

  if (a == b)
    return -1;

  if(id > accounts[middle]->account_id)
    return bin_search_account(accounts, middle+1, b, id);

  if(id < accounts[middle]->account_id)
    return bin_search_account(accounts, a, middle-1, id);

  return -1;
  }

int limit_find_account(plan_limit* limit, int account_id)
  {
  if (limit->num_accounts == 0)
    return -1;

  if (account_id < limit->accounts[0]->account_id || limit->accounts[limit->num_accounts-1]->account_id < account_id)
    return -1;

  return bin_search_account(limit->accounts, 0, limit->num_accounts-1, account_id);
  }

int walltime_suits_to_limit(int walltime, plan_cluster* cluster)
  {
  unsigned int found_limit = 0;
  //printf ("DEBUG: looking for threshold for cluster %s with %d cpus.\n", cluster->cluster_name, cluster->num_running_cpus);
  struct cluster_limits_arrays* limits;
  limits = find_limits_for_cluster (cluster->cluster_name, cluster->num_running_cpus);

  for (found_limit = 0; found_limit < limits->tresholds.size();found_limit++)
    {
    if (walltime <= limits->tresholds[found_limit])
      break;
    }

  return found_limit;
  }

void add_limit_to_limit(plan_limit* limit, int account_id, time_t walltime, int num_cpus, plan_cluster* cluster)
  {
  int found_account = -1;
  int found_limit = -1;

  found_account = limit_find_account(limit, account_id);

  if (found_account == -1)
    found_account = limit_add_account(limit, account_id);

  found_limit = walltime_suits_to_limit(walltime, cluster);

  limit -> accounts[found_account] -> num_cpus[found_limit] += num_cpus;
  }

plan_limit_account** clone_limit_accounts(plan_limit_account** old_limit_accounts, int num_accounts)
  {
  plan_limit_account** new_limit_accounts;

  if ((new_limit_accounts = (plan_limit_account**) malloc(num_accounts * sizeof(plan_limit_account*))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  for (int i = 0; i< num_accounts; i++)
    {
    if ((new_limit_accounts[i] = (plan_limit_account*) malloc(sizeof(plan_limit_account))) == NULL)
      {
      perror("Memory Allocation Error");
      return NULL;
      }

    if ((new_limit_accounts[i] -> num_cpus = (int*) malloc(NUM_LIMITS * sizeof(int))) == NULL)
      {
      perror("Memory Allocation Error");
      return NULL;
      }

    for (int j = 0; j < NUM_LIMITS;j++)
      new_limit_accounts[i] -> num_cpus[j] = old_limit_accounts[i] -> num_cpus[j];

    new_limit_accounts[i] -> account_id = old_limit_accounts[i] -> account_id;
    }

  return new_limit_accounts;
  }

plan_limit* clone_limit(plan_limit* old_limit, time_t new_timestamp)
  {
  plan_limit* new_limit = limit_create(new_timestamp);

  new_limit -> accounts = clone_limit_accounts(old_limit -> accounts, old_limit -> num_accounts);

  new_limit -> num_accounts = old_limit -> num_accounts;

  new_limit -> capacity = new_limit -> num_accounts;

  return new_limit;
  }

plan_limit* clone_limit_timestamp(plan_list* limits, time_t new_timestamp)
  {
  plan_limit* new_limit = NULL;

  plan_limit* limit;

  limits -> current = NULL;
  while (list_get_next(limits) != NULL)
    {
    limit = (plan_limit*)limits -> current;
    if (limit->timestamp == new_timestamp)
      {
      return NULL;
      }

    if (limit -> successor == NULL and limit -> timestamp < new_timestamp)
      {
      new_limit = limit_create(new_timestamp);
      list_add_end(limits, (void*) new_limit);
      break;
      }
    else
      if (limit -> timestamp < new_timestamp and ((plan_limit*)limit -> successor) -> timestamp > new_timestamp)
        {
        new_limit = clone_limit(limit, new_timestamp);
        list_add_inbackof(limits, limit, (void*) new_limit);
        break;
        }
    }

  return new_limit;
  }

void add_job_to_limits(plan_list* limits, plan_job* job, plan_cluster* cluster)
  {
  plan_limit* limit;

  int found_start_time = 0;

  int found_completion_time = 0;

  if (limits -> last == NULL)
    list_add_end(limits, (void*)limit_create(job -> completion_time));

  if (((plan_limit*)limits -> first) -> timestamp > job -> start_time)
    list_add_begin(limits, (void*)limit_create(job -> start_time));

  limits -> current = NULL;
  while (list_get_next(limits) != NULL)
    {
    limit = (plan_limit*)limits -> current;

    if (limit -> timestamp == job -> start_time)
      found_start_time = 1;

    if (limit -> timestamp == job -> completion_time)
      found_completion_time = 1;
    }

  if (not found_start_time)
    limit = clone_limit_timestamp(limits, job -> start_time);


  if (not found_completion_time)
    limit = clone_limit_timestamp(limits, job -> completion_time);

  limits -> current = NULL;
  while (list_get_next(limits) != NULL)
    {
    limit = (plan_limit*)limits -> current;

    if (limit -> timestamp >= job -> start_time and limit -> timestamp < job -> completion_time)
      add_limit_to_limit(limit, job -> account_id, job -> completion_time - job -> start_time, job -> req_ppn * job -> req_num_nodes, cluster);
    }
  }

int sum_limits_of_limit(plan_limit* limit, int limit_position)
  {
  int sum = 0;

  for (int i = 0; i < limit->num_accounts; i++)
    sum += limit->accounts[i]->num_cpus[limit_position];

  return sum;
  }

int limits_is_gap_ok(plan_list* limits, plan_job* job, plan_gap* gap, plan_cluster* cluster)
  {
  plan_limit* limit;
  int found_limit = -1;
  int found_account = -1;
  int walltime = job -> jinfo->get_walltime();
  int num_cpus = job -> req_ppn * job -> req_num_nodes;
  int global_limit_value = 0;

  found_limit = walltime_suits_to_limit(walltime, cluster);

  if (found_limit == -1)
    return 0;

  if (limits_values[found_limit] < num_cpus )
    return 0;

  limits -> current = NULL;
  while (list_get_next(limits) != NULL)
    {
    limit = (plan_limit*)limits -> current;

    if ((limit -> timestamp >= gap -> start_time and limit -> timestamp < gap -> end_time) or
       ((limit->successor != NULL) and (limit -> timestamp < gap -> start_time and limit->successor->timestamp > gap -> start_time )))
      {
      global_limit_value = sum_limits_of_limit(limit, found_limit);
      
      if (limits_values_global[found_limit] < global_limit_value + num_cpus)
        return 0;
      }

    found_account = -1;

    found_account = limit_find_account(limit, job -> account_id);

    if (found_account != -1)
      {
      if ((limit -> timestamp >= gap -> start_time and limit -> timestamp < gap -> end_time) or
         ((limit->successor != NULL) and (limit -> timestamp < gap -> start_time and limit->successor->timestamp > gap -> start_time )))
        if (limits_values[found_limit] < limit -> accounts[found_account] -> num_cpus[found_limit] + num_cpus)
          return 0;
      }
    }
  
  return 1;
  }

time_t adjust_job_starttime_according_to_limits(plan_list* limits, plan_job* job, plan_cluster* cluster)
  {
  plan_limit* limit;
  int found_limit = -1;
  int found_account = -1;
  int global_limit_value = 0;

  found_limit = walltime_suits_to_limit(job -> estimated_processing, cluster);

  limits -> current = NULL;
  while (list_get_next(limits) != NULL)
    {
    limit = (plan_limit*)limits -> current;

    if ((limit -> successor != NULL and (limit -> timestamp <= job -> start_time and limit -> successor -> timestamp > job -> start_time)) or
        (limit -> timestamp > job -> start_time and limit -> timestamp < job -> completion_time))
      {
      global_limit_value = sum_limits_of_limit(limit, found_limit);
      if (global_limit_value + (job->req_ppn*job->req_num_nodes) > limits_values_global[found_limit])
        {
        //sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "Global limits in action", "before: %ld %s", job -> start_time, job -> jinfo -> account);
        job -> start_time = limit -> successor -> timestamp;
        job -> completion_time = job -> start_time + job -> estimated_processing;
        //sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "Global limits in action", "after: %ld %s", job -> start_time, job -> jinfo -> account);
	continue;
        }
      }

    found_account = -1;

    found_account = limit_find_account(limit, job -> account_id);

    if (found_account== -1)
      continue;

    if ((limit -> successor != NULL and (limit -> timestamp <= job -> start_time and limit -> successor -> timestamp > job -> start_time)) or
        (limit -> timestamp > job -> start_time and limit -> timestamp < job -> completion_time))
      if (limit->accounts[found_account]->num_cpus[found_limit] + (job->req_ppn*job->req_num_nodes) > limits_values[found_limit])
        {
		//if (limit->successor == NULL)
		//  {//asi odeber job z rozvrhu a zacni update od zacatku
		//  }
		//sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "User limits in action", "before: %ld %s", job -> start_time, job -> jinfo -> account);
        job -> start_time = limit -> successor -> timestamp;
        job ->completion_time = job -> start_time + job -> estimated_processing;
	    //sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "User limits in action", "after: %ld %s", job -> start_time, job -> jinfo -> account);
        }
    }

  return job->start_time;
  }

void update_limits_values(sched* schedule, int k)
  {
  //printf("DEBUG: updating limits values:\n");
//  int select_limit_array=0;
//  if (schedule->clusters[k]->num_running_cpus >= 300)
//	  select_limit_array=1;
  struct cluster_limits_arrays* limits;
  limits = find_limits_for_cluster (schedule->clusters[k]->cluster_name, schedule->clusters[k]->num_running_cpus);
  //printf("DEBUG: selected limit of sizes %d and %d.\n",limits->percentages.size(),limits->percentages_global.size());
  for (unsigned int i=0; i<limits->tresholds.size();i++)
    {
      //DEBUG
  //    printf("%f = %f",percentages[select_limit_array][i],limits->limits_percentages[i]);
  //    printf("%f = %f",limits_percentages_global[select_limit_array][i],limits->percentages_global[i]);
    limits_values.resize(limits->tresholds.size(),2);
    limits_values_global.resize(limits->tresholds.size(),2);
    limits_values[i] = schedule->clusters[k]->num_running_cpus * limits->percentages[i];
    limits_values_global[i] = schedule->clusters[k]->num_running_cpus * limits->percentages_global[i];
    }
  }