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
#include <limits.h>
#include <math.h>

#include "plan_operations.h"
#include "plan_evaluation.h"
#include "plan_list_operations.h"
#include "plan_gaps.h"
#include "globals.h"

#include <time.h>
#include <sys/time.h>

bool job_backup_fixnodes(plan_job* job)
  {
  if (job -> fixed_nname_arr == NULL )
    return false;

  if (job -> fixed_nname_arr_backup != NULL )
    for (int i=0; i< job->req_num_nodes; i++)
      if (job->fixed_nname_arr_backup[i]!= NULL)
        free(job->fixed_nname_arr_backup[i]);
    
  free(job -> fixed_nname_arr_backup);

  if ((job -> fixed_nname_arr_backup = (char**)malloc(job -> req_num_nodes*sizeof(char*))) == NULL)
    {
    perror("Memory Allocation Error");
    return false;
    }

  for (int i = 0; i < job -> req_num_nodes; i++)
    {
    if (job -> fixed_nname_arr[i] != NULL )
      {
      job -> fixed_nname_arr_backup[i] = strdup(job -> fixed_nname_arr[i]);
      free(job -> fixed_nname_arr[i]);
      job -> fixed_nname_arr[i] = NULL;
      }
    else
      {
      //if (job -> fixed_nname_arr_backup[i]!= NULL)
      //free(job -> fixed_nname_arr_backup[i]);
      job -> fixed_nname_arr_backup[i] =  NULL;
      }
    
    if (job -> fixed_nname_arr[i] != NULL)
      free(job -> fixed_nname_arr[i]);    
    job -> fixed_nname_arr[i] = NULL;
    }
  
  return true;
  }

bool job_restore_fixnodes(plan_job* job)
  {
  if (job -> fixed_nname_arr_backup != NULL)
    {
    for (int i=0; i< job->req_num_nodes; i++)
      if (job->fixed_nname_arr[i]!= NULL) {
        free(job->fixed_nname_arr[i]);
        job->fixed_nname_arr[i] = NULL;
      }
    
    free(job -> fixed_nname_arr);
    job -> fixed_nname_arr = job -> fixed_nname_arr_backup;
    job -> fixed_nname_arr_backup = NULL;
    }
  else
    return false;

  return true;
  }

void job_free_fixnodes_backup(sched* schedule, int k)
  {
  plan_job* job;

  schedule->jobs[k] -> current = NULL;
  for (int i = 0; i < schedule -> jobs[k] -> num_items; i++)
    {
    list_get_next(schedule -> jobs[k]);
    job = (plan_job*)schedule -> jobs[k] -> current;
    
    if (job == NULL)
        break;

    if (job -> fixed_nname_arr_backup != NULL)
      {
      for (int i=0; i< job->req_num_nodes; i++)
        if (job->fixed_nname_arr_backup[i]!= NULL)
          free(job->fixed_nname_arr_backup[i]);
      free(job -> fixed_nname_arr_backup);
      job -> fixed_nname_arr_backup = NULL;
      }
    }
  }

