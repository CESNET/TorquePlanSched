#include "data_types.h"
#include "node_info.h"
#include "job_info.h"
#include "misc.h"
#include "globals.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "site_pbs_cache_scheduler.h"
extern "C" {
#include "nodespec.h"
}

#include "api.hpp"

#include "SchedulerCore_RescInfoDb.h"

#include "logic/NodeFilters.h"
#include "NodeSort.h"
#include "base/PropRegistry.h"
#include "base/MiscHelpers.h"
using namespace Scheduler;
using namespace Core;

#include <sstream>
#include <cassert>
#include <cmath>
#include <vector>
#include <algorithm>
#include <iterator>
#include <stdexcept>
using namespace std;

static int assign_node(JobInfo *jinfo, pars_spec_node *spec, const vector<node_info*>& suitable_nodes)
  {
  ScratchType scratch = ScratchNone;
  int fit_occupied = 0;
  repository_alternatives *ra = NULL;
  CheckResult node_test;

  for (size_t i = 0; i < suitable_nodes.size(); i++) /* for each node */
    {
    if (suitable_nodes[i]->physical_node()->has_virtual_assignment() ||
        suitable_nodes[i]->has_assignment())
      continue;

    // jobs requesting runs
    if (jinfo->cluster_mode != ClusterCreate)
      {
      if ((node_test = suitable_nodes[i]->can_fit_job_for_run(jinfo,spec,&scratch)) != CheckAvailable)
        {
        ++fit_occupied;
        continue;
        }
      }
    else if (jinfo->cluster_mode == ClusterCreate)
      {
      if ((node_test = suitable_nodes[i]->can_fit_job_for_boot(jinfo,spec,&scratch,&ra)) != CheckAvailable)
        {
        ++fit_occupied;
        continue;
        }
      }

    suitable_nodes[i]->set_assignment(spec);
    if (jinfo->is_exclusive()) // allocate all processors and memory, when job is exclusive
      {
      suitable_nodes[i]->get_assignment()->procs = suitable_nodes[i]->get_cores_total();
      suitable_nodes[i]->get_assignment()->mem   = suitable_nodes[i]->get_mem_total();
      }
    suitable_nodes[i]->set_scratch_assign(scratch);

    if (ra != NULL)
      suitable_nodes[i]->set_selected_alternative(ra);
    else
      suitable_nodes[i]->set_selected_alternative(NULL); /* FIXME META Prepsat do citelneho stavu */

    jinfo->schedule.push_back(suitable_nodes[i]);

    return 0;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->job_id.c_str(), "Nodespec not matched: %d suitable, %d occupied.", suitable_nodes.size(), fit_occupied);
  return 1;
  }


static int ondemand_reboot(JobInfo *jinfo, pars_spec_node *spec, const vector<node_info*>& suitable_nodes)
  {
  ScratchType scratch = ScratchNone;
  int fit_occupied = 0;
  repository_alternatives *ra = NULL;
  CheckResult node_test;

  if (jinfo->cluster_mode != ClusterNone) // only for normal jobs
    return 2;

  for (size_t i = 0; i < suitable_nodes.size(); i++) /* for each node */
    {
    if (suitable_nodes[i]->physical_node()->has_virtual_assignment() ||
        suitable_nodes[i]->has_assignment())
      continue;

    if ((node_test = suitable_nodes[i]->can_fit_job_for_boot(jinfo,spec,&scratch,&ra)) != CheckAvailable)
      {
      ++fit_occupied;
      continue;
      }

    suitable_nodes[i]->physical_node()->set_assignment(spec);
    suitable_nodes[i]->physical_node()->set_scratch_assign(scratch);

    if (ra != NULL)
      suitable_nodes[i]->physical_node()->set_selected_alternative(ra);
    else
      suitable_nodes[i]->physical_node()->set_selected_alternative(NULL); /* FIXME META Prepsat do citelneho stavu */

    jinfo->schedule.push_back(suitable_nodes[i]->physical_node());

    return 0;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->job_id.c_str(), "Nodespec not matched: %d suitable, %d occupied.", suitable_nodes.size(), fit_occupied);
  return 1;
  }

