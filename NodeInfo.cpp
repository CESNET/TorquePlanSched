#include <cstring>
#include <climits>
#include "data_types.h"
#include "NodeInfo.h"
#include "server_info.h"
#include "misc.h"
#include "SchedulerCore_RescInfoDb.h"
#include "site_pbs_cache_scheduler.h"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"

using namespace std;
//node_info& node_info::operator = (const node_info& src) { return *this; }

void node_info::propagate_resources(node_info *source)
  {
  // set physical node
  this->p_core_assigned = max(this->p_core_assigned,source->p_core_assigned);
  this->p_resc.join_resources(source->p_resc);

  // go through virtual nodes
  vector< boost::shared_ptr<node_info> >::const_iterator i;
  for (i = this->p_slave_nodes.begin(); i != this->p_slave_nodes.end(); i++)
    {
    (*i)->p_core_assigned = max(source->p_core_assigned,(*i)->p_core_assigned);
    (*i)->p_resc.join_resources(source->p_resc);
    }
  }

//int node_has_enough_resource(node_info *ninfo, char *name, char *value, enum ResourceCheckMode mode);

CheckResult result_helper(bool result)
  {
  if (result)
    return CheckAvailable;
  else
    return CheckNonFit;
  }

CheckResult node_info::has_prop(const pars_prop* property, bool physical_only) const
  {
  bool negative = false;

  assert(property != NULL);

  char *prop_name  = property->name;
  char *prop_value = property->value;

  if (prop_name[0] == '^')
    {
    negative = true;
    prop_name++;
    }

  if (prop_value == NULL) /* property, not a resource */
    {
    if (strcmp(this->get_name(),prop_name) == 0)
      return result_helper(!negative);

    if (this->has_phys_prop(prop_name))
      return result_helper(!negative);

    if (!physical_only)
      {
      if (this->has_virt_prop(prop_name))
        return result_helper(!negative);
      }
    }
  else /* resource or ppn */
    {
    if (strcmp(prop_name,"host") == 0 && strcmp(prop_value, this->get_name()) == 0)
      return result_helper(!negative);

    return has_resc(property);
    }

  return result_helper(negative); /* if negative property and not found return true, otherwise return false */
  }

bool node_info::has_prop(const char* property) const
  {
  char *buf;
  pars_prop *prop;
  CheckResult ret;

  assert(property != NULL);

  buf = strdup(property);
  prop = parse_prop(buf);

  ret = has_prop(prop,true);

  free_pars_prop(&prop);
  free(buf);

  return ret == CheckAvailable;
  }

unsigned long long node_info::get_mem_total() const
{
  Resource *mem = this->get_resource("mem");
  if (mem == NULL)
    return 0;

  return mem->get_capacity();
}

CheckResult node_info::has_props_boot(const JobInfo *, const pars_spec_node *spec, const repository_alternatives *virt_conf) const
  {
  CheckResult result = CheckAvailable;
  pars_prop *prop = spec->properties;

  while (prop != NULL)
    {
    CheckResult test_phys = this->has_prop(prop,true);
    CheckResult test_virt = (alternative_has_property(const_cast<repository_alternatives*>(virt_conf),prop->name) == 0)?CheckNonFit:CheckAvailable;

    if (test_phys == CheckNonFit && test_virt == CheckNonFit)
      return CheckNonFit;

    if (test_phys != CheckAvailable && test_virt != CheckAvailable)
      result = CheckOccupied;

    prop = prop->next;
    }

  return result;
  }

CheckResult node_info::has_props_run(const JobInfo *, const pars_spec_node *spec) const
  {
  CheckResult result = CheckAvailable;
  pars_prop *prop = spec->properties;

  while (prop != NULL)
    {
    CheckResult test = has_prop(prop,false);
    if (test == CheckNonFit)
      return CheckNonFit;
    else if (test == CheckOccupied)
      result = CheckOccupied;

    prop = prop->next;
    }

  return result;
  }

