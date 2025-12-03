#ifndef 	__STACK_H
#define 	__STACK_H

#define STACK_REALLOC_GROW_FACTOR 2

#ifndef STACK_STARTING_CAPACITY
#	define STACK_STARTING_CAPACITY 32
#endif

typedef struct {
	void* data;
	size_t object_size;
	size_t count;
	size_t capacity;
} Stack;

void Stack_resize(Stack* s);

#define Stack_new(P, T) \
	Stack_init((P),sizeof(T))

void Stack_init(Stack* s,size_t type_sz) {
	Stack init =  {
		.data = NULL,
		.object_size = type_sz,
		.capacity = STACK_STARTING_CAPACITY,
		.count = 0,
	};
	Stack_resize(&init);
	memset(init.data, 0, type_sz*STACK_STARTING_CAPACITY);
	*s = init;
}

void Stack_resize(Stack* s) {
	const size_t sz 		= s->object_size;
	const size_t new_size 	= s->capacity * STACK_REALLOC_GROW_FACTOR;
	void* realloced_stack = realloc(s->data, sz * new_size);
	if (!realloced_stack) assert(0 && "Failed to realloc");
	s->capacity = new_size;
	s->data = realloced_stack;
}

void __stack_place(Stack* s, void* data, size_t data_size) {
	assert(data_size && "Cannot push item of size 0");
	assert(s->count < s->capacity);
	void* location = (char*)s->data + (s->count*s->object_size);
	memcpy(location,data,data_size);
}

#define Stack_push(S,ITEM) __stack_push((S),&(ITEM),sizeof(ITEM))
void __stack_push(Stack* s, void* item, size_t item_sz) {
	if (s->count > s->capacity - 1) 
		Stack_resize(s);
	__stack_place(s,item,item_sz);
	s->count++;
}

void* Stack_get(Stack* s, size_t index) {
	return ((char*)s->data + (s->object_size*index));
}


void* Stack_alloc(Stack* s) {
	if (s->count > s->capacity - 1) 
		Stack_resize(s);
	return Stack_get(s,s->count++);
}


void Stack_clear(Stack* s) {
	const size_t sz = s->count*s->object_size;
	memset(s->data,0,sz);
}

void Stack_free(Stack* s) {
	assert(s && "Expected valid stack pointer");
	assert(s->data && "Stack was already freed");
	// maybe.. // Stack_clear(s);
	free(s->data);

	// analogus to memset(s,0,sizeof(Stack));
	*s = (Stack) {0};
}

#endif // 	__STACK_H 
