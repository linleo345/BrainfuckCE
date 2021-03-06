#ifndef STACK_H_
#define STACK_H_

#include <stdint.h>

#ifndef _WIN32
#define __TICE__
#endif

#ifdef __TICE__
#define MAX_STACK_SIZE 100
#else
#define MAX_STACK_SIZE 2048
#endif

#define STACK_TYPE unsigned int

typedef struct Stack {
    STACK_TYPE stack[MAX_STACK_SIZE];
    unsigned int top;
} Stack_t;

void stack_Create(Stack_t *stack);
void stack_Push(Stack_t *stack, STACK_TYPE value);
STACK_TYPE stack_Pop(Stack_t *stack);
STACK_TYPE stack_Peek(Stack_t *stack);

#endif