CheckResult node_info::has_spec(const JobInfo *job, const pars_spec_node *spec, ScratchType *scratch) const
  {
  CheckResult proc_result = has_proc(job,spec);
  CheckResult mem_result  = has_mem(job,spec);
  CheckResult scratch_result = has_scratch(job,spec,scratch);

  if (proc_result == CheckNonFit ||
      mem_result == CheckNonFit ||
      scratch_result == CheckNonFit)
    {
    return CheckNonFit;
    }

  if (proc_result == CheckOccupied ||
      mem_result == CheckOccupied ||
      scratch_result == CheckOccupied)
    {
    return CheckOccupied;
    }

  return CheckAvailable;
  }

CheckResult node_info::has_bootable_state(ClusterMode mode) const
  {
  // wrong type of node for booting jobs
  if (this->get_type() != NodeVirtual)
    return CheckNonFit;

  // machine doesn't have configured any virtual images
  if (alternatives == NULL || alternatives[0] == NULL)
    return CheckNonFit;

  // no physical host configured for the virtual node
  if (this->get_host() == NULL)
    return CheckNonFit;

  // configured physical host is not a cloud node
  if (this->get_host()->get_type() != NodeCloud)
    return CheckNonFit;

  // check after & before
  long now = time(NULL);

  if (!this->is_virtual_node()) // for virtual nodes, available after has a special meaning
  if (this->get_avail_after() != 0 && now < this->get_avail_after())
    return CheckNonFit;

  if (this->get_avail_before() != 0 && now > this->get_avail_before())
    return CheckNonFit;

  switch (magrathea_status)
    {
    case MagratheaStateFreeBootable:
      if (!this->is_free() || this->is_down() || this->is_offline() || this->is_unknown())
        return CheckOccupied;
    case MagratheaStateDownBootable:
      // down bootable nodes can only be used for ondemand clusters if rebootable
      if (mode == ClusterNone && (!this->p_is_rebootable))
        return CheckNonFit;
      break;

    // wrong magrathea state (configuration level)
    case MagratheaStateFree:
    case MagratheaStateOccupiedWouldPreempt:
    case MagratheaStateRunning:
    case MagratheaStateRunningPreemptible:
    case MagratheaStateRunningPriority:
    case MagratheaStateDown:
    case MagratheaStateFrozen:
    case MagratheaStateOccupied:
    case MagratheaStatePreempted:
      return CheckNonFit;

    // wrong magrathea state (current status)
    case MagratheaStateBooting:
    case MagratheaStateRunningCluster:
    case MagratheaStateNone:
    case MagratheaStateDownDisappeared:
    case MagratheaStateRemoved:
    case MagratheaStateShuttingDown:
      return CheckOccupied;
    default: break;
    }

  if (this->get_host()->is_down() || this->get_host()->is_offline() || this->get_host()->is_unknown())
    return CheckOccupied;

  int jobs_present = 0;
  if (this->get_host() != NULL)
    {
    if (this->get_host()->has_jobs())
      jobs_present = 1;

    for (size_t i = 0; i < this->get_host()->get_hosted().size(); i++)
      {
      if (this->get_host()->get_hosted()[i]->has_jobs())
        jobs_present = 1;
      }
    }

  if (jobs_present)
    return CheckOccupied;

  return CheckAvailable;
  }

CheckResult node_info::has_runnable_state() const
  {
  // wrong type of node for running jobs
  if (this->get_type() == NodeTimeshared || this->get_type() == NodeCloud)
    return CheckNonFit;

  // check after & before
  long now = time(NULL);

  if (!this->is_virtual_node()) // for virtual nodes, available after has a special meaning
  if (this->get_avail_after() != 0 && now < this->get_avail_after())
    return CheckNonFit;

  if (this->get_avail_before() != 0 && now > this->get_avail_before())
    return CheckNonFit;

  if (this->get_type() == NodeVirtual)
    {
    switch (magrathea_status)
      {
      // wrong magrathea state (configuration level)
      case MagratheaStateDown:
      case MagratheaStateDownBootable:
      case MagratheaStateFrozen:
      case MagratheaStateDownDisappeared:
      case MagratheaStateShuttingDown:
      case MagratheaStateRemoved:
        return CheckNonFit;

      // wrong magrathea current state
      case MagratheaStateNone:
      case MagratheaStateOccupied:
      case MagratheaStatePreempted:
        return CheckOccupied;

      default: break;
      }
    }

  // bad current pbs state
  if (is_offline() || is_down() || is_unknown())
    return CheckOccupied;

  return CheckAvailable;
  }