#if 0
static pars_spec_node* merge_node_specs(pars_spec_node *first, pars_spec_node *second)
  {
  pars_spec_node *result = clone_pars_spec_node(first);
  result->mem = max(result->mem,second->mem);
  result->vmem = max(result->vmem,second->vmem);
  result->procs = max(result->procs,second->procs);
  result->scratch = max(result->scratch,second->scratch);

  pars_prop *iter = second->properties;
  while (iter != NULL)
    {
    if (iter->value == NULL)
      { iter = iter->next; continue; }

    if (res_check_type(iter->name) != ResCheckNone)
      {
      pars_prop *iter2 = result->properties;
      while (iter2 != NULL && strcmp(iter2->name,iter->name) != 0)
        { iter2 = iter2->next; }

      if (iter2 == NULL)
        {
        pars_prop *prop = init_pars_prop();
        prop->name = strdup(iter->name);
        prop->value = strdup(iter->value);
        if (result->properties_end == NULL)
          {
          result->properties = prop;
          result->properties_end = prop;
          }
        else
          {
          result->properties_end->next = prop;
          prop->prev = result->properties_end;
          result->properties_end = prop;
          }
        }
      else
        {
        sch_resource_t value1 = res_to_num(iter->value);
        sch_resource_t value2 = res_to_num(iter2->value);

        if (value1 >= value2)
          {
          free(iter2->value);
          iter2->value = strdup(iter->value);
          }
        }
      }
    iter = iter->next;
    }

  return result;
  }


static int assign_all_nodes(JobInfo *jinfo, pars_spec_node *spec, const vector<node_info*>& nodes)
  {
  int starving = 0;

  vector<node_info*> suitable_nodes;
  NodeSuitableForSpec::filter_starving(nodes,suitable_nodes,jinfo,spec);

  // ADD VIRTUAL NODES HERE

  for (size_t i = 0; i < suitable_nodes.size(); i++) /* for each node */
    {
    node_info *node = suitable_nodes[i];
    if (node->has_starving_assignment())
      {
      pars_spec_node *starved = node->get_starving_assignment();
      jinfo->unplan_from_node(node,starved);

      // join specs
      pars_spec_node *joined = merge_node_specs(spec,starved);
      jinfo->plan_on_node(node,joined);
      free_pars_spec_node(&starved);
      node->set_starving_assignment(joined);
      free_pars_spec_node(&joined);
      }
    else
      {
      jinfo->plan_on_node(node,spec);
      node->set_starving_assignment(spec);
      }
    ++starving;
    }

  sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_JOB, jinfo->name, "Starving job: %d total nodes, %d starved nodes.", nodes.size(), starving);
  return 1;
  }
#endif

CheckResult try_assign_nodes(JobInfo *jinfo, pars_spec_node *spec, const vector<node_info*>& run_nodes, const vector<node_info*>& boot_nodes)
  {
  for (unsigned i = 0; i < spec->node_count; i++)
    {
    int run_ret = 2;
    int boot_ret = 2;

    if (run_nodes.size() > 0)
      {
      // first try normal execution
      // if successful, just continue to next requested node
      if ((run_ret = assign_node(jinfo,spec,run_nodes)) == 0)
        continue;
      }

    // if there are some rebootable nodes and we didn't find any normal nodes, try ondemand reboot
    if (boot_nodes.size() > 0)
      {
      // if successful, just continue to next requested node
      if ((boot_ret = ondemand_reboot(jinfo,spec,boot_nodes)) == 0)
        continue;
      }

    // Not ever possible to run, or on-demand boot request
    if (run_ret == 2 && boot_ret == 2)
      return CheckNonFit;

    // this state should never be reached for starving jobs
    return CheckOccupied;
    }

  // all requested nodes were successfuly assigned
  return CheckAvailable;
  }

