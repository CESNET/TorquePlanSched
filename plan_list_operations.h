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

#ifndef PLAN_LIST_OPERATIONS_H
#define PLAN_LIST_OPERATIONS_H
/*
 * common and general linked list operations here
 */

plan_list* list_create(listtype type);

int list_append(plan_list* list, plan_list* list_to_append);

void* list_get_first(plan_list* list);

void* list_get_last(plan_list* list);

void* list_get_next(plan_list* list);

void* list_get_previous(plan_list* list);

void* get_successor(void* item,listtype type);

void* get_predeccessor(void* item,listtype type);

void* set_successor(void* item, void* successor, listtype type);

void* set_predeccessor(void* item, void* predeccessor, listtype type);

void* list_add_infrontof(plan_list* list, void* item_next, void* item_new);

void* list_add_inbackof(plan_list* list, void* item_previous, void* item_new);

void* list_add_begin(plan_list* list, void* item_new);

void* list_add_end(plan_list* list, void* item_new);

void* list_get_item(plan_list* list, int position);

void* list_get_random_item(plan_list* list, int count_limit);

int list_remove_item(plan_list* list, void* item_remove, unsigned set_free);

int list_remove(plan_list* list, int position, unsigned set_free);

int list_move_item(plan_list* list, void* item_move, void* predeccessor);

int list_swap_item(plan_list* list, void* item_one,void* item_two);

int list_repleace_item(plan_list* list, void* item_old, void* item_new, unsigned set_free);

int list_adjust_job_position(plan_list* list, plan_job* job);

#endif