CheckResult node_info::can_boot_job(const JobInfo *jinfo) const
  {
  if (this->is_notusable())
    return CheckNonFit;

  if (jinfo->placement.size() != 0 && this->get_resource(jinfo->placement.c_str()) == NULL)
    return CheckNonFit;

  CheckResult result = has_bootable_state(jinfo->cluster_mode);
  if (result == CheckNonFit)
    return CheckNonFit;

  long now = time(NULL);
  if (this->get_avail_before() != 0)
  if (this->get_avail_before() - jinfo->get_walltime() <= now)
    return CheckNonFit;

  if (jinfo->cluster_mode == ClusterUse)
    return CheckNonFit;

  if (this->get_nomultinode() && jinfo->is_multinode())
    return CheckNonFit;

  // User does not have an account on this machine - can never run
  if (site_user_has_account(const_cast<char*>(jinfo->account.c_str()),const_cast<char*>(this->get_name()),const_cast<char*>(this->get_phys_cluster().c_str())) == CHECK_NO) // TODO const fix
    return CheckNonFit;

  // Server already installing to many machines
  if (this->get_parent_server()->installing_nodes_overlimit())
    return CheckOccupied;

  // Machine is already allocated to a virtual cluster, only ClusterUse type of jobs allowed
  if (this->get_resource("machine_cluster") != NULL)
    result = CheckOccupied;

  return result;
  }

CheckResult node_info::can_run_job(const JobInfo *jinfo) const
  {
  if (this->is_notusable())
    return CheckNonFit;

  if (jinfo->placement.size() != 0 && this->get_resource(jinfo->placement.c_str()) == NULL)
    return CheckNonFit;

  CheckResult result = has_runnable_state();
  if (result == CheckNonFit)
    return CheckNonFit;

  long now = time(NULL);
  if (this->get_avail_before() != 0)
  if (this->get_avail_before() - jinfo->get_walltime() <= now)
    return CheckNonFit;

  if (jinfo->cluster_mode == ClusterCreate)
    return CheckNonFit;

  // job requires a virtual cluster, node is a physical non-virtual node
  if (this->get_type() == NodeCluster && jinfo->cluster_mode != ClusterNone)
    return CheckNonFit;

  if (this->get_nomultinode() && jinfo->is_multinode())
    return CheckNonFit;

  Resource *res_machine = this->get_resource("machine_cluster");
  if (jinfo->cluster_mode != ClusterUse) /* users can always go inside a cluster */
    {
    // User does not have an account on this machine - can never run
    if (site_user_has_account(const_cast<char*>(jinfo->account.c_str()),const_cast<char*>(this->get_name()),const_cast<char*>(this->get_phys_cluster().c_str())) == CHECK_NO) // TODO const char fix
      return CheckNonFit;

    // Machine is already allocated to a virtual cluster, only ClusterUse type of jobs allowed
    if (res_machine != NULL)
      result = CheckOccupied;
    }
  else
    {
    if (jinfo->cluster_name.size() != 0)
      return CheckNonFit;

    // Check whether this machine is running the cluster user is requesting
    if ((res_machine == NULL) || (res_machine->get_str_val() == ""))
      return CheckNonFit;
    else if (res_machine->get_str_val() != jinfo->cluster_name)
      return CheckNonFit;
    }

  return result;
  }

CheckResult node_info::can_fit_job_for_run(const JobInfo *jinfo, const pars_spec_node *spec, ScratchType *scratch) const
  {
  CheckResult result;
  CheckResult node_test;

  if ((result = can_run_job(jinfo)) == CheckNonFit)
    return CheckNonFit;

  if ((node_test = has_spec(jinfo, spec, scratch)) == CheckNonFit)
    return CheckNonFit;

  if (node_test == CheckOccupied)
    result = CheckOccupied;

  if ((node_test = has_props_run(jinfo,spec)) == CheckNonFit)
    return CheckNonFit;

  if (node_test == CheckOccupied)
    result = CheckOccupied;

  return result;
  }

