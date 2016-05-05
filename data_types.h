/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/

#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <vector>
#include <string>
#include <map>

#include <time.h>


enum ClusterMode { ClusterNone, ClusterCreate, ClusterUse };

#include "torque.h"
#include "constant.h"
#include "config.h"
extern "C" {
#include "nodespec.h"
#include "site_pbs_cache.h"
}

#include "logic/LogicFwd.h"

#include "boost/shared_ptr.hpp"
#include "boost/weak_ptr.hpp"

struct server_info;

struct state_count;

struct queue_info;

struct node_info;

struct JobInfo;

struct resource_req;

struct holiday;

struct prev_job_info;

struct group_info;

struct usage_info;

struct token;

typedef struct state_count state_count;

typedef struct server_info server_info;

typedef struct queue_info queue_info;

typedef struct JobInfo JobInfo;

typedef struct node_info node_info;

typedef struct resource_req resource_req;

typedef struct usage_info usage_info;

typedef struct group_info group_info;

typedef struct prev_job_info prev_job_info;

typedef struct token token;

/* since resource values and usage values are linked */


typedef enum {
  MagratheaStateNone,
  MagratheaStateRemoved,
  MagratheaStateDown,
  MagratheaStateDownBootable,
  MagratheaStateBooting,
  MagratheaStateFree,
  MagratheaStateFreeBootable,
  MagratheaStateOccupiedWouldPreempt,
  MagratheaStateOccupied,
  MagratheaStateRunningPreemptible,
  MagratheaStateRunningPriority,
  MagratheaStateRunning,
  MagratheaStateRunningCluster,
  MagratheaStatePreempted,
  MagratheaStateFrozen,
  MagratheaStateDownDisappeared,
  MagratheaStateShuttingDown
} MagratheaState;


struct token
  {
  char* identifier;             /* Token identifier */
  float count;                  /* The number of tokens available of type identifier */
  };


#include "ServerInfo.h"
#include "SchedulerCore_StateCount.h"

struct queue_info
  {
  /* queue is started - can be scheduled */
  bool is_started;

unsigned is_exec:
  1;  /* is the queue an execution queue */

unsigned is_route:
  1;  /* is the queue a routing queue */

unsigned is_ok_to_run:
  1; /* is it ok to run jobs in this queue */
  
unsigned is_global:
  1; /* jobs can be moved to other servers from this queue */

unsigned dedtime_queue:
  1; /* jobs can run in dedicated time */
  
unsigned excl_nodes_only:
  1; /**< run on exclusive nodes only */

unsigned is_admin_queue : 1; /* admin job queue */

  struct server_info *server;   /* server where queue resides */
  char *name;                   /* queue name */
  Scheduler::Core::StateCount sc;               /* number of jobs in different states */
  int max_run;                  /* max jobs that can run in queue */
  int max_user_run;             /* max jobs that a user can run in queue */
  int max_group_run;            /* max jobs that a group can run in a queue */
  int max_proc;                 /* max processors that can be consumed by this queue */
  int max_user_proc;            /* max processors that can be consumed by a user */
  int max_group_proc;           /* max processors that can be consumed by a group */
  int priority;                 /* priority of queue */
  time_t starving_support;      /* time required for jobs to starve */

  std::vector<JobInfo*> jobs; /* array of jobs that reside in queue OWNED */
  std::vector<JobInfo*> running_jobs; /* array of jobs in the running state */

  unsigned excl_node_count;
  unsigned excl_node_capacity;
  node_info **excl_nodes;

  char *fairshare_tree;
  
  char *required_property;

  double queue_cost;

  void update_on_job_run(JobInfo *j);

  long long int number_of_running_jobs_for_group(const std::string& group_name) const;
  long long int number_of_running_jobs_for_user(const std::string& user_name) const;
  long long int number_of_running_cores() const;
  long long int number_of_running_cores_for_group(const std::string& group_name) const;
  long long int number_of_running_cores_for_user(const std::string& user_name) const;

private:
  mutable std::unordered_map<std::string,long long int> running_jobs_by_group; // cache
  mutable std::unordered_map<std::string,long long int> running_jobs_by_user; // cache
  mutable std::unordered_map<std::string,long long int> running_cores_by_group; // cache
  mutable std::unordered_map<std::string,long long int> running_cores_by_user; // cache
  mutable std::pair<bool,long long int> running_cores; // cache
  };

#include "JobInfo.h"
#include "NodeInfo.h"

struct resource_req
  {
  char *name;   /* name of the resource */
  sch_resource_t amount; /* numeric value of resource */
  char *res_str;  /* string value of resource */

  struct resource_req *next; /* next resource_req in list */
  };

