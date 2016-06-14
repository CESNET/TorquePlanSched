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
#include <time.h>
#include "misc.h"

#include "plan_list_operations.h"
#include "plan_fairshare.h"

double job_response_time(plan_job* job, bool real)
  {
  int completion_time;

  if (real)
	completion_time = job -> jinfo -> compl_time;
  else
	completion_time = job -> completion_time;

  return completion_time - job -> jinfo -> qtime;
  }

double job_wait_time(plan_job* job)
  {
  return job -> start_time - job -> jinfo -> qtime;
  }

double job_slowdown_time(plan_job* job, bool real)
  {
  int completion_time;

  if (real)
    completion_time = job -> jinfo -> compl_time;
  else
	completion_time = job -> completion_time;

  if (completion_time - job -> start_time < 1)
	return (completion_time - job -> jinfo -> qtime) / 1;

  return ((completion_time - job -> jinfo -> qtime) * 1.0) / ((completion_time - job -> start_time) * 1.0);
  }


plan_eval* evaluation_reset(plan_eval* evaluation)
  {
  evaluation -> start_time = 0;

  evaluation -> end_time = 0;

  evaluation -> num_jobs = 0;

  evaluation -> response_time = 0;

  evaluation -> wait_time = 0;

  evaluation -> slowdown = 0;

  evaluation -> sum_response_time = 0;

  evaluation -> sum_wait_time = 0;

  evaluation -> sum_slowdown = 0;

  evaluation -> fairness = 0;

  return evaluation;
  }

plan_eval* evaluation_calc_criterion(plan_eval* evaluation)
  {
  if (evaluation -> num_jobs > 0)
    {
    evaluation -> response_time = evaluation -> sum_response_time / evaluation -> num_jobs;

    evaluation -> slowdown = evaluation -> sum_slowdown / evaluation -> num_jobs;

    evaluation -> wait_time = evaluation -> sum_wait_time / evaluation -> num_jobs;
    } else  {
    evaluation -> start_time = 0;

    evaluation -> end_time = 0;

    return NULL;
    }
  return evaluation;
  }


plan_eval* evaluation_add_job(plan_eval* evaluation, plan_job* job, bool jobfinished)
  {
  int completion_time;

  double response_time;

  double slowdown;

  double wait_time;

  if (jobfinished)
    completion_time = job -> jinfo -> compl_time;
  else
    completion_time = job -> completion_time;

  if (evaluation -> start_time == 0 || evaluation -> start_time > job -> start_time)
    evaluation -> start_time = job -> start_time;

  if (evaluation -> end_time <  completion_time)
    evaluation -> end_time = completion_time;

  response_time = job_response_time(job, jobfinished);

  slowdown = job_slowdown_time(job, jobfinished);

  wait_time = job_wait_time(job);

  if (jobfinished)
    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "job finished", "RT: %f, WT: %f, SD: %f ",
		  response_time,
		  wait_time,
		  slowdown);

  evaluation -> sum_response_time += response_time;

  evaluation -> sum_slowdown += slowdown;

  evaluation -> sum_wait_time += wait_time;

  evaluation -> num_jobs++;

  return evaluation;
  }

plan_eval evaluate_schedule(sched* schedule)
  {
  plan_eval res;
  int cl_number;
  plan_job* job;
  plan_gap* last_gap;

  evaluation_reset(&res);

  for (cl_number = 0; cl_number < schedule -> num_clusters; cl_number++)
    {
    last_gap = (plan_gap*) schedule -> gaps[cl_number] -> last;

    res.end_time = last_gap -> start_time;

    schedule -> jobs[cl_number] -> current = NULL;
    while (list_get_next(schedule -> jobs[cl_number]) != NULL)
      {
      job = (plan_job*) schedule -> jobs[cl_number] -> current;

      if (job -> jinfo -> state == JobQueued)
        evaluation_add_job(&res, job, false);
      }
    }

  fairshare_get_planned_from_schedule(schedule);

  res.fairness=fairshare_result_max();

  evaluation_calc_criterion(&res);

  return res;
  }

void evaluation_log(plan_eval* evaluation, const char* evaluation_description)
  {
  char buffer_start[30];
  char buffer_end[30];

  if (evaluation -> num_jobs > 0)
    {
    strftime(buffer_start, 30, "%Y:%m:%d %H:%M:%S", localtime(&evaluation -> start_time));
    strftime(buffer_end, 30, "%Y:%m:%d %H:%M:%S", localtime(&evaluation -> end_time));

    sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_REQUEST, "evaluation", "%s, number of jobs: %d, RT: %f, WT: %f, SD: %f, Fair: %f, start time: %s, end time: %s, makespan: %i",
                evaluation_description,
                evaluation -> num_jobs,
                evaluation -> response_time,
                evaluation -> wait_time,
                evaluation -> slowdown,
                evaluation -> fairness,
                buffer_start,
                buffer_end,
                evaluation->end_time - evaluation->start_time);
    }
  }