CheckResult node_info::can_fit_job_for_boot(const JobInfo *jinfo, const pars_spec_node *spec, ScratchType *scratch, repository_alternatives **alternative) const
  {
  CheckResult result;
  CheckResult node_test;
  repository_alternatives** ra = NULL;

  if ((result = can_boot_job(jinfo)) == CheckNonFit)
    return CheckNonFit;

  if ((node_test = has_spec(jinfo, spec, scratch)) == CheckNonFit)
    return CheckNonFit;

  if (node_test == CheckOccupied)
    result = CheckOccupied;

  node_test = CheckNonFit;

  for (ra = alternatives; *ra != NULL; ++ra)
    {
    node_test = this->has_props_boot(jinfo,spec,*ra);
    if (node_test == CheckNonFit)
      {
      continue;
      }
    else if (node_test == CheckOccupied)
      {
      node_test = CheckOccupied;
      continue;
      }
    else
      {
      node_test = CheckAvailable;
      break;
      }
    }

  // *ra == NULL
  if (node_test == CheckNonFit)
    return CheckNonFit;

  // *ra == NULL
  if (node_test == CheckOccupied)
    result = CheckOccupied;

  *alternative = *ra;

  return result;
  }


void node_info::fetch_bootable_alternatives()
  {
  // only virtual nodes can have bootable alternatives
  if (this->get_type() != NodeVirtual)
    return;

  Resource *resc;
  if ((resc = this->get_resource("magrathea")) != NULL)
    {
    this->alternatives = get_bootable_alternatives(const_cast<char*>(this->get_name()),NULL); // TODO const char fix
    }
  }


static int get_magrathea_value(MagratheaState state)
  {
  switch (state)
    {
    case MagratheaStateRunningCluster:
      return 0;

    case MagratheaStateRunning:
    case MagratheaStateRunningPriority:
      return 1;

    case MagratheaStateDownBootable:
      return 2;

    case MagratheaStateFreeBootable:
      return 3;

    case MagratheaStateFree:
      return 4;

    case MagratheaStateRunningPreemptible:
      return 5;

    case MagratheaStateOccupiedWouldPreempt:
      return 6;

    default: return 99;
    }

  return 99;
  }

bool node_info::operator < (const node_info& right)
  {
  // physical nodes go before all virtual nodes
  if ((!this->is_virtual_node()) && right.is_virtual_node())
    return true;
  if (this->is_virtual_node() && (!right.is_virtual_node()))
    return false;

  // virtual nodes are sorted strictly by available time
  if (this->is_virtual_node() && right.is_virtual_node())
    {
    if (this->p_avail_after < right.p_avail_after)
      return true;
    else if (this->p_avail_after > right.p_avail_after)
      return false;
    // if both nodes are available at the same time, continue with the standard comparison
    }

  if (this->get_priority() > right.get_priority()) // bigger number = bigger priority
    return true;
  else if (this->get_priority() < right.get_priority())
    return false;

  /* nodes have the same priority, sort by magrathea status
     - usable for run first
     - usable for boot next
  */

  int left_magrathea = get_magrathea_value(this->magrathea_status);
  int right_magrathea = get_magrathea_value(right.magrathea_status);

  if (this->get_type() != NodeVirtual) left_magrathea = -1;
  if (right.get_type() != NodeVirtual) right_magrathea = -1;

  if (left_magrathea < right_magrathea)
    return true;
  else if (left_magrathea > right_magrathea)
    return false;

  // nodes have the same priority and magrathea status
  // schedule nodes with smaller gaps first

  if (this->get_cores_free() < right.get_cores_free())
    return true;
  else if (this->get_cores_free() > right.get_cores_free())
    return false;

  return strcmp(this->get_name(),right.get_name()) < 0;
  }

