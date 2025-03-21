/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __eCV5546_GROUP_H
#define __eCV5546_GROUP_H

#include <dm/device.h>
void group_groups(struct udevice *dev);
const char *eys3d_get_group_name_by_selector(unsigned int selector);
int eys3d_get_groups_count(void);

struct grp2fp_map_t *eys3d_get_group_by_selector(unsigned int group_selector,
						   unsigned int func_selector);

const char *eys3d_get_function_name_by_selector(unsigned int selector);
struct func_t *eys3d_get_function_by_selector(unsigned int selector);
int eys3d_get_function_count(void);

int eys3d_pin_function_association_query(unsigned int pin_selector,
					   unsigned int func_selector);
int eys3d_group_function_association_query(unsigned int group_selector,
					     unsigned int func_selector);

#endif /* __eCV5546_GROUP_H */
