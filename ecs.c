#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include "array_stack.h"
#include "ecs.h"

/* Initial capacity of space for entities */
#define INITIAL_CAPACITY 32

/* The storage of components */
typedef struct {
    /* Number of types that are into each component bundle */
    uint32_t type_count;

    /* Full capacity component bundles of all components groups */
    uint32_t cap;

    /* Size in component bundles allocated */
    size_t size;

    /* Array of sizes of each component type, they should add up to `size` */
    size_t* data_size_array;

    /* Array of offsets of each component on the component bundle */
    size_t* data_offset_array;

    /* Store the components all together and tighly packed, each entity has
     * allocated space as it had all the components so there can be a lot of
     * fragmentation */
    void* data;
} component_store_t;

/* The storage for the allocated entities, with the bitmaks that identify which
 * components each entity has associated */
typedef struct {
    /* Bitmasks in order, their index is the entity id */
    uint32_t* mask_array;

    /* Marks the state of a certain entity, for example if is dead or alive */
    uint32_t* flag_array;

    /* Cuantity of allocated entities */
    uint32_t count;

    /* Cuantity of allocated space for entities */
    uint32_t cap;
} entity_store_t;

/* The state of the Entity-Component-System */
typedef struct {
    /* Storage for components tighly packed */
    component_store_t component_store;

    /* Storage of entity data */
    entity_store_t entity_store;

    /* ... */
    query_result_t query_result;

    /* Allocator for new entity ids */
    array_stack_t* entity_pool;
} state_t;

/* There is only 1 static state for the ECS */
static state_t state = {0};

void
ecs_init(uint32_t n, ...) 
{
    va_list ap;
    size_t sizes[32];
    size_t offsets[32];
    size_t size = 0;

    /* Fill the sizes and offsets for each provided type the ECS will hold */
    va_start(ap, n);
    for (uint32_t i = 0; i < n; i++) {
        sizes[i] = va_arg(ap, size_t);
        offsets[i] = size;
        size += sizes[i];
    }
    va_end(ap);

    /* Create the entity pool */
    state.entity_pool = as_create(sizeof(uint32_t)),

    /* Create the storage */
    state.component_store = (component_store_t) {
        .type_count = n,
        .cap = INITIAL_CAPACITY,
        .data = malloc(INITIAL_CAPACITY * size),
        .data_size_array = malloc(n * sizeof(size_t)),
        .data_offset_array = malloc(n * sizeof(size_t)),
        .size = size
    };
	memcpy(state.component_store.data_size_array, 
           sizes, n * sizeof(size_t));
	memcpy(state.component_store.data_offset_array, 
           offsets, n * sizeof(size_t));

    /* Create the entitiy storage */
    state.entity_store = (entity_store_t) {
        .count = 0,
        .cap = INITIAL_CAPACITY,
        .mask_array = malloc(INITIAL_CAPACITY * sizeof(uint32_t)),
        .flag_array = malloc(INITIAL_CAPACITY * sizeof(uint32_t))
    };
}

entity_t 
ecs_create() 
{
    entity_t entity;
    uint32_t id;

    /* If there is any killed entity empty space use it */
    if (state.entity_pool->count > 0) {
        id = *(uint32_t*)as_pop(state.entity_pool);

    /* Otherwise use new space, checking if we need to allocate more */
    } else {
        id = state.entity_store.count++;
        if (state.entity_store.cap == id) {
            uint32_t* new_flag_array = 
                realloc(state.entity_store.flag_array, 
                        state.entity_store.cap * 2 * sizeof(uint32_t));
            uint32_t* new_mask_array =
                realloc(state.entity_store.mask_array, 
                        state.entity_store.cap * 2 * sizeof(uint32_t));
            void* new_data = 
                realloc(state.component_store.data,
                        state.component_store.cap * 2 *
                            state.component_store.size);
            uint32_t* new_query_result_list = 
                realloc(state.query_result.list, 
                        state.entity_store.cap * 2 * sizeof(uint32_t));

            /* Check if any of the array extensions failed */
            if (!new_flag_array || !new_mask_array || !new_data) {
                printf("Error: Failed to reallocate %s:%d\n", 
                        __FILE__, __LINE__);
                exit(1);
            }

            state.entity_store.flag_array = new_flag_array;
            state.entity_store.mask_array = new_mask_array;
            state.entity_store.cap *= 2;

            state.query_result.list = new_query_result_list;

            state.component_store.data = new_data;
            state.component_store.cap *= 2;
        }
    }

    /* At first the entity mask is marked as if the entity had no associated
     * components, its just marked as alive state */
    state.entity_store.mask_array[id] = 0;
    state.entity_store.flag_array[id] = ENTITY_FLAG_ALIVE;
    entity.id = id;

    return entity;
}

void*
ecs_get(uint32_t entity_id, uint32_t component_id)
{
    /* Knowing the entity_id indicates the index on the data of component 
     * bundles and that the exact offset for a certain component in a bundle
     * can be obtained through `component_store.data_offset_array` we get
     * the address of that component.
     *
     * TODO: Add more checks */
    return (uint8_t*)state.component_store.data + 
                (entity_id * state.component_store.size + 
                 state.component_store.data_offset_array[component_id]);
}

/* Add a component with data to an entity */
void 
ecs_add(uint32_t entity_id, uint32_t component_id, void* data)
{
    /* Calculate size of the component to allocate */
    size_t size = state.component_store.data_size_array[component_id];

    /* Get a pointer to where it should belong */
    void* ptr = ecs_get(entity_id, component_id);

    /* Update the mask of the selected entity so it now says this one has
     * certain component, in case it already had one, this won't be an issue 
     * it will be the same a an update */
    state.entity_store.mask_array[entity_id] |= (1 << component_id);

    /* Copy the new component to its location */
    memcpy(ptr, data, size);
}

void 
ecs_remove(uint32_t entity_id, uint32_t component_id)
{
    /* Just mark on the entity mask that it doesn't contain that component */
    state.entity_store.mask_array[entity_id] &= ~(1 << component_id);
}

uint32_t 
ecs_has(uint32_t entity_id, uint32_t component_id)
{
    return 
        0 != (state.entity_store.mask_array[entity_id] & (1 << component_id));
}

/* Kill an entity */
void ecs_kill(uint32_t entity_id)
{
    /* If is alive kill it by marking it as dead (~ENTITY_FLAG_ALIVE),
     * clearing its mask and pushing it to the entity pool */
    if ((state.entity_store.flag_array[entity_id] & ENTITY_FLAG_ALIVE) != 0) {
        state.entity_store.flag_array[entity_id] &= ~ENTITY_FLAG_ALIVE;
        state.entity_store.mask_array[entity_id] = 0;
        as_push(state.entity_pool, &entity_id);
    }
}

query_result_t*
ecs_query(uint32_t n, ...)
{
    va_list ap;
    uint32_t i, mask = 0;

    state.query_result.count = 0;

    /* Create a mask for the components we want */
    va_start(ap, n);
    for (i = 0; i < n; i++) {
        mask |= (1 << va_arg(ap, uint32_t));
    }
    va_end(ap);

    /* Compare the query mask with the mask of the entity and if their 
     * components matches add it to the query result */
    for (i = 0; i < state.entity_store.count; i++) {
        if (state.entity_store.flag_array[i] & ENTITY_FLAG_ALIVE && 
                mask == (state.entity_store.mask_array[i] & mask)) {
            state.query_result.list[state.query_result.count++] = i;
        }
    }

    return &state.query_result;
}