CheckResult try_assign_spec(JobInfo *jinfo, const vector<node_info*>& nodes)
  {
  CheckResult result = CheckAvailable;
  pars_spec_node *iter = jinfo->parsed_nodespec()->nodes;

  while (iter != NULL)
    {
    // nodes suitable for nodespec
    vector<node_info*> fit_nodes, reboot_nodes;
    //NodeSuitableForSpec::filter_assign(nodes,fit_nodes,jinfo,iter);
    NodeSuitableForSpec::filter_reboot(nodes,reboot_nodes,jinfo,iter);
    
    /* plan-based sched */
    fit_nodes = nodes;

    // count unique nodes
    set<string> unique_jobs;

    for (size_t i = 0; i < fit_nodes.size(); i++)
      unique_jobs.insert(fit_nodes[i]->get_name());

    for (size_t i = 0; i < reboot_nodes.size(); i++)
      unique_jobs.insert(reboot_nodes[i]->get_name());

    if (unique_jobs.size() < iter->node_count)
      return CheckNonFit;

    // sort nodes according to state & schedule
    sort(fit_nodes.begin(),fit_nodes.end(),NodeStateSort());
    sort(reboot_nodes.begin(),reboot_nodes.end(),NodeStateSort());

    CheckResult ret;
    if ((ret = try_assign_nodes(jinfo,iter,fit_nodes,reboot_nodes)) == CheckNonFit)
      return CheckNonFit;

    if (ret == CheckOccupied)
      result = CheckOccupied;

    iter = iter->next;
    }

  return result;
  }

CheckResult find_nodes(JobInfo *jinfo, const vector<node_info*>& nodes)
  {
  CheckResult result = CheckNonFit;

  if (jinfo->placement.size() != 0)
    {
    pair<bool,size_t> reg = get_prop_registry()->get_property_id(jinfo->placement);
    if (!reg.first)
      throw runtime_error("Mismatch state, validity of registry should have been already checked.");

    size_t prop_id = reg.second;
    size_t max_val_id = get_prop_registry()->value_count(prop_id).second;

    for (size_t i = 0; i < max_val_id; i++)
      {
      nodes_preassign_clean(nodes);
      jinfo->schedule.clear();

      vector<node_info*> nodes_set;
      back_insert_iterator<vector<node_info*>> it(nodes_set);
      copy_if(nodes.begin(), nodes.end(), it, [prop_id,i](node_info* node) {
        return node->has_reg_prop(prop_id,i);
      });
      //NodeSuitableForPlace::filter(nodes,nodes_set,prop_id,i);

      CheckResult ret;
      if ((ret = try_assign_spec(jinfo,nodes_set)) == CheckAvailable)
        {
        result = CheckAvailable;
        break;
        }
      else if (ret == CheckOccupied)
        {
        result = CheckOccupied;
        }
      }
    }
  else
    {
    result = try_assign_spec(jinfo,nodes);
    }

  return result;
  }

int check_nodespec(server_info *sinfo, JobInfo *jinfo, int nodecount, node_info **ninfo_arr, int )
  {
  vector<node_info*> nodes(&ninfo_arr[0],&ninfo_arr[nodecount]);

  // nodes suitable for job
  vector<node_info*> suitable_nodes;
  NodeSuitableForJob::filter(nodes,suitable_nodes,jinfo);

  CheckResult result = find_nodes(jinfo,suitable_nodes);

  // PROCESS SCHEDULING RESULTS
  if (result == CheckNonFit)
    {
    nodes_preassign_clean(ninfo_arr,nodecount);
    return REQUEST_NOT_MATCHED;
    }

  if (result == CheckOccupied) /* some part of nodespec couldn't be assigned */
    {
    if (jinfo->queue->is_admin_queue)
      {
      return NODESPEC_NOT_ENOUGH_NODES_TOTAL;
      }

    nodes_preassign_clean(ninfo_arr,nodecount);
    jinfo->schedule.clear();

    if (jinfo->is_starving()) /* if starving, eat out the resources anyway */
      {
      // expand virtual nodes;
      vector<node_info*> virtual_nodes;
      for (size_t i = 0; i < suitable_nodes.size(); i++)
      for (size_t j = 0; j < suitable_nodes[i]->get_slave_nodes().size(); j++)
        virtual_nodes.push_back(suitable_nodes[i]->get_slave_nodes()[j].get());

      suitable_nodes.insert(suitable_nodes.end(),virtual_nodes.begin(),virtual_nodes.end());

      result = find_nodes(jinfo,suitable_nodes);

      if (result == CheckOccupied)
        return NODESPEC_NOT_ENOUGH_NODES_TOTAL;

      // plan on nodes & their slave sisters
      for (size_t i = 0; i < jinfo->schedule.size(); i++)
        {
        // plan on the possibly "future" node
        jinfo->plan_on_node(jinfo->schedule[i],jinfo->schedule[i]->get_assignment());
        // propagate resources from the "future" node to all other nodes belonging to the physical node
        jinfo->schedule[i]->physical_node()->propagate_resources(jinfo->schedule[i]);
        }

      jinfo->plan_on_server(sinfo);
      jinfo->plan_on_queue(jinfo->queue); // TODO proper resource counting on queues

      return JOB_SCHEDULED;
      }

    return NODESPEC_NOT_ENOUGH_NODES_TOTAL;
    }

  jinfo->calculated_fairshare = jinfo->calculate_fairshare_cost(suitable_nodes);
  return SUCCESS; /* if we reached this point, we are done */
  }

