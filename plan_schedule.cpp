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

#include "plan_list_operations.h"
#include "plan_optimization.h"
#include "plan_fairshare.h"
#include "plan_gaps_memory.h"
#include "plan_limits.h"

#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "globals.h"
#include "fairshare.h"
#include <algorithm>

using namespace std;

#define PBS_MAX_TIME (LONG_MAX - 1)

long decode_time(
  char             *val)

  {
  int   i;
  char  msec[4];
  int   ncolon = 0;
  int   use_days = 0;
  int   days = 0;
  char *pc;
  long  rv = 0;
  char *workval;
  char *workvalsv = NULL;
  long ret=0;

  if ((val == NULL) || (strlen(val) == 0))
    {
    return ret;
    }

  /* FORMAT: [0w][0d][0h][0m][0s] */
  if (strpbrk(val,"sSmMhHdDwW") != NULL)
    {
    const char *old_time = val;

    long long total_time = 0;

    // seconds
    const char *seconds = strpbrk(old_time,"sS");
    if (seconds != old_time && seconds != NULL)
      {
      --seconds;
      while (seconds != old_time && isdigit(*seconds))
        --seconds;
      if (seconds != old_time)
        ++seconds;

      total_time += atoi(seconds);
      }

    // minutes
    const char *minutes = strpbrk(old_time,"mM");
    if (minutes != old_time && minutes != NULL)
      {
      --minutes;
      while (minutes != old_time && isdigit(*minutes))
        --minutes;
      if (minutes != old_time)
        ++minutes;

      total_time += atoi(minutes)*60;
      }

    // hours
    const char *hours = strpbrk(old_time,"hH");
    if (hours != old_time && hours != NULL)
      {
      --hours;
      while (hours != old_time && isdigit(*hours))
        --hours;
      if (hours != old_time)
        ++hours;

      total_time += atoi(hours)*60*60;
      }

    // days
    const char *days = strpbrk(old_time,"dD");
    if (days != old_time && days != NULL)
      {
      --days;
      while (days != old_time && isdigit(*days))
        --days;
      if (days != old_time)
        ++days;

      total_time += atoi(days)*24*60*60;
      }

    // weeks
    const char *weeks = strpbrk(old_time,"wW");
    if (weeks != old_time && weeks != NULL)
      {
      --weeks;
      while (weeks != old_time && isdigit(*weeks))
        --weeks;
      if (weeks != old_time)
        ++weeks;

      total_time += atoi(weeks)*7*24*60*60;
      }

    rv = total_time;
    }
  /* FORMAT:  [DD]:HH:MM:SS[.MS] */
  else
    {
    workval = strdup(val);

    workvalsv = workval;

    if (workvalsv == NULL)
      {
      /* FAILURE - cannot alloc memory */

      goto badval;
      }

    for (i = 0;i < 3;++i)
      msec[i] = '0';

    msec[i] = '\0';

    for (pc = workval;*pc;++pc)
      {
      if (*pc == ':')
        {
        if (++ncolon > 3)
          goto badval;

        /* are days specified? */
        if (ncolon > 2)
          use_days = 1;
        }
      }

    for (pc = workval;*pc;++pc)
      {
      if (*pc == ':')
        {

        *pc = '\0';

        if (use_days)
          {
          days = atoi(workval);
          use_days = 0;
          }
        else
          {
          rv = (rv * 60) + atoi(workval);
          }

        workval = pc + 1;

        }
      else if (*pc == '.')
        {
        *pc++ = '\0';

        for (i = 0; (i < 3) && *pc; ++i)
          msec[i] = *pc++;

        break;
        }
      else if (!isdigit((int)*pc))
        {
        goto badval; /* bad value */
        }
      }

    rv = (rv * 60) + atoi(workval);

    if (days > 0)
     rv = rv + (days * 24 * 3600);

    if (rv > PBS_MAX_TIME)
      goto badval;

    if (atoi(msec) >= 500)
      rv++;
    }

  ret = rv;

  free(workvalsv);

  /* SUCCESS */

  return ret;

badval:

  free(workvalsv);

  return(PBSE_BADATVAL);
  }  /* END decode_time() */

bool is_prefix(string const& s1, string const&s2)
  {
  const char*p = s1.c_str();
  const char*q = s2.c_str();
  while (*p&&*q)
    if (*p++!=*q++)
      return false;
  return true;
  }

