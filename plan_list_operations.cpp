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
#include "misc.h"

#include "plan_list_operations.h"
#include "plan_jobs.h"
#include "plan_gaps.h"
#include "plan_free_data.h"


plan_list* list_create(listtype type)
  {
  plan_list* list_new;
  if ((list_new = (plan_list*) malloc(sizeof(plan_list))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  list_new -> num_items = 0;

  list_new -> first = NULL;

  list_new -> last = NULL;

  list_new -> current = NULL;

  list_new -> type = type;

  return list_new;
  }

int list_append(plan_list* list, plan_list* list_to_append)
  {
  if (list == NULL || list_to_append == NULL || list->type != list_to_append->type)
    return 1;

  if (list -> num_items > 0 && list_to_append -> num_items > 0)
    {
    set_successor(list->last, list_to_append -> first, list -> type);
    set_predeccessor(list_to_append -> first, list -> last, list -> type);

    list -> last = list_to_append -> last;
    list -> num_items += list_to_append -> num_items;
    }

  if (list -> num_items == 0)
    {
	list -> first = list_to_append -> first;
	list -> last = list_to_append -> last;
	list -> num_items += list_to_append -> num_items;
    }

  free(list_to_append);
  return 0;
  }

void* list_get_first(plan_list* list)
  {
  if (list == NULL)
    return NULL;

  list -> current = list -> first;
  return list -> current;
  }

void* list_get_last(plan_list* list)
  {
  if (list == NULL)
    return NULL;

  list -> current = list -> last;
  return list -> current;
  }

void* list_get_next(plan_list* list)
  {
  if (list == NULL)
    return NULL;

  if (list -> current == NULL)
	return list -> current = list -> first;

  list -> current = get_successor(list -> current, list -> type);
  return list -> current;
  }

void* list_get_previous(plan_list* list)
{
  if (list == NULL)
    return NULL;

  if (list -> current == NULL)
	return list -> current = list -> last;

  list -> current = get_predeccessor(list -> current, list -> type);
  return list -> current;
  }

void* get_successor(void* item,listtype type)
  {
  if (item == NULL)
    return NULL;

  void* suc;
  /*
  return ((plan_item*) item)->successor;
  */

  if (type == Gaps)
	return (void*)(((plan_gap*) item) -> successor);

  if (type == Jobs)
    return (void*)(((plan_job*) item) -> successor);

  if (type == Limits)
    return (void*)(((plan_limit*) item) -> successor);

  return NULL;
  }

void* get_predeccessor(void* item,listtype type)
  {
  if (item == NULL)
    return NULL;
/*
  return ((plan_item*) item)->predeccessor;
  */
  if (type == Gaps)
    return (void*)(((plan_gap*) item) -> predeccessor);
  if (type == Jobs)
    return (void*)(((plan_job*) item) -> predeccessor);
  if (type == Limits)
    return (void*)(((plan_limit*) item) -> predeccessor);
  return NULL;
  }

void* set_successor(void* item, void* successor, listtype type)
  {
  if (item == NULL)
    return NULL;
  /*
  ((plan_item*) item)->successor = successor;
  */
  if (type == Gaps)
	((plan_gap*) item) -> successor = (struct plan_gap*) successor;

  if (type == Jobs)
    ((plan_job*) item) -> successor = (struct plan_job*) successor;

  if (type == Limits)
    ((plan_limit*) item) -> successor = (struct plan_limit*) successor;
  return successor;

  }

void* set_predeccessor(void* item, void* predeccessor, listtype type)
  {
  if (item == NULL)
    return NULL;
  /*
  ((plan_item*) item)->predeccessor = predeccessor;
  */
  if (type == Gaps)
    ((plan_gap*) item) -> predeccessor = (struct plan_gap*) predeccessor;
  if (type == Jobs)
    ((plan_job*) item) -> predeccessor = (struct plan_job*) predeccessor;
  if (type == Limits)
    ((plan_limit*) item) -> predeccessor = (struct plan_limit*) predeccessor;
  return predeccessor;
  }

void* list_add_infrontof(plan_list* list, void* item_next, void* item_new)
  {
  void* item_next_predeccessor;

  if (list == NULL)
	return NULL;

  if (item_next == NULL)
	return list_add_begin(list, (void*)item_new);

  if (item_new == NULL)
    {
	if (list -> type == Jobs)
	  item_new = (void*)job_create();
	if (list -> type == Gaps)
	  item_new = (void*)gap_create();
    }

  item_next_predeccessor = get_predeccessor(item_next, list -> type);
  if (item_next_predeccessor != NULL)
    {
	set_successor(item_next_predeccessor, item_new, list -> type);
	}
    else
    {
    list -> first = item_new;
    }

  set_successor(item_new, item_next, list -> type);
  set_predeccessor(item_new, item_next_predeccessor, list -> type);
  set_predeccessor(item_next, item_new, list->type);

  list -> num_items++;
  list -> current = item_new;
  return item_new;
}

void* list_add_inbackof(plan_list* list, void* item_previous, void* item_new)
  {
  void* item_previous_successor;

  if (list == NULL)
	return NULL;

  if (item_previous == NULL)
    return list_add_end(list, (void*)item_new);

  if (item_new == NULL)
    {
	if (list -> type == Jobs)
	  item_new = (void*)job_create();

	if (list -> type == Gaps)
	  item_new = (void*)gap_create();
    }

  item_previous_successor = get_successor(item_previous, list -> type);
  if (item_previous_successor != NULL)
    {
	set_predeccessor(item_previous_successor, item_new, list -> type);
	}
    else
    {
    list -> last = item_new;
    }

  set_successor(item_new, item_previous_successor, list -> type);
  set_predeccessor(item_new, item_previous, list -> type);
  set_successor(item_previous, item_new, list -> type);

  list -> num_items++;
  list -> current = item_new;

  return item_new;
  }

void* list_add_begin(plan_list* list, void* item_new)
  {
  if (list == NULL)
    return NULL;

  if (item_new == NULL)
    {
	if (list -> type == Jobs)
	  item_new = (void*)job_create();

	if (list -> type == Gaps)
	  item_new = (void*)gap_create();
    }

  if (list -> num_items == 0)
    {
	list -> num_items++;
	list -> first = item_new;
	list -> last = item_new;
	list -> current = item_new;
	return item_new;
    }

  return list_add_infrontof(list, list -> first, item_new);
  }

void* list_add_end(plan_list* list, void* item_new)
  {
  if (list == NULL)
    return NULL;

  if (item_new == NULL)
	{
	if (list -> type == Jobs)
	  item_new = (void*)job_create();

	if (list -> type == Gaps)
	  item_new = (void*)gap_create();
	}

  if (list -> num_items == 0)
    {
	list -> num_items++;
	list -> first = item_new;
	list -> last = item_new;
	list -> current = item_new;
	return item_new;
    }
  return list_add_inbackof(list, list -> last, item_new);
  }

void* list_get_item(plan_list* list, int position)
  {
  if (list == NULL || list -> num_items <= position || position < 0)
	  return NULL;

  if (list -> num_items / 2 < position )
    {
	list -> current = list -> last;
	for (int i = 0; i < list -> num_items - position -1; i++)
	  list -> current = get_predeccessor(list -> current,list -> type);
    }
    else
    {
    list -> current = list -> first;
    for (int i = 0; i < position; i++)
      list -> current = get_successor(list -> current, list -> type);
    }
  return list -> current;
  }

void* list_get_random_item(plan_list* list, int count_limit)
  {
  int position;

  if (list -> num_items < count_limit)
    return NULL;

  position = (rand() % list -> num_items);

  return list_get_item(list, position);
  }

int list_remove_item(plan_list* list, void* item_remove, unsigned set_free)
  {
  void* item_remove_predeccessor;
  void* item_remove_successor;

  if (item_remove == NULL || list == NULL)
    return 1;

  item_remove_predeccessor = get_predeccessor(item_remove, list -> type);
  item_remove_successor = get_successor(item_remove, list -> type);

  if (item_remove_predeccessor != NULL)
    {
	set_successor(item_remove_predeccessor, item_remove_successor, list -> type);
    }
  else
    {
	set_predeccessor(item_remove_successor, NULL, list -> type);
    list -> first = item_remove_successor;
    }

  if (item_remove_successor != NULL)
    {
	set_predeccessor(item_remove_successor, item_remove_predeccessor, list -> type);
    }
  else
    {
	set_successor(item_remove_predeccessor, NULL, list -> type);
    list -> last=item_remove_predeccessor;
    }

  list -> num_items--;
  list -> current = NULL;
  if (set_free)
    {
	if (list -> type == Jobs) free_job((plan_job*)item_remove);
	if (list -> type == Gaps) free_gap((plan_gap*)item_remove);
	if (list -> type == Limits) free_limit((plan_limit*)item_remove);
    }
    else
    {
    set_successor(item_remove, NULL, list -> type);
    set_predeccessor(item_remove, NULL, list -> type);
    }

  return 0;
  }

int list_remove(plan_list* list, int position, unsigned set_free)
  {
  return list_remove_item(list, list_get_item(list, position), set_free);
  }

int list_move_item(plan_list* list, void* item_move, void* predeccessor)
  {
  if (item_move == NULL)
    return 1;

  list_remove_item(list, item_move, 0);
  if (predeccessor != NULL)
    list_add_inbackof(list, predeccessor, item_move);

  if (predeccessor == NULL)
    list_add_infrontof(list, list_get_first(list), item_move);
  return 0;
  }

int list_swap_item(plan_list* list, void* item_one,void* item_two)
  {
  void* predeccessor;

  if (item_one == item_two || item_one == NULL || item_two == NULL)
    return 1;

  predeccessor = get_predeccessor(item_one, list -> type);
  if ( predeccessor != NULL)
	  return list_move_item(list, item_one, item_two) && list_move_item(list, item_two, predeccessor);

  predeccessor = get_predeccessor(item_two, list -> type);
  if ( predeccessor != NULL)
  	  return list_move_item(list, item_two, item_one) && list_move_item(list, item_one, predeccessor);

  return 0;
  }

int list_repleace_item(plan_list* list, void* item_old, void* item_new, unsigned set_free)
  {
  if (item_old == item_new || item_old == NULL || item_new == NULL )
 	return 1;

  if (get_successor(item_new, list -> type) != NULL || get_predeccessor(item_new, list -> type) != NULL )
    return 1;

  set_successor(item_new, get_successor(item_old, list -> type), list -> type);
  set_predeccessor(item_new, get_predeccessor(item_old, list -> type), list->type);

  if (get_successor(item_new, list -> type) != NULL){
    set_predeccessor(get_successor(item_new, list -> type), item_new, list -> type);
    }else
    {
	list -> last = item_new;
    }

  if (get_predeccessor(item_new, list -> type) != NULL)
    {
	set_successor(get_predeccessor(item_new, list -> type), item_new, list -> type);
    }else
    {
	list -> first = item_new;
    }
  if (set_free)
    {
    free(item_old);
    }
    else
    {
    set_successor(item_old, NULL, list -> type);
    set_predeccessor(item_old, NULL, list -> type);
    }

  return 0;
  }

//tohle jeste zkontroluj:-)
int list_adjust_job_position(plan_list* list, plan_job* job)
  {
  plan_job* new_predeccessor;
  plan_job* new_successor;
  new_predeccessor = job -> predeccessor;
  new_successor = job -> successor;

  //posun dopredu
  while (new_predeccessor != NULL &&
		  (new_predeccessor -> start_time > job -> start_time ||
		    (new_predeccessor -> start_time == job -> start_time && new_predeccessor -> job_id > job -> job_id)
		  )
		)
    new_predeccessor = new_predeccessor -> predeccessor;


  //posun dozadu
  while (new_successor != NULL &&
		  (new_successor -> start_time < job ->  start_time ||
		    (new_successor -> start_time == job -> start_time && new_successor -> job_id < job -> job_id)
		  )
		)
    new_successor = new_successor -> successor;

  if (new_successor != job -> successor)
    if (new_successor == NULL)
    {
	new_predeccessor = (plan_job*)list -> last;
    } else
    {
    new_predeccessor = new_successor -> predeccessor;
    }

  if (new_predeccessor != job -> predeccessor)
    list_move_item(list, (void*) job, (void*) new_predeccessor);

  return 0;
  }