/** Clean job assignments
 *
 * @param ninfo_arr
 */
void nodes_preassign_clean(node_info **ninfo_arr, int count)
  {
  int i;

  if (ninfo_arr == NULL)
    return;

  for (i = 0; i < count; i++)
    {
    assert(ninfo_arr[i] != NULL);
    ninfo_arr[i]->clean_assign();
    }
  }

void nodes_preassign_clean(const vector<node_info*>& nodes)
  {
  for (size_t i = 0; i < nodes.size(); i++)
    nodes[i]->clean_assign();
  }

/** Construct a full target node specification (hostname:nodespec)
 *
 * @note Expects a valid node info
 */
static void get_target_full(stringstream& s, JobInfo *jinfo, node_info *ninfo, bool cluster)
  {
  assert(jinfo != NULL || ninfo != NULL);

  if (!ninfo->has_assignment())
    return;

  if (jinfo->cluster_mode == ClusterCreate || cluster)
    ninfo->get_assign_string(s,RescOnlyAssignString);
  else
    ninfo->get_assign_string(s,FullAssignString);
  }

/** Get the target string from preassigned nodes
 *
 * @param ninfo_arr List of nodes to parse
 * @return Allocated string containing the targets
 */
char* nodes_preassign_string(JobInfo *jinfo, node_info **ninfo_arr, int count, int &booting, double &minspec)
  {
  stringstream s;
  bool cluster = false;
  bool first = true;
  int i;

  booting = 0;
  minspec = -1;

  assert(ninfo_arr != NULL);

  for (i = 0; i < count && ninfo_arr[i] != NULL; i++)
    {
    if (ninfo_arr[i]->has_assignment() && ninfo_arr[i]->magrathea_status == MagratheaStateBooting)
      {
      booting = 1;
      return NULL;
      }
    }

  for (i = 0; i < count && ninfo_arr[i] != NULL; i++)
    {
    if (ninfo_arr[i]->has_selected_alternative())
      cluster = true;
    }

  for (i = 0; (unsigned)i < jinfo->schedule.size(); i++)
    {
    if (!first) s << "+";
      first = false;
      get_target_full(s,jinfo,jinfo->schedule[i],cluster);

      if (minspec == -1)
        minspec = jinfo->schedule[i]->get_node_spec();
      else
        minspec = min(minspec,jinfo->schedule[i]->get_node_spec());
    }

  if (jinfo->is_exclusive())
    s << "#excl";

  return strdup(s.str().c_str());
  }

/** Get the target string from preassigned nodes - plan-based scheduler
 *
 * @param ninfo_arr List of nodes to parse
 * @return Allocated string containing the targets
 */
char* nodes_preassign_string_new(JobInfo *jinfo, node_info **ninfo_arr, int count, int &booting)
  {
  stringstream s;
  bool first = true;
  int i;

  assert(ninfo_arr != NULL);

  for (i = 0; i < count && ninfo_arr[i] != NULL; i++)
    {
    if (ninfo_arr[i]->has_assignment() && ninfo_arr[i]->magrathea_status == MagratheaStateBooting)
      {
      booting = 1;
      return NULL;
      }
    }

  for (i = 0; i < count && ninfo_arr[i] != NULL; i++)
    {
    if (ninfo_arr[i]->has_assignment())
      {
      if (!first) s << "+";
      first = false;
      get_target_full(s,jinfo,ninfo_arr[i], false);
      }
    }

  if (jinfo->is_exclusive())
    s << "#excl";

  return strdup(s.str().c_str());
  }
