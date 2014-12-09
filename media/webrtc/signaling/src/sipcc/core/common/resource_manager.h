/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RM_MGR_H__
#define _RM_MGR_H__

typedef struct resource_manager {
    int16_t  max_element;
    int16_t  max_index;
    uint32_t *table;
} resource_manager_t;

void               rm_clear_all_elements(resource_manager_t *rm);
void               rm_clear_element(resource_manager_t *rm, int16_t element);
void               rm_set_element(resource_manager_t *rm, int16_t element);
boolean            rm_is_element_set(resource_manager_t *rm, int16_t element);
int16_t            rm_get_free_element(resource_manager_t *rm);
void               rm_show(resource_manager_t *rm);
resource_manager_t *rm_create(int16_t max_element);
void               rm_destroy(resource_manager_t *rm);

#endif
