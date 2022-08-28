#ifndef ECS_H
#define ECS_H

#include <inttypes.h>
#include <stddef.h>

#define ENTITY_FLAG_ALIVE 1

/* Identifier for each entity, unique */
typedef struct {
    uint32_t id;
} entity_t;

typedef struct {
    /* Number of components */
    uint32_t count;

    /* The query must have enough capacity to hold a query of all avaible 
     * components */
    uint32_t cap;

    /* Array of entities that match the data of the query */
    uint32_t *list;
} query_result_t;

/* Initialize the ECS module with the component size.
 *
 * Must pass in the size of each component type in the order you want to store
 * them. The maximum number of component types is 32, though this could be
 * extended by adding another bitmask and a bit switch */
void
ecs_init(uint32_t component_count, ...);

/* Create an entity. Returns a handle which contains the id */
entity_t 
ecs_create();

/* Returns a pointer to the place where the component data for a certain type
 * of component for a certain entity should belong */
void*
ecs_get(uint32_t entity_id, uint32_t component_id);

/* Add a component with data to an entity */
void 
ecs_add(uint32_t entity_id, uint32_t component_id, void* data);

/* Remove a component from an entity */
void 
ecs_remove(uint32_t entity_id, uint32_t component_id);

/* Returns true if entity has component */
uint32_t 
ecs_has(uint32_t entity_id, uint32_t component_id);

/* Kill an entity */
void ecs_kill(uint32_t entity_id);

/* Query all components from the entities that match with the component ids
 * of the provided ones */
query_result_t*
ecs_query(uint32_t component_count, ...);

#endif
