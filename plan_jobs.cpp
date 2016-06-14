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
#include "check.h"
#include "misc.h"
#include "fifo.h"

#include <cstring>
#include <ctype.h>

#include "plan_list_operations.h"
#include "plan_operations.h"
#include "plan_gaps.h"
#include "plan_schedule.h"
#include "plan_log.h"
#include "job_info.h"
#include "plan_config.h"
#include "plan_limits.h"
#include <time.h>
#include <sys/time.h>

#include <limits.h>

plan_job* job_create()
  {
  plan_job *job;

  if ((job = (plan_job*) malloc(sizeof(plan_job))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  job -> successor = NULL;

  job -> predeccessor = NULL;

  job -> cpu_indexes = NULL;

  job -> latest_ncpu_index = NULL;

  job -> req_ppn = 0;

  job -> original_req_ppn = 0;

  job -> req_num_nodes = 0;
  
  job -> req_gpu = 0;

  job -> jinfo = NULL;

  job -> ninfo_arr = NULL;

  job -> fixed_nname_arr = NULL;

  job -> fixed_nname_arr_backup = NULL;

  job -> sch_nodespec = NULL;
  
  job -> available_after = 0;

  job -> run_me = 0;

  job -> account_id = -1;

  return job;
  }


void* job_fillin(plan_job* job, JobInfo *assigned_job, time_t start_time, time_t due_date)
  {
  resource_req *res_walltime = NULL;

  if (job == NULL)
    job = job_create();

  job -> jinfo = assigned_job;

  if (assigned_job == NULL)
    {
    job -> job_id = -1;
    }
  else
    {
    job -> job_id = parse_head_number(assigned_job -> job_id.c_str());
    }

  job -> usage = find_resource_req(assigned_job -> resreq, "procs") -> amount;

  job -> req_num_nodes = find_resource_req(assigned_job -> resreq, "nodect") -> amount;
  
  if (find_resource_req(assigned_job -> resreq, "gpu"))
    job -> req_gpu = find_resource_req(assigned_job -> resreq, "gpu") -> amount;

  job -> req_ppn = job -> usage / job -> req_num_nodes;

  job -> original_req_ppn = 0;

  job -> req_mem = find_resource_req(assigned_job -> resreq , "mem") -> amount / job -> req_num_nodes;

  job -> req_scratch_local = job_get_scratch_local(assigned_job) ;

  res_walltime = find_resource_req(assigned_job -> resreq, "walltime");
  if (res_walltime == NULL)
    {
    job -> estimated_processing = DEFAULT_WALLTIME;
    } 
  else 
    {
    job -> estimated_processing = res_walltime -> amount;
    }

  if (start_time == -1)
    {
    job -> start_time = -1;
    
    job -> completion_time = -1;
    }
  else
    {
    job -> start_time = start_time;

    job -> completion_time = start_time+job -> estimated_processing;
    }

  job -> due_date = due_date;

  if (job -> ninfo_arr != NULL)
    {
	//freeschednodes
    }

  if ((job -> ninfo_arr = (node_info**)malloc(job -> req_num_nodes*sizeof(node_info*))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  for (int i=0; i<job -> req_num_nodes;i++)
    {
    job -> ninfo_arr[i]= NULL;
    }

  if ((job -> fixed_nname_arr = (char**)malloc(job -> req_num_nodes * sizeof(char*))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  for (int i=0; i<job -> req_num_nodes;i++)
    {
    job -> fixed_nname_arr[i] = NULL;
    }

  if ((job -> cpu_indexes = (int*) malloc(job -> usage * sizeof(int))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  if ((job -> latest_ncpu_index = (int*)malloc(job -> req_num_nodes * sizeof(int))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  return (void*) job;
  }


int add_already_running_jobs(sched* schedule, JobInfo** new_jobs)
  {
  int cl_number = 0;
  int counter = 0;
  bool found=0;

  plan_job* new_job;

  if (new_jobs == NULL)
    return 1;

  while (*new_jobs != NULL)
    {
    if ((*new_jobs) -> state == JobRunning)
      {
      new_job = job_create();
      job_fillin(new_job, *new_jobs, (*new_jobs) -> stime, 0);
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, new_job -> jinfo -> job_id.c_str(), "new job already running");

      new_job->account_id = set_and_get_account_id((char*)new_job->jinfo->account.c_str());

      found=0;
      for (cl_number=0; cl_number < schedule -> num_clusters; cl_number++)
        {
        for (int i=0; i<schedule -> clusters[cl_number] -> num_nodes; i++)
          {
          if (!schedule -> clusters[cl_number] -> nodes[i] -> has_jobs())
            continue;

          for (int j=0; j<schedule -> clusters[cl_number] -> nodes[i] -> get_cores_total(); j++)
            if (new_job -> jinfo == schedule -> clusters[cl_number] -> nodes[i] -> jobs_on_cpu[j])
              {
              sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, schedule -> clusters[cl_number] -> nodes[i] -> get_name(), "already running job found on");
              list_add_begin(schedule -> jobs[cl_number], new_job);

              j = schedule -> clusters[cl_number] -> nodes[i] -> get_cores_total();
              i = schedule -> clusters[cl_number] -> num_nodes;
              cl_number = schedule -> num_clusters;

              found = 1; break;
              }
          if (found == 1)
            break;
          }
        if (found == 1)
          break;
        }
      }
    counter++;
    new_jobs++;
    }

  new_jobs-=counter;
  return 0;
  }

int try_to_schedule_new_jobs(int pbs_sd, server_info *p_info, sched* schedule, JobInfo** new_jobs, time_t time_now)
  {
  int cl_number = 0;
  int counter = 0;
  int not_scheduled = 0;
  int cl_min = 0;
  char buffer_time[30];
  
  struct timeval tv_start;
  struct timeval tv_step1;
  //struct timeval tv_step2;
  long long step_time;
  long long total_time;

  plan_job* new_job;
  plan_gap* found_gap;
  plan_gap* found_earliest_gap;

  if (new_jobs == NULL)
    return 1;

  while (*new_jobs!=NULL)
    {
    total_time = 0;
    gettimeofday(&tv_start, NULL); 
        
    if (((*new_jobs) -> state != JobRunning) && ((*new_jobs) -> state != JobQueued))
      {
      //job neni obslouzen
      }

    if ((*new_jobs) -> state == JobQueued)
      {
      new_job = job_create();

      job_fillin(new_job, *new_jobs, -1, 0);

      new_job->account_id = set_and_get_account_id((char*)new_job->jinfo->account.c_str());

      /*
       * Find first suitable gap on suitable cluster
       */

      found_earliest_gap = NULL;
      found_gap = NULL;
      cl_number = 0;
      cl_min = -1;
      while (cl_number < schedule -> num_clusters )
        {

        if (check_cluster_suitable(p_info, schedule -> clusters[cl_number], new_job))
          { /* not suitable; try next cluster */
          cl_number++;
          continue;
          }
        
        found_gap = find_first_gap(schedule, cl_number ,new_job, false);

        if (found_gap != NULL)
          {
          if (found_earliest_gap == NULL)
            {
            found_earliest_gap = found_gap;

            cl_min = cl_number;
            }
          else
            {
            if (found_gap -> start_time < found_earliest_gap -> start_time )
              {
              found_earliest_gap = found_gap;
              cl_min = cl_number;
              }
            }
          }
        cl_number++;
        }

      cl_number = cl_min;
      found_gap = found_earliest_gap;
          
      gettimeofday(&tv_step1, NULL); 
      step_time = (tv_step1.tv_sec * 1e6 + tv_step1.tv_usec) - (tv_start.tv_sec * 1e6 + tv_start.tv_usec);
          
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, new_job -> jinfo -> job_id.c_str(), "time measurement; find_first_gap (several clusters, find the best): %ld", step_time);
      total_time = total_time + step_time;
      gettimeofday(&tv_start, NULL); 

      if (found_gap == NULL)
        {
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, new_job -> jinfo -> job_id.c_str(), "No suitable gap!");
        update_job_planned_start(pbs_sd, new_job);
        update_job_planned_nodes(pbs_sd, new_job->jinfo, "");
        update_job_comment(pbs_sd, new_job -> jinfo, "Never Running: This jobs requirements will never be satisfied under the current grid configuration.");
        not_scheduled ++;
        }
      else
        {
        found_gap = find_first_gap(schedule, cl_number ,new_job, true);
                
        gettimeofday(&tv_step1, NULL); 
        step_time = (tv_step1.tv_sec * 1e6 + tv_step1.tv_usec) - (tv_start.tv_sec * 1e6 + tv_start.tv_usec);
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, new_job -> jinfo -> job_id.c_str(), "time measurement; find_first_gap (use the best): %ld", step_time);
        total_time = total_time + step_time;
        gettimeofday(&tv_start, NULL); 

        strftime(buffer_time, 30, "%Y:%m:%d %H:%M:%S", localtime(&found_gap -> start_time));

        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, new_job -> jinfo -> job_id.c_str(), "Gap found on cluster: %d start: %s gab duration: %ld job duration %ld", cl_number, buffer_time, found_gap -> duration, new_job -> estimated_processing );

        job_put_in_gap(schedule,cl_number,new_job,found_gap);

        if (update_sched(schedule, cl_number, time_now) == 2)
          update_sched(schedule, cl_number, time_now);
                
        gettimeofday(&tv_step1, NULL);
        step_time = (tv_step1.tv_sec * 1e6 + tv_step1.tv_usec) - (tv_start.tv_sec * 1e6 + tv_start.tv_usec);
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, new_job -> jinfo -> job_id.c_str(), "time measurement; update (after find_first_gap): %ld cluster: %ld #jobs: %ld", step_time, cl_number, schedule ->jobs[cl_number]->num_items );
        total_time = total_time + step_time;
                
        long int total_num_jobs=0;                
        for (int i = 0; i < schedule -> num_clusters; i++)
          total_num_jobs += schedule ->jobs[i]->num_items;
                
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, new_job -> jinfo -> job_id.c_str(), "time measurement; new job total time: %ld #jobs: %ld", total_time, total_num_jobs);
        }

      }
    new_jobs++;
    counter++;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "New Jobs", "%d/%d scheduled", counter - not_scheduled, counter);

  new_jobs -= counter;
  free(new_jobs);
  return 0;
  }

void remove_substring(char *s,const char *toremove)
  {
  while( (s = strstr(s,toremove)) )
    memmove(s,s+strlen(toremove),1+strlen(s+strlen(toremove)));
  }


char *multip_numbers(int a,char *b)
  {
  char *newstring = (char*)malloc( sizeof(char) * ( strlen(b) + 3 ) );
  strcpy (newstring,b);

  char *s=strstr(b, "mem=");
  long long int new_num=atoll(s+4) * a;
  char *p = s+4;
  while (isdigit(*p)) {p++;}

  newstring[strlen(b)-strlen(s)+4]='\0';

  sprintf(newstring, "%s%lld%s", newstring, new_num,p);
  return newstring;
  }

int job_create_nodespec(plan_job* job)
  {
  int distinct_num_nodes;
  int found;
  int num_ppn;
  int is_excl=0;
  node_info* planned_node;

  char* tmp_ns_start;
  char* tmp_multi;

  int tmp_ppn;
  int tmp_ns_length;

  tmp_ns_start = strchr((char*)job -> jinfo -> nodespec.c_str(), ':') + 1;

  if ((tmp_ppn = strstr(tmp_ns_start, "ppn=")!=NULL))
    tmp_ns_start = strchr(tmp_ns_start + tmp_ppn, ':') + 1;

  if (!strcmp(tmp_ns_start + strlen(tmp_ns_start) - 5,"#excl"))
    {
    is_excl = 1;
    tmp_ns_start[strlen(tmp_ns_start) - 5]='\0';
    }

  char * pch = strchr(tmp_ns_start,'^');
  char * chmin;
  char * ch;
  char * to_remove;

  while (pch != NULL)
    {
    chmin=NULL;
    ch = strchr(pch,',');
    if (ch != NULL)
      chmin=ch;

    ch = strchr(pch,':');
    if (ch != NULL && chmin==NULL)
      chmin=ch;
    else
      if (ch != NULL && chmin!=NULL && ch < chmin)
        chmin=ch;

    ch = strchr(pch,' ');
    if (ch != NULL && chmin==NULL)
      chmin=ch;
    else
      if (ch != NULL && chmin!=NULL && ch < chmin)
        {
	chmin=ch;
        }

    if (chmin == NULL)
      to_remove=strdup(pch-1);
    else
      {
      to_remove=strdup(pch);
      to_remove[chmin-pch+1]='\0';
      }
    
    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_JOB, "aaa", "%s", to_remove);

    remove_substring(tmp_ns_start, to_remove);

    pch = strchr(tmp_ns_start,'^');
    }

  tmp_ns_length = strlen(tmp_ns_start);

  planned_node = NULL;
  num_ppn = 1;

  job -> sch_nodespec = NULL;


  for (int i = 0; i < job -> req_num_nodes; i++)
    {
    if (planned_node == job -> ninfo_arr[i])
      num_ppn++;

    if (planned_node != NULL && planned_node != job -> ninfo_arr[i])
      {
      if (job -> sch_nodespec == NULL)
        {
        if ((job -> sch_nodespec = (char*)malloc(tmp_ns_length + 50)) == NULL)
          {
          perror("Memory Allocation Error");
          return 0;
          }

        tmp_multi = multip_numbers(num_ppn,tmp_ns_start);
        sprintf(job -> sch_nodespec, "host=%s:ppn=%d:%s", planned_node -> get_name(), num_ppn * job -> req_ppn,  tmp_multi);
        free(tmp_multi);
	}
      else
        {
        if ((job -> sch_nodespec=(char*)realloc(job -> sch_nodespec,strlen(job -> sch_nodespec) + tmp_ns_length + 50)) == NULL)
          {
          perror("Memory Allocation Error");
          return 0;
          }

        tmp_multi = multip_numbers(num_ppn,tmp_ns_start);
        sprintf(job -> sch_nodespec, "%s+host=%s:ppn=%d:%s",job -> sch_nodespec, planned_node -> get_name(), num_ppn * job -> req_ppn,  tmp_multi);
        free(tmp_multi);
        }

      planned_node = job -> ninfo_arr[i];
      num_ppn=1;
      }

    if (planned_node == NULL)
      planned_node = job -> ninfo_arr[i];
    }

  if (planned_node == NULL)
    {
    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_JOB, "ERROR", "going to run job without planned_node");
    return 0;
    }

  if (job -> sch_nodespec == NULL)
    {
    if ((job -> sch_nodespec = (char*)malloc(tmp_ns_length + 50)) == NULL)
      {
      perror("Memory Allocation Error");
      return 0;
      }

    tmp_multi = multip_numbers(num_ppn,tmp_ns_start);
    sprintf(job -> sch_nodespec, "host=%s:ppn=%d:%s", planned_node -> get_name(), num_ppn * job -> req_ppn,  tmp_multi);
    free(tmp_multi);
    }
  else
    {
    if ((job -> sch_nodespec=(char*)realloc(job -> sch_nodespec, strlen(job -> sch_nodespec) + tmp_ns_length + 50)) == NULL)
      {
      perror("Memory Allocation Error");
      return 0;
      }

    tmp_multi = multip_numbers(num_ppn,tmp_ns_start);
    sprintf(job -> sch_nodespec, "%s+host=%s:ppn=%d:%s",job -> sch_nodespec, planned_node -> get_name(), num_ppn * job -> req_ppn,  tmp_multi);
    free(tmp_multi);
    }

  distinct_num_nodes = 0;

  for (int i = 0; i < job -> req_num_nodes; i++)
    {
    found=0;
    for (int j = 0; j < distinct_num_nodes; j++)
      if (job -> ninfo_arr[i] == job -> ninfo_arr[j])
        found = 1;

    if (found)
      {
      job -> ninfo_arr[i] = NULL;
      }
    else
      {
      job -> ninfo_arr[distinct_num_nodes] = job -> ninfo_arr[i];

      if (distinct_num_nodes != i)
        job -> ninfo_arr[i] = NULL;

      distinct_num_nodes++;
      }
    }

  if (is_excl || job->jinfo ->is_exclusive())
    {
    if ((job -> sch_nodespec=(char*)realloc(job -> sch_nodespec,strlen(job -> sch_nodespec) + 10)) == NULL)
      {
      perror("Memory Allocation Error");
      return 0;
      }
    sprintf(job -> sch_nodespec, "%s%s",job -> sch_nodespec, "#excl");
    }

  job -> jinfo -> nodespec = job -> sch_nodespec;
  sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_JOB, "sch_nodespec", "job: %s spec: %s",job -> jinfo->job_id.c_str(),job->sch_nodespec);
  
  free(job -> sch_nodespec);
  job -> sch_nodespec = NULL;

  return distinct_num_nodes;
  }