std::set<std::string> filter_exclude(std::set<std::string> strings, bool select_exclude)
  {
  std::set<std::string> output;
  std::set<std::string>::const_iterator it;

  for (it = strings.begin(); it != strings.end(); it++)
    if (select_exclude)
      {
      if (is_prefix(*it,"min_") || is_prefix(*it,"max_"))
        output.insert(*it);
      }
    else
      {
      if (!is_prefix(*it,"min_") && !is_prefix(*it,"max_"))
        output.insert(*it);
      }

  return output;
  }

sched* schedule_create()
  {
  sched* new_schedule;

  if ((new_schedule = (sched*) malloc(sizeof(sched))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  new_schedule -> num_clusters = 0;

  new_schedule -> jobs = NULL;

  new_schedule -> gaps = NULL;

  new_schedule -> clusters = NULL;

  new_schedule -> updated = NULL;

  new_schedule -> limits = NULL;

  return new_schedule;
  }

plan_cluster* cluster_create()
  {
  plan_cluster *cluster_new;

  if ((cluster_new = new (nothrow) plan_cluster) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  cluster_new -> nodes = NULL;

  cluster_new -> num_nodes = 0;

  cluster_new -> nodes_name = NULL;

  cluster_new -> num_properties = 0;

  cluster_new -> sinfo = NULL;

  return cluster_new;
  }

/*
int clus_push_properties(plan_cluster* clus, char **properties)
  {
  int num_prop;
  num_prop = 0;
  if (properties != NULL)
    {
    while (*properties != NULL)
	  {
      clus -> cluster_properties.insert(string(*properties));

	  properties++;
	  }
    }


  clus -> num_properties = num_prop;
  return 0;
  }
*/

int schedule_add_cluster(sched* sched, server_info* sinfo, const char* cluster_name, const set<string>& properties)
  {
  if (sched == NULL)
    return 1;

  sched -> num_clusters++;

  if ((sched -> jobs = (plan_list**) realloc(sched -> jobs,(sched -> num_clusters) * sizeof(plan_list*))) == NULL)
    {
    perror("Memory Allocation Error");
    return 1;
    }

  (sched -> jobs)[sched -> num_clusters -1] = list_create(Jobs);

  if ((sched -> gaps = (plan_list**) realloc(sched -> gaps,(sched -> num_clusters) * sizeof(plan_list*))) == NULL)
    {
    perror("Memory Allocation Error");
    return 1;
    }

  (sched -> gaps)[sched -> num_clusters -1] = list_create(Gaps);

  //list_add_end(( sched -> gaps)[sched -> num_clusters -1],gap_create_last(time(0), NULL));

  if ((sched -> limits = (plan_list**) realloc(sched -> limits,(sched -> num_clusters) * sizeof(plan_limits*))) == NULL)
    {
    perror("Memory Allocation Error");
    return 1;
    }

  (sched -> limits)[sched -> num_clusters -1] = list_create(Limits);

  if ((sched -> clusters = (plan_cluster**) realloc(sched -> clusters,(sched -> num_clusters) * sizeof(plan_cluster*))) == NULL)
    {
    perror("Memory Allocation Error");
    return 1;
    }

  (sched -> clusters)[sched -> num_clusters -1] = cluster_create();

  if ((sched -> updated = (unsigned*) realloc(sched -> updated,(sched -> num_clusters) * sizeof(unsigned))) == NULL)
    {
    perror("Memory Allocation Error");
    return 1;
    }

  (sched -> clusters)[sched -> num_clusters -1] -> cluster_name = strdup(cluster_name);
  (sched -> clusters)[sched -> num_clusters -1] -> num_nodes = 0;
  (sched -> clusters)[sched -> num_clusters -1] -> num_cpus = 0;
  (sched -> clusters)[sched -> num_clusters -1] -> num_running_cpus = 0;
  (sched -> clusters)[sched -> num_clusters -1] -> available_ram = 0;
  (sched -> clusters)[sched -> num_clusters -1] -> num_properties = filter_exclude(properties, 0).size();
  (sched -> clusters)[sched -> num_clusters -1] -> cluster_properties = filter_exclude(properties, 0);

  (sched -> jobs)[sched -> num_clusters -1] -> sinfo = sinfo;
  (sched -> gaps)[sched -> num_clusters -1] -> sinfo = sinfo;
  (sched -> limits)[sched -> num_clusters -1] -> sinfo = sinfo;

  return 0;
  }

int find_property(char **properties,char *property)
  {
  while (*properties != NULL)
    {
    if (strcmp(*properties, property) == 0)
      return 0;
    
    properties++;
    }

  /* property not found */
  return 1;
  }

int cluster_check_fit(plan_cluster* cl, node_info* ninfo)
  {
  return cl -> cluster_properties != filter_exclude(ninfo->get_phys_prop(), 0);
  }

int check_cluster_suitable(server_info* p_info, plan_cluster* cl, plan_job* job)
  {
  int req_num_nodes;

  bool isbackfill = false;
  
  if (job->req_scratch_local == -2)
    return 1;

  if (is_prefix(job->jinfo->queue->name,"backfill") and cl -> cluster_properties.find("q_backfill") != cl -> cluster_properties.end() )
    isbackfill = true;

  if (!isbackfill)
    {
    if (job->jinfo->queue->required_property != NULL && !is_prefix(string(job->jinfo->queue->required_property),"q_")
        && cl -> cluster_properties.find(string(job->jinfo->queue->required_property)) == cl -> cluster_properties.end())
      return 1;

    if (strcmp(job->jinfo->queue->name, conf.default_queue) == 0 || is_prefix(job->jinfo->queue->name,"q_") || is_prefix(job->jinfo->queue->name,"backfill"))
      for (int j = 0; j < p_info->num_queues; ++j)
	if (p_info->queues[j]->required_property != NULL && strlen(p_info->queues[j]->required_property) > 0)
          if (!is_prefix(p_info->queues[j]->required_property,"q_")
              && cl -> cluster_properties.find(string(p_info->queues[j]->required_property)) != cl -> cluster_properties.end())
	    {
            /* pokud na clusteru tato fronta explicitne je tak to nezakazujem, muze tam*/
            if (strcmp(job->jinfo->queue->name, p_info->queues[j]->name) == 0)
              continue;

	    return 1;
            }
    }

  const char *node_spec = strdup((char*)job -> jinfo -> nodespec.c_str());
  if ( node_spec == NULL || node_spec[0] == '\0')
    {
    node_spec = "1:ppn=1";
    }

  /* re-parse nodespec */
  pars_spec *spec;
  if ((spec = parse_nodespec(node_spec)) == NULL)
    {
    free((char *)node_spec);
    return SCHD_ERROR;
    }
  
  free((char *)node_spec);

  pars_prop *iter;
  iter = spec -> nodes -> properties;
  while (iter != NULL)
    {
    if (iter -> value != NULL)
      {
      iter = iter -> next;
      continue;
      }
        
    if (iter -> name[0] == '^')
      {
      if ( cl -> cluster_properties.find(string(iter -> name + 1)) != cl -> cluster_properties.end())
        {
        free_parsed_nodespec(spec);
        return 1;
	}
      }
    else
      {
      if ( cl -> cluster_properties.find(string(iter -> name)) == cl -> cluster_properties.end())
        {
        free_parsed_nodespec(spec);
        return 1;
        }
      }

    iter = iter -> next;
    }

  if (job->jinfo->placement == "" and job->jinfo->placement[0] == '^')
    if ( cl -> cluster_properties.find(string(job->jinfo->placement.c_str() + 1)) != cl -> cluster_properties.end())
      {
      free_parsed_nodespec(spec);
      return 1;
      }

  free_parsed_nodespec(spec);

  req_num_nodes = job -> req_num_nodes;

  for (int i = 0; i < cl -> num_nodes; i++)
    if (cl -> nodes[i] -> get_cores_total() >= job -> req_ppn)
      req_num_nodes = req_num_nodes - (cl -> nodes[i] -> get_cores_total() / job -> req_ppn);

  if (req_num_nodes <= 0)
    return 0;
  
  Resource *res;
  if (job->req_gpu > 0)
    {
    //v clusteru musi byt uzel s GPU
    for (int i = 0; i < cl -> num_nodes; i++)
      {
      res = cl -> nodes[i]->get_resource("gpu");
      if (res)
        if (job->req_gpu <= res->get_capacity())
          return 1;
      }
      // neni tu
     
    return 0;
    }
  
  return 1;
  }

long get_walltime_limit_min(std::set<std::string> properties)
  {
  std::set<std::string> excluded_properties = filter_exclude(properties, 1);
  std::set<std::string>::const_iterator it;

  long ret = 0;

  for (it = excluded_properties.begin(); it != excluded_properties.end(); it++)
    {
    if (is_prefix(*it,"min_"))
      ret = decode_time(strdup((*it).c_str()+4));
    }

  return ret;
  }

long get_walltime_limit_max(std::set<std::string> properties)
  {
  std::set<std::string> excluded_properties = filter_exclude(properties, 1);
  std::set<std::string>::const_iterator it;

  long ret = 0;

  for (it = excluded_properties.begin(); it != excluded_properties.end(); it++)
    {
    if (is_prefix(*it,"max_"))
      ret = decode_time(strdup((*it).c_str()+4));
    }

  return ret;
  }

int cluster_add_node(sched* schedule, int clus_number , node_info* node)
  {
  plan_cluster* cl = schedule -> clusters[clus_number];
  if (cl == NULL)
    return 1;

  cl -> num_nodes++;
  if ((cl->nodes = (node_info**) realloc(cl -> nodes, (cl -> num_nodes) * sizeof(node_info*))) == NULL)
    {
    perror("Memory Allocation Error");
    return 1;
    }

  if ((cl->nodes_name = (char**) realloc(cl -> nodes_name, (cl -> num_nodes) * sizeof(char*))) == NULL)
    {
    perror("Memory Allocation Error");
    return 1;
    }

  (cl -> nodes)[cl -> num_nodes -1] = node;
  (cl -> nodes_name)[cl -> num_nodes -1] = strdup(node->get_name());

  schedule -> clusters[clus_number] -> num_cpus += node -> get_cores_total();

  //running cpus update
  if (! (node ->is_down() ||  node ->is_offline() || node ->is_notusable()))
    {
    schedule -> clusters[clus_number] -> num_running_cpus += node -> get_cores_free();
    schedule -> clusters[clus_number] -> available_ram += get_node_mem(node);
    }

  return 0;
  }

int plan_set_cluster_server(sched* schedule, int clus_number, server_info* sinfo)
  {
  if (schedule -> clusters[clus_number] == NULL)
    return 1;

  if (schedule -> clusters[clus_number] -> sinfo == sinfo)
    return 0;

  schedule -> clusters[clus_number] -> sinfo = sinfo;
  schedule -> jobs[clus_number] -> sinfo = sinfo;
  schedule -> gaps[clus_number] -> sinfo = sinfo;

  return 0;
  }

int plan_find_cluster_for_node(sched* schedule,  server_info* sinfo, node_info* ninfo)
  {
/*
  char **node_properties;
  node_properties =  ninfo->properties;
  if (node_properties != NULL)
    while (*node_properties != NULL)
      {
  	  node_properties++;
      }
*/
  if (schedule == NULL || ninfo == NULL)
    return 1;

  if (ninfo->cluster_name == NULL)
    ninfo->cluster_name = strdup("");

  for (int i = 0; i < schedule -> num_clusters; i++)
    if (strcmp(schedule -> clusters[i] -> cluster_name, ninfo -> cluster_name) == 0)
      {
      for (int j = 0; j < schedule -> clusters[i] -> num_nodes; j++)
        if (strcmp(schedule -> clusters[i] -> nodes_name[j], ninfo -> get_name()) == 0)
          {

          //free_node_info(schedule -> clusters[i] -> nodes[j]);
          schedule -> clusters[i] -> nodes[j] = ninfo;
          plan_set_cluster_server(schedule, i, sinfo);

          return 0;
          }

      if (cluster_check_fit(schedule -> clusters[i], ninfo) == 0)
        {
        cluster_add_node(schedule, i, ninfo);
        sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "Node added", "%s to known cluster %d",ninfo -> get_name(), i);
        plan_set_cluster_server(schedule, i, sinfo);
        return 0;
        }
      }

  schedule_add_cluster(schedule, sinfo, ninfo -> cluster_name ,ninfo -> get_phys_prop());
  cluster_add_node(schedule, schedule -> num_clusters-1, ninfo);
  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "Cluster created","%s added to newly created cluster %d", ninfo -> get_name(), schedule -> num_clusters-1);
  plan_set_cluster_server(schedule, schedule -> num_clusters - 1, sinfo);
  
  return 0;
  }

int plan_update_clusters(sched* schedule,  server_info* sinfo)
  {
  for (int i = 0; i < sinfo -> num_nodes; i++)
    plan_find_cluster_for_node(schedule, sinfo, sinfo -> nodes[i]);

  return 0;
  }

sch_resource_t job_get_scratch_local(JobInfo *jinfo)
  {
  unsigned long long scratch = 0;
  /* read the nodespec, has to be sent from server */
  const char *node_spec = strdup(jinfo->nodespec.c_str());
  if ( node_spec == NULL || node_spec[0] == '\0')
    {
    sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER,
    		jinfo->job_id.c_str(),"No nodespec was provided for this job, assuming 1:ppn=1.");
    node_spec = "1:ppn=1";
    }
	  /* re-parse the nodespec */
  pars_spec *spec;
  if ((spec = parse_nodespec(node_spec)) == NULL)
    {
    free((char*) node_spec);
    return -1;
    }
  
  scratch = spec->nodes->scratch;
  
  if (spec->nodes->scratch_type == ScratchSSD)
    scratch = -2;
  else 
    if (spec->nodes->scratch_type == ScratchShared) 
      scratch = -2;

  free_parsed_nodespec(spec);
  free((char*) node_spec);
  
  return scratch;
  }

