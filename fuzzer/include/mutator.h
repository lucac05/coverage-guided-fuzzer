#ifndef MUTATOR_H
#define MUTATOR_H

#include "input.h"

/*
 * Mutates the given input to create a new modified input.
 * The specific algorithm for the mutation is specified in the
 * assignment document. Refer to that as well as `Mutation.md`.
 * 
 * @param input The input to mutate
 * @returns The newly created mutated input
 */
INPUT mutate(INPUT input);

#endif