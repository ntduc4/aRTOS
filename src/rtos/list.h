#ifndef RTOS_LIST_H
#define RTOS_LIST_H

#include <stdint.h>

typedef struct rtos_list rtos_list_t;
typedef struct rtos_list_item rtos_list_item_t;

struct rtos_list_item {
  uint32_t value;

  struct rtos_list_item *prev;
  struct rtos_list_item *next;

  void *owner;
  rtos_list_t *container;
};

struct rtos_list {
  uint32_t count;
  rtos_list_item_t sentinel;
};

void rtos_init_list(rtos_list_t *list);
void rtos_init_list_item(rtos_list_item_t *item, void *owner);

void rtos_list_append(rtos_list_item_t *dst, rtos_list_item_t *item);
void rtos_list_prepend(rtos_list_item_t *dst, rtos_list_item_t *item);

void rtos_list_remove(rtos_list_item_t *item);

void rtos_list_insert_end(rtos_list_t *list, rtos_list_item_t *item);
void rtos_list_insert_sorted(rtos_list_t *list, rtos_list_item_t *item);

#endif // !RTOS_LIST_H
