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

#ifndef PLAN_DATA_TYPES_H
#define PLAN_DATA_TYPES_H

typedef enum {Gaps, Jobs, Limits} listtype;

/*
 * main plan structure
 */

typedef struct sched sched;

/*
 * each cluster consits of plan_list of plan_job, plan_list of plan_gap and plan_list of plan_limit
 */
typedef struct plan_cluster plan_cluster;

typedef struct plan_list plan_list;

typedef struct plan_job plan_job;
typedef struct plan_gap plan_gap;
typedef struct plan_limit plan_limit;

/*
 * plan_gap_mem is structure within plan_gap
 */
typedef struct plan_gap_mem plan_gap_mem;
typedef struct first_free_mem first_free_mem;

/*
 * plan_limit_account is structure within plan_limit
 */
typedef struct plan_limit_account plan_limit_account;
typedef struct plan_limits plan_limits;

/*
 * plan_eval is structure used during evaluation plan
 */
typedef struct plan_eval plan_eval;

#include <set>
#include <string>

#include "plan_config.h"

/*!
 * Struktura uchovávající celý rozvrh
 */
struct sched
  {
  int num_clusters;

  plan_cluster** clusters;

  plan_list** jobs;

  plan_list** gaps;

  plan_list** limits;

  unsigned* updated;
  };

struct plan_cluster
  {
  char *cluster_name;

  int num_properties;

  std::set<std::string> cluster_properties;

  server_info *sinfo;

  int num_nodes;

  node_info **nodes;

  char **nodes_name;

  int num_cpus;

  int num_running_cpus;

  int num_queued_jobs;

  long int available_ram;
  };

/*!
 * Seznam, tvoří jej prvky jobu nebo děr.
 */
struct plan_list
  {
  server_info *sinfo;

  listtype type;

  int num_items;

  void* first;

  void* last;

  void* current;
  };

/*!
 * Prvek seznamu naplánovaných jobů
 */
struct plan_job
  {
  int job_id;

  JobInfo *jinfo;

  time_t start_time;

  time_t completion_time;

  time_t estimated_processing;

  time_t due_date;
  
  time_t available_after;

  int run_me;

  int usage;

  int req_ppn;

  int original_req_ppn;

  int req_num_nodes;
  
  sch_resource_t req_mem;

  sch_resource_t req_scratch_local;
  
  int req_gpu;

  node_info **ninfo_arr;

  char **fixed_nname_arr;

  char **fixed_nname_arr_backup;

  int *cpu_indexes;

  int *latest_ncpu_index;

  char *sch_nodespec;

  unsigned is_paused: 1;

  struct plan_job *successor;

  struct plan_job *predeccessor;

  int account_id;
  };

/*!
 * Prvek seznamu děr.
 */
struct plan_gap
  {
  time_t start_time;

  time_t end_time;

  time_t duration;

  int usage;

  int num_nodes;

  struct plan_gap_mem **nodes_memory;

  struct plan_job *following_job;

  struct plan_gap *successor;

  struct plan_gap *predeccessor;
  };

/*!
 * Uchovává dostupnou pamět relevantních uzlů pro každou díru.
 */
struct plan_gap_mem
  {
  node_info *ninfo;

  int num_cpus;

  long int available_mem;

  long int available_scratch_local;

  long int tmp_used_cpus;

  int updated;
  };

/*!
 * First Free Slot
 */
struct first_free_slot
  {
  time_t time;

  node_info *ninfo;

  int free_to_run;

  int *releated_cpus;

  sch_resource_t mem;

  sch_resource_t scratch_local;

  sch_resource_t available_mem;

  sch_resource_t available_scratch_local;

  sch_resource_t mem_used_per_cpu;

  sch_resource_t scratch_local_used_per_cpu;

  int updated;
  };

struct plan_limit_account
  {
  int account_id;

  int* num_cpus;

  };

struct plan_limit
  {
  time_t timestamp;

  struct plan_limit_account **accounts;

  int num_accounts;

  int capacity;

  struct plan_limit *successor;

  struct plan_limit *predeccessor;
  };

struct plan_eval
  {
  time_t start_time;

  time_t end_time;

  int num_jobs;

  double response_time;

  double wait_time;

  double slowdown;

  double sum_response_time;

  double sum_wait_time;

  double sum_slowdown;

  double fairness;
  };

#endif