void node_info::process_magrathea_status()
  {
  Resource *res_magrathea;
  res_magrathea = this->get_resource("magrathea");

  if (res_magrathea == NULL || magrathea_decode_new(res_magrathea,&this->magrathea_status,this->p_is_rebootable) != 0)
    {
    this->magrathea_status = MagratheaStateNone;
    return;
    }

  if (this->has_jobs())
    {
    // if there are already jobs on this node, the magrathea state can't be free/down-bootable
    if (this->magrathea_status == MagratheaStateDownBootable || this->magrathea_status == MagratheaStateFree)
      {
      this->magrathea_status = MagratheaStateNone;
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_NODE, this->get_name(), "Node had inconsistent magrathea state.");
      return;
      }
    }

  if (this->get_host() != NULL && this->get_host()->has_jobs())
    {
    // if the host already has jobs, the magrathea state can't be down-bootable
    if (this->magrathea_status == MagratheaStateDownBootable)
      {
      this->magrathea_status = MagratheaStateNone;
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_NODE, this->get_name(), "Node had inconsistent magrathea state.");
      return;
      }
    }
  }

void node_info::process_machine_cluster()
  {
  Resource *res_cluster;
  res_cluster = this->get_resource("machine_cluster");

  this->set_virtual_cluster("");
  this->set_virtual_image("");
  this->p_is_building_cluster = false;

  if (res_cluster == NULL)
    return;

  string value = res_cluster->get_str_val();
  size_t pos = value.find(';');

  if (pos != value.npos)
    {
    this->set_virtual_cluster(value.substr(0,pos));
    this->set_virtual_image(value.substr(pos+1,value.length()-pos-1));
    }
  else
    {
    this->set_virtual_cluster(value);
    }

  // determine whether node is currently booting
  if (this->get_virtual_cluster().substr(0,strlen("internal_")) == "internal_")
    {
    this->p_is_building_cluster = true;
    }
  else if (this->magrathea_status != MagratheaStateRunningCluster)
    {
    this->p_is_building_cluster = true;
    }
  }


bool compare_job_ends(JobInfo *left, JobInfo *right)
  {
  return left->completion_time() < right->completion_time();
  }

void node_info::expand_virtual_nodes()
  {
  // nodes cannot be expanded once assigned
  assert(!(this->physical_node()->has_virtual_assignment() || this->has_assignment()));
  // cannot expand already virtual nodes
  assert(!this->is_virtual_node());

  set<JobInfo *,bool(*)(JobInfo*,JobInfo*)> running_jobs(compare_job_ends);

  set<string> running_jobs_names;
  set<string>::const_iterator i;
  for (i = this->get_jobs().begin(); i != this->get_jobs().end(); i++)
    {
    size_t pos = i->find('/');
    running_jobs_names.insert(i->substr(pos+1,i->length()-pos-1));
    }

  for (i = running_jobs_names.begin(); i != running_jobs_names.end(); i++)
    {

    // go through all the running jobs, find the corresponding one
    for (size_t j = 0; j < this->get_parent_server()->running_jobs.size(); j++)
      {
      if (*i == this->get_parent_server()->running_jobs[j]->job_id)
        {
        running_jobs.insert(this->get_parent_server()->running_jobs[j]);
        break;
        }
      }
    }

  node_info *source_node = this;

  set<JobInfo*,bool(*)(JobInfo*,JobInfo*)>::const_iterator j;
  for (j = running_jobs.begin(); j != running_jobs.end(); j++)
    { // for each job (in the order of completion, unplan from node, creating a new virtual one)

    pars_spec *nodespec = parse_nodespec((*j)->sched_nodespec.c_str());
    pars_spec_node *node = nodespec->nodes;
    while (node != NULL && this->p_name != node->host)
      node = node->next;

    if (node != NULL) // found this node in nodespec
      {
      // make a copy of the original node and assign it
      boost::shared_ptr<node_info> slave = boost::make_shared<node_info>(*source_node);
      slave->p_phys_node = this;
      this->p_slave_nodes.push_back(slave);

      // unplan job from this node and set available time
      (*j)->unplan_from_node(slave.get(),node);
      slave->p_avail_after = (*j)->completion_time();
      // record jobs that need to finish for this node state to exist
      if (source_node->p_waiting_on_jobs == "")
        slave->p_waiting_on_jobs = (*j)->job_id;
      else
        slave->p_waiting_on_jobs += string(", ") + (*j)->job_id;

      source_node = slave.get();
      }

    free_parsed_nodespec(nodespec);
    }

  }