struct prev_job_info
  {
  char *name;   /* name of job */
  char *account;  /* account name of user */
  group_info *ginfo;  /* ginfo of user in fair share tree */
  resource_req *resused; /* resources used by the job */
  };

struct mom_res
  {
  char name[MAX_RES_NAME_SIZE];  /* name of resources for addreq() */
  char ans[MAX_RES_RET_SIZE];  /* what is returned from getreq() */

unsigned eol:
  1;   /* set for sentinal value */
  };

/* global data types */

/* This structure is used to write out the usage to disk */

struct t
  {

unsigned hour :
  5;

unsigned min :
  6;

unsigned none :
  1;

unsigned all  :
  1;
  };

struct timegap
  {
  time_t from;
  time_t to;
  };

struct config
  {
  /* these bits control the scheduling policy
   * prime_* is the prime time setting
   * non_prime_* is the non-prime setting
   */

unsigned prime_rr :
  1; /* round robin through queues*/

unsigned non_prime_rr :
  1;

unsigned prime_bq :
  1; /* by queue */

unsigned non_prime_bq :
  1;

unsigned prime_sf :
  1; /* strict fifo */

unsigned non_prime_sf :
  1;

unsigned prime_fs :
  1; /* fair share */

unsigned non_prime_fs :
  1;

unsigned prime_lb :
  1; /* load balancing */

unsigned non_prime_lb :
  1;

unsigned prime_hsv :
  1; /* help starving jobs */

unsigned non_prime_hsv:
  1;

unsigned prime_sq :
  1; /* sort queues by priority */

unsigned non_prime_sq :
  1;

unsigned prime_lbrr :
  1; /* round robin through load balanced nodes */

unsigned non_prime_lbrr:
  1;
  
unsigned ignore_remote_local :
 1; /**<  ignore remote local queues
          only check global queues on remote servers */
unsigned move_jobs :
 1; /** < job moving */

unsigned priority_fairshare :
 1; /** < fairshare with queue priority */

  struct sort_info *sort_by;  /* current sort */

  struct sort_info *prime_sort;  /* prime time sort */

  struct sort_info *non_prime_sort; /* non-prime time sort */
  time_t half_life;   /* half-life time in seconds */
  time_t sync_time;   /* time between syncing usage to disk */

  struct t prime[HIGH_DAY][HIGH_PRIME]; /* prime time start and prime time end*/
  int holidays[MAX_HOLIDAY_SIZE]; /* holidays in julian date */
  int num_holidays;   /* number of acuall holidays */

  struct timegap ded_time[MAX_DEDTIME_SIZE]; /* dedicated times */
  int holiday_year;   /* the year the holidays are for */
  int unknown_shares;   /* unknown group shares */
  int log_filter;   /* what events to filter out */
  char global_prefix[PBS_MAXQUEUENAME +1]; /* prefix to global queues */
  char local_server[PBS_MAXSERVERNAME +1]; /* local server name */
  char ded_prefix[PBS_MAXQUEUENAME +1]; /* prefix to dedicated queues */
  time_t max_starve;   /* starving threshold */
  char* ignored_queues[MAX_IGNORED_QUEUES]; /* list of ignored queues */
  char* slave_servers[MAX_SLAVE_SERVERS]; /* list of slave servers */
  int max_user_run;

  time_t max_cycle; /* maximum cycle length */
  
  /* plan-based scheduler configuration */
  char default_queue[PBS_MAXQUEUENAME +1];
  time_t config_loaded;/* when was this configuration loaded*/
  int optim_minimal_queued;
  int new_jobs_limit_per_cycle;
  int optim_duration_limit;
  int optim_timeout;

  //std::vector<int> *limits_tresholds;
  std::map< std::string, std::map< long, struct cluster_limits_arrays> > *limits_for_clusters;  
  };

/* for description of these bits, check the PBS admin guide or scheduler IDS */

struct sort_info;

struct status
  {

unsigned round_robin:
  1;

unsigned by_queue:
  1;

unsigned strict_fifo:
  1;

unsigned fair_share:
  1;

unsigned load_balancing:
  1;

unsigned load_balancing_rr:
  1;

unsigned help_starving_jobs:
  1;

unsigned sort_queues:
  1;

unsigned is_prime:
  1;

unsigned is_ded_time:
  1;

  struct sort_info *sort_by;

  time_t current_time;

  JobInfo *starving_job; /* the most starving job */

  };

struct cluster_limits_arrays
  {
	std::vector<float> percentages;
	std::vector<float> percentages_global;
	std::vector<long> tresholds;
  };
#endif
