#include "list.h"
#include <stddef.h>

void rtos_init_list(rtos_list_t *list) {
  if (list == NULL)
    return;

  list->count = 0;

  list->sentinel.value = 0;
  list->sentinel.next = &list->sentinel;
  list->sentinel.prev = &list->sentinel;
  list->sentinel.owner = NULL;
  list->sentinel.container = list;
}

void rtos_init_list_item(rtos_list_item_t *item, void *owner) {
  if (item == NULL || owner == NULL)
    return;

  item->container = NULL;
  item->value = 0;
  item->next = NULL;
  item->prev = NULL;
  item->owner = owner;
}

void rtos_list_append(rtos_list_item_t *dst, rtos_list_item_t *item) {
  if (dst == NULL || item == NULL || dst->container == NULL ||
      item->container != NULL)
    return;
  rtos_list_item_t *next = dst->next;

  dst->next = item;
  item->prev = dst;

  item->next = next;
  next->prev = item;

  item->container = dst->container;
  dst->container->count++;
}

void rtos_list_prepend(rtos_list_item_t *dst, rtos_list_item_t *item) {
  if (dst == NULL || item == NULL || dst->container == NULL ||
      item->container != NULL)
    return;
  rtos_list_item_t *prev = dst->prev;

  dst->prev = item;
  item->next = dst;

  item->prev = prev;
  prev->next = item;

  item->container = dst->container;
  dst->container->count++;
}

void rtos_list_remove(rtos_list_item_t *item) {
  if (item == NULL || item->container == NULL || item->next == NULL ||
      item->prev == NULL || &item->container->sentinel == item)
    return;

  rtos_list_item_t *prev = item->prev;
  rtos_list_item_t *next = item->next;
  prev->next = next;
  next->prev = prev;
  item->container->count--;

  item->container = NULL;
  item->next = NULL;
  item->prev = NULL;
}

void rtos_list_insert_end(rtos_list_t *list, rtos_list_item_t *item) {
  if (list == NULL)
    return;
  rtos_list_prepend(&list->sentinel, item);
}

void rtos_list_insert_sorted(rtos_list_t *list, rtos_list_item_t *item) {
  if (list == NULL || item == NULL || list->sentinel.next == NULL ||
      list->sentinel.prev == NULL || item->container != NULL)
    return;

  rtos_list_item_t *current = list->sentinel.next;
  while (current != &list->sentinel && current->value <= item->value) {
    current = current->next;
  }

  rtos_list_prepend(current, item);
}

void rtos_list_insert_reversed_sorted(rtos_list_t *list,
                                      rtos_list_item_t *item) {
  if (list == NULL || item == NULL || list->sentinel.next == NULL ||
      list->sentinel.prev == NULL || item->container != NULL)
    return;

  rtos_list_item_t *current = list->sentinel.next;
  while (current != &list->sentinel && current->value >= item->value) {
    current = current->next;
  }

  rtos_list_prepend(current, item);
}