JobInfo** plan_known_jobs_update(sched* schedule,  server_info* sinfo, time_t time_now)
  {
  int job_count;
  unsigned int found;
  int job_id=0;
  int new_jobs_counter;
  int new_jobs_counter_remote;
  JobInfo** new_jobs;
  JobInfo* jinfo;
  plan_job* job;
  resource_req *res_walltime = NULL;

  if (sinfo -> jobs.empty())
    return NULL;

  found = 0;
  new_jobs = NULL;
  job_count = 0;
  new_jobs_counter = 0;
  new_jobs_counter_remote = 0;

  job_count = sinfo->jobs.size();

  sort(&(sinfo->jobs[0]), &(sinfo->jobs[job_count-1])+1, compare_fairshare);

  job_count = 0;  
  for (std::vector<JobInfo*>::iterator it = sinfo -> jobs.begin(); it != sinfo -> jobs.end(); it++)
    {
    found = 0;
    jinfo = *it;
    job_id = parse_head_number(jinfo -> job_id.c_str());

    for (int i = 0; i < schedule -> num_clusters; i++)
      {
      schedule -> jobs[i] -> current= NULL;

      while (list_get_next(schedule -> jobs[i]) != NULL)
        if (((plan_job*)schedule -> jobs[i] -> current) -> job_id == job_id)
          {
          found = 1;
          job = (plan_job*)schedule -> jobs[i] -> current;

          //free_job_info(job -> jinfo);

          //jinfo->preprocess();

          job -> jinfo = jinfo;

          res_walltime = find_resource_req(jinfo -> resreq, "walltime");
          if (res_walltime == NULL) {
            job -> estimated_processing = DEFAULT_WALLTIME;
          } 
        else 
          {
            job -> estimated_processing = res_walltime -> amount;
          }

          job -> completion_time = job -> start_time + job -> estimated_processing;

          job -> req_mem = find_resource_req(jinfo -> resreq , "mem") -> amount / job -> req_num_nodes;

          //job -> req_scratch_local = job_get_scratch(jinfo) / job -> req_num_nodes;
          job -> req_scratch_local = 0;

          /* hotovy job smazat z rozvrhu */
          if (jinfo -> state == JobCompleted)
            {
            fairshare_add_job(job, schedule -> clusters[i]->num_running_cpus, schedule -> clusters[i]->available_ram );
            list_remove_item(schedule -> jobs[i], schedule -> jobs[i] -> current, 1);
            fairshare_log();
            }

          //pokud je job bezici ale v tomto okamziku ma nebo uz mel koncit dame mu cas ukonceni now+1
          if ((jinfo -> state == JobRunning || jinfo -> state == JobExiting)
               && job -> completion_time <= time_now)
            job -> completion_time = time_now+1;

          /* pokud budem planovat pres klastry tak se musi nasledujici radek zakomentovat */
          i = schedule -> num_clusters;
          break;
          }
      }

    if (jinfo -> state == JobCompleted)
      found = 1;

    if (jinfo -> state != JobRunning && jinfo -> state != JobQueued)
      found = 1;

    // jestli je job remote tak nic
    if (jinfo -> state == JobRunning && string(jinfo->job_id).find(conf.local_server) == string::npos)
      found = 1;
    
    if (jinfo -> state == JobQueued && string(jinfo->job_id).find(conf.local_server) == string::npos)
      {
      //skip remote job array
      if (jinfo->job_id.find('-') != std::string::npos and jinfo->job_id.find('-') < jinfo->job_id.find('.'))
        {
        found = 1;
        }
      else
        {
        new_jobs_counter_remote++;
        if (new_jobs_counter_remote > 10)
          found = 1;    
        }
      }

    if (found == 0 && new_jobs_counter < conf.new_jobs_limit_per_cycle)
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, jinfo -> job_id.c_str(), "new job");
      new_jobs_counter++;

      if (jinfo ->state == JobQueued)
         sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "new_queued_job", "%s %s", jinfo -> job_id.c_str(), jinfo ->account.c_str());

      if ((new_jobs = (JobInfo**) realloc(new_jobs, (new_jobs_counter + 1) * sizeof(JobInfo*))) == NULL)
        {
        perror("Memory Allocation Error");
        return NULL;
        }

      new_jobs[new_jobs_counter - 1] = jinfo;
      new_jobs[new_jobs_counter] = NULL;
      }

    job_count++;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "New jobs", " count: %d", new_jobs_counter);

  return new_jobs;
  }