plan_job** backup_job_order(sched* schedule, int k)
  {
  plan_job** backup;

  if ((k >= schedule -> num_clusters) || (schedule -> jobs[k] == NULL) || (schedule -> jobs[k] -> num_items == 0))
    return NULL;

  if ((backup = (plan_job**)malloc((1 + schedule->jobs[k] -> num_items) * sizeof(plan_job*))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  schedule -> jobs[k] -> current = NULL;
  for (int i = 0; i < schedule -> jobs[k] -> num_items; i++)
    {
    list_get_next(schedule -> jobs[k]);
    backup[i] = (plan_job*)schedule -> jobs[k] -> current;
    }

  backup[schedule -> jobs[k] -> num_items] = NULL;
  return backup;
  }

int restore_job_order(sched* schedule, int k, plan_job** backup)
  {
  int i;
  void* item;

  if ((k >= schedule -> num_clusters) || (schedule -> jobs[k] == NULL) || (schedule -> jobs[k] -> num_items == 0) || (backup == NULL))
    return 0;

  schedule -> jobs[k] -> num_items = 0;
  schedule -> jobs[k] -> current = NULL;
  schedule -> jobs[k] -> first = NULL;
  schedule -> jobs[k] -> last=NULL;

  i = 0;
  while (backup[i] != NULL)
    {
    item = (void*)backup[i];

    if (i == 0)
      {
      schedule -> jobs[k] -> first=item;
      schedule -> jobs[k] -> last=item;

      set_predeccessor(item, NULL, Jobs);
      set_successor(item, NULL, Jobs);

      }
    else
      {
      set_successor(schedule -> jobs[k] -> last, item, Jobs);
      set_predeccessor(item, schedule -> jobs[k] -> last, Jobs);
      set_successor(item, NULL, Jobs);

      schedule -> jobs[k] -> last = item;
      }

    //only backuped node will be restored
    job_restore_fixnodes((plan_job*) item);

    i++;
    schedule -> jobs[k] -> num_items++;
    }

  return schedule -> jobs[k] -> num_items;
  }


int random_job_to_random_position(sched* schedule, int k, time_t time)
  {
  plan_job* job;
  plan_job* job_tmp;

  plan_job* predeccessor;

  if (schedule -> clusters[k] -> num_queued_jobs < conf.optim_minimal_queued)
    return 1;

  job = NULL;
  job_tmp = NULL;

  do
    {
    job = (plan_job*)list_get_random_item(schedule -> jobs[k], conf.optim_minimal_queued);
    }
  while (job == NULL || job -> jinfo -> state != JobQueued);

  predeccessor=job->predeccessor;

  job_backup_fixnodes(job);

  list_remove_item(schedule -> jobs[k], (void*) job, 0);

  if (update_sched(schedule, k, time)==2)
    update_sched(schedule, k, time);

  job_tmp = NULL;

  do
    {
    //jobs_count_limit se musi o jedno snizit ale nesmi se dostat pod 2
    job_tmp = (plan_job*)list_get_random_item(schedule -> jobs[k], conf.optim_minimal_queued-1);
    }
  while (job_tmp == predeccessor);

  if (job_tmp != NULL)
    {
    list_add_inbackof(schedule -> jobs[k], (void*) job_tmp, (void*) job);
    }
  else
    {
    list_add_begin(schedule -> jobs[k],(void*) job);
    }

  if (update_sched(schedule, k, time)==2)
    update_sched(schedule, k, time);

  return 0;
  }

int random_job_to_first_gap(sched* schedule, int k, time_t time)
  {
  plan_job* job;
  plan_gap* gap;

  if (schedule -> clusters[k] -> num_queued_jobs < conf.optim_minimal_queued)
    return 1;

  job = NULL;
  gap = NULL;

  do
    {
    job = (plan_job*)list_get_random_item(schedule -> jobs[k], conf.optim_minimal_queued);
    }
  while (job == NULL || job -> jinfo->state != JobQueued);

  job_backup_fixnodes(job);

  list_remove_item(schedule -> jobs[k], (void*) job, 0);

  if (update_sched(schedule, k, time)==2)
    update_sched(schedule, k, time);

  gap = find_first_gap(schedule, k, job, true);

  if (gap == NULL || gap -> following_job == job)
    return 1;

  job_put_in_gap(schedule, k, job, gap);

  if (update_sched(schedule, k, time)==2)
    update_sched(schedule, k, time);

  return 0;
  }

int plan_optimization(sched* schedule, time_t time_now)
  {
  int cl_number;
  int num_queued;
  int optim_cycles;
  int res;
  int num_attempts;
  int num_success;
  plan_eval evaluation_before, evaluation_after;
  plan_job** backup;
  time_t optim_duration;
  
  struct timeval tv_start;
  struct timeval tv_step1;
  //struct timeval tv_step2;
  long long step_time;  

  double weight_slowdown;
  double weight_wait_time;
  double weight_response_time;
  double weight_fairness;

  optim_cycles = 0;

  optim_duration = -1;

  num_attempts = 0;

  num_success = 0;

  num_queued = 0;

  for (cl_number = 0; cl_number < schedule -> num_clusters; cl_number++)
    num_queued += schedule -> clusters[cl_number] -> num_queued_jobs;

  optim_cycles = 30;
  optim_duration = 20;
          
  if (optim_cycles == 0)
    {
    evaluation_before = evaluate_schedule(schedule);
    return 0;
    }


  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "evaluation", "number of queued jobs: %i cycles limit: %i duration limit %i",
          num_queued,
          optim_cycles,
          optim_duration);

  gettimeofday(&tv_start, NULL); 
  evaluation_before = evaluate_schedule(schedule);

  optim_duration += time(0);

  for (int i = 0; i < optim_cycles; i++)
    {
    for (cl_number = 0; cl_number < schedule->num_clusters; cl_number++)
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "evaluation", "cluster %d:", cl_number);
      if (schedule -> clusters[cl_number] -> num_queued_jobs < conf.optim_minimal_queued)
        {
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "evaluation", "not enough queued jobs on cluster %d", cl_number);
        continue;
        }

      if (optim_duration < time(0))
        {
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "evaluation", "optimization time is up");
        i = optim_cycles;
        continue;
        }

      backup = backup_job_order(schedule,cl_number);

      //res = random_job_to_first_gap(pbs_sd, schedule, cl_number,time_now);
      res = random_job_to_random_position(schedule, cl_number,time_now);

      if (res == 0) 
        {
        num_attempts++;
        evaluation_after = evaluate_schedule(schedule);

        weight_slowdown = (1.0 * (evaluation_before.slowdown - evaluation_after.slowdown)) / evaluation_before.slowdown;

        weight_wait_time = (1.0 * (evaluation_before.wait_time - evaluation_after.wait_time)) / evaluation_before.wait_time;

        weight_response_time = (0.0 * (evaluation_before.response_time - evaluation_after.response_time)) / evaluation_before.response_time;

        weight_fairness = (10.0 * (evaluation_before.fairness - evaluation_after.fairness)) / evaluation_before.fairness;
            
        if (weight_slowdown + weight_wait_time + weight_response_time + weight_fairness > 0) 
          {
          num_success++;

          sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "evaluation", "success on cluster %d number of queued jobs: %d, slowdown: %f %, wait_time: %f %, response_time: %f %, fairness: %f %",
                    cl_number,
                    schedule -> clusters[cl_number] -> num_queued_jobs,
                    (1.0 - ((evaluation_before.slowdown * 1.0) / (evaluation_after.slowdown * 1.0))) * 100,
                    (1.0 - ((evaluation_before.wait_time * 1.0) / (evaluation_after.wait_time * 1.0))) * 100,
                    (1.0 - ((evaluation_before.response_time * 1.0) / (evaluation_after.response_time * 1.0))) * 100,
                    (1.0 - ((evaluation_before.fairness * 1.0) / (evaluation_after.fairness * 1.0))) * 100
                    );
          sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "evaluation", "success on cluster %d number of queued jobs: %d, slowdown; before: %f after: %f",
                    cl_number,
                    schedule -> clusters[cl_number] -> num_queued_jobs,
                    evaluation_before.slowdown, evaluation_after.slowdown
                    );        
          
          sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "evaluation", "success on cluster %d number of queued jobs: %d, wait_time; before: %f after: %f",
                    cl_number,
                    schedule -> clusters[cl_number] -> num_queued_jobs,
                    evaluation_before.wait_time, evaluation_after.wait_time
                    );      


          sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "evaluation", "success on cluster %d number of queued jobs: %d, response_time; before: %f after: %f",
                    cl_number,
                    schedule -> clusters[cl_number] -> num_queued_jobs,
                    evaluation_before.response_time, evaluation_after.response_time
                    );         
          
          sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "evaluation", "success on cluster %d number of queued jobs: %d, fairness; before: %f after: %f",
                    cl_number,
                    schedule -> clusters[cl_number] -> num_queued_jobs,
                    evaluation_before.fairness, evaluation_after.fairness
                    );           

          evaluation_before = evaluation_after;

          job_free_fixnodes_backup(schedule, cl_number);
          }
        else
          {
          restore_job_order(schedule, cl_number, backup);
          job_free_fixnodes_backup(schedule, cl_number);
          if (update_sched(schedule, cl_number, time_now) != 0)
              update_sched(schedule, cl_number, time_now);
          evaluation_after = evaluate_schedule(schedule);
#if 0
    # prioritni joby tuto hlasku zpusobi vzdy
          if (evaluation_before.response_time != evaluation_after.response_time ||
              evaluation_before.wait_time != evaluation_after.wait_time ||
	      evaluation_before.slowdown != evaluation_after.slowdown)
            sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "Optimization Error", "BACKUP FAILED");
#endif          
	  }
	}
      else
        {
        restore_job_order(schedule, cl_number, backup);
        }
        
      free(backup);
      } // cluster loop
    } // main optimization loop

   gettimeofday(&tv_step1, NULL); 
   step_time = (tv_step1.tv_sec * 1e6 + tv_step1.tv_usec) - (tv_start.tv_sec * 1e6 + tv_start.tv_usec);
   sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "evaluation", "number of attempts: %d, number of successes: %d duration: %ld", num_attempts, num_success, step_time);

   return 0;
}
