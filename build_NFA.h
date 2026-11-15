typedef enum {
    OPERAND_SET = 0, OPERAND_NFA = 1, STATE_EPSILON1 = 2, STATE_EPSILON2 = 3, STATE_CONSUME = 4, STATE_ACCEPT = 5
} OPERAND_TYPE;

typedef struct {
    U64 upper;
    U64 lower;
} CHARACTER_SET;

typedef struct NFA_STATE {
    CHARACTER_SET set;
	U64 id;
    struct NFA_STATE* next1;
    struct NFA_STATE* next2;
} NFA_STATE;

typedef struct {
    NFA_STATE* start;
    NFA_STATE* end;
	U64 num_states;
} NFA;

typedef struct {
    U8 tag;
    union {
        NFA nfa;
        CHARACTER_SET set;
    };
} STACK_OPERAND;

void nfa_state_init(NFA_STATE* state, U8 type, CHARACTER_SET set, NFA_STATE* next1, NFA_STATE* next2){
	state->id = 0ull;
    if(type == STATE_EPSILON1){
        state->next1 = next1; 
        state->next2 = nullptr; 
        return;
    } 
    
    if (type == STATE_EPSILON2){
        state->next1 = next1; 
        state->next2 = next2; 
        return;
    }
    
    if (type == STATE_CONSUME){
		state->set = set;
        state->next1 = next1; 
        state->next2 = next1; 
        return;
    }

    if (type == STATE_ACCEPT){
        state->next1 = nullptr; 
        state->next2 = nullptr; 
        return;
    }
}



STACK_OPERAND stack_operand_from_token(U16 token){

    static U64 predef_sets[] = {
		0x03ff000000000000ull, 0x0000000000000000ull, //digit
		0x0000000000000000ull, 0x07fffffe00000000ull, //lower
		0x0000000000000000ull, 0x0000000007fffffeull, //upper
		0x03ff000000000000ull, 0x07fffffe87fffffeull, //word
		0xffffffffffffffffull, 0xffffffffffffffffull, //all
		0x0                  , 0x0                   //none 
    };
    
   /* 
	static unsigned long long charsetLookup[24] = {
		0x0000'0000'0000'0000, 0x07ff'fffe'07ff'fffe, //alpha
		0x0000'0000'0000'0000, 0x0000'0000'07ff'fffe, //upper
		0x0000'0000'0000'0000, 0x07ff'fffe'0000'0000, //lower
	 	0x0000'0001'0000'3e00, 0x0                  , //whitespace
		0x03ff'0000'0000'0000, 0x0000'0000'0000'0000, //digit
		0x03ff'0000'0000'0000, 0x07e0'0000'07e0'0000, //hexdigit
		0x03ff'0000'0000'0000, 0x07ff'fffe'87ff'fffe, //word
	   0xfc00'fffe'0000'0000, 0x7800'0001'f800'0001, //punct
		0xffff'ffff'ffff'ffff, 0xffff'ffff'ffff'ffff, //all
		0x0                  , 0x0                  , //none
		0x2402               , 0x0                  , //Start line
		0x2401               , 0x0                    //EndLine
	};
    */
    CHARACTER_SET set = {.upper = 0, .lower = 0};

    if(token > 1023){
        U8 character = token - 1024; 
        if(character > 63){
            set.upper = 1ull << (character - 64);
            set.lower = 0ull;
        } else {
            set.upper = 0ull;
            set.lower = 1ull << character;
        }
		
    }
	else if(token > 767){
        U8 predef_set_index = token - 768;
        set.upper = predef_sets[2ull * predef_set_index + 1];
        set.lower = predef_sets[2ull * predef_set_index + 0]; 
    }

    STACK_OPERAND op = {
        .set = set,
        .tag = OPERAND_SET
    };

    return op;
}


STACK_OPERAND stack_operand_nfa_from_set(MEM_ARENA* arena, STACK_OPERAND a){
    NFA nfa;
    nfa.start = mem_arena_push(arena, sizeof(NFA_STATE));
    nfa.end = mem_arena_push(arena, sizeof(NFA_STATE));
	nfa.num_states = 2;

    nfa_state_init(nfa.start, STATE_CONSUME, a.set, nfa.end, nullptr);
    nfa_state_init(nfa.end, STATE_ACCEPT, (CHARACTER_SET){0ull, 0ull}, nullptr, nullptr);

    STACK_OPERAND op = {
        .tag = OPERAND_NFA,
        .nfa = nfa 
    };
    return op;
}

STACK_OPERAND stack_operand_concatenate(MEM_ARENA* arena, STACK_OPERAND a, STACK_OPERAND b){
    if(a.tag == OPERAND_SET){
        a = stack_operand_nfa_from_set(arena, a); 
    } 
    
    if(b.tag == OPERAND_SET){
        b = stack_operand_nfa_from_set(arena, b);
    }


    nfa_state_init(a.nfa.end, STATE_EPSILON1, (CHARACTER_SET){0ull, 0ull}, b.nfa.start, nullptr);
    STACK_OPERAND op;

    op.nfa.start = a.nfa.start;
    op.nfa.end = b.nfa.end;
	op.nfa.num_states = a.nfa.num_states + b.nfa.num_states;
    op.tag = OPERAND_NFA;

    return op;

}



STACK_OPERAND stack_operand_branch(MEM_ARENA* arena, STACK_OPERAND a, STACK_OPERAND b){
    if(a.tag == OPERAND_SET){
        a = stack_operand_nfa_from_set(arena, a); 
    } 
    
    if(b.tag == OPERAND_SET){
        b = stack_operand_nfa_from_set(arena, b);
    }

    STACK_OPERAND op;


    op.tag = OPERAND_NFA;
    op.nfa.start = mem_arena_push(arena, sizeof(NFA_STATE));

	op.nfa.num_states = a.nfa.num_states + b.nfa.num_states + 2;
    nfa_state_init(op.nfa.start, STATE_EPSILON2, (CHARACTER_SET){0ull, 0ull}, a.nfa.start, b.nfa.start);



    op.nfa.end = mem_arena_push(arena, sizeof(NFA_STATE));
    nfa_state_init(op.nfa.end, STATE_ACCEPT, (CHARACTER_SET){0ull, 0ull}, nullptr, nullptr);

    

    nfa_state_init(a.nfa.end, STATE_EPSILON1, (CHARACTER_SET){0ull, 0ull}, op.nfa.end, nullptr);
    nfa_state_init(b.nfa.end, STATE_EPSILON1, (CHARACTER_SET){0ull, 0ull}, op.nfa.end, nullptr);

   return op; 
}


STACK_OPERAND stack_operand_kleene_star(MEM_ARENA* arena, STACK_OPERAND a){
    if(a.tag == OPERAND_SET){
        a = stack_operand_nfa_from_set(arena, a); 
    } 

    STACK_OPERAND op;


    op.tag = OPERAND_NFA;
    op.nfa.start = mem_arena_push(arena, sizeof(NFA_STATE));
    op.nfa.end = mem_arena_push(arena, sizeof(NFA_STATE));
	op.nfa.num_states = a.nfa.num_states + 2;


    nfa_state_init(op.nfa.start, STATE_EPSILON2, (CHARACTER_SET){0ull,0ull}, a.nfa.start, op.nfa.end);

    nfa_state_init(op.nfa.end, STATE_ACCEPT, (CHARACTER_SET){0ull,0ull}, nullptr, nullptr);
    
    nfa_state_init(a.nfa.end, STATE_EPSILON2, (CHARACTER_SET){0ull,0ull}, a.nfa.start, op.nfa.end);

    return op;
}

STACK_OPERAND stack_operand_closure(MEM_ARENA* arena, STACK_OPERAND a){
    if(a.tag == OPERAND_SET){
        a = stack_operand_nfa_from_set(arena, a); 
    } 

    STACK_OPERAND op;


    op.tag = OPERAND_NFA;
    op.nfa.start = a.nfa.start;
	op.nfa.num_states = a.nfa.num_states + 1;

    op.nfa.end = mem_arena_push(arena, sizeof(NFA_STATE));
    nfa_state_init(op.nfa.end, STATE_ACCEPT, (CHARACTER_SET){0ull, 0ull}, nullptr, nullptr);

    nfa_state_init(a.nfa.end, STATE_EPSILON2, (CHARACTER_SET){0ull, 0ull}, a.nfa.start, op.nfa.end);

    return op;
}



STACK_OPERAND stack_operand_optional(MEM_ARENA* arena, STACK_OPERAND a){
    if(a.tag == OPERAND_SET){
        a = stack_operand_nfa_from_set(arena, a); 
    } 

    STACK_OPERAND op;


    op.tag = OPERAND_NFA;
    op.nfa.start = mem_arena_push(arena, sizeof(NFA_STATE));
    op.nfa.end = a.nfa.end;
	op.nfa.num_states = a.nfa.num_states + 1;


    nfa_state_init(op.nfa.start, STATE_EPSILON2, (CHARACTER_SET){0ull, 0ull}, a.nfa.start, op.nfa.end);
   
    return op;
}



STACK_OPERAND stack_operand_complement(STACK_OPERAND a){
    if(a.tag == OPERAND_NFA){
        fprintf(stdout, "CHARSET ERROR");
        exit(1);
    }
    
    CHARACTER_SET set = a.set;

    set.upper = ~set.upper;
    set.lower = ~set.lower;


    STACK_OPERAND op = {
        .tag = OPERAND_SET,
        .set = set
    };

    return op;
}

STACK_OPERAND stack_operand_intersection(STACK_OPERAND a, STACK_OPERAND b){

    if(a.tag == OPERAND_NFA){
        fprintf(stdout, "CHARSET ERROR");
        exit(1);
    }
    if(b.tag == OPERAND_NFA){
        fprintf(stdout, "CHARSET ERROR");
        exit(1);
    }
    
    CHARACTER_SET set;

    set.upper = a.set.upper & b.set.upper;
    set.lower = a.set.lower & b.set.lower;


    STACK_OPERAND op = {
        .tag = OPERAND_SET,
        .set = set
    };

    return op;
}

STACK_OPERAND stack_operand_union(STACK_OPERAND a, STACK_OPERAND b){

    if(a.tag == OPERAND_NFA){
        fprintf(stdout, "CHARSET ERROR");
        exit(1);
    }
    if(b.tag == OPERAND_NFA){
        fprintf(stdout, "CHARSET ERROR");
        exit(1);
    }
    
    CHARACTER_SET set;

    set.upper = a.set.upper | b.set.upper;
    set.lower = a.set.lower | b.set.lower;


    STACK_OPERAND op = {
        .tag = OPERAND_SET,
        .set = set
    };

    return op;
}

typedef struct {
	U64 count;
	U64 capacity;
	STACK_OPERAND operands[];
} STACK;

STACK* stack_create(MEM_ARENA* arena, U64 init_cap){
	STACK* stack = mem_arena_push(arena, sizeof(STACK) + init_cap* sizeof(STACK_OPERAND));
	stack->count = 0;
	stack->capacity = init_cap;
	return stack;
}

STACK_OPERAND stack_pop(STACK* stack){
	stack->count--;
	return stack->operands[stack->count];
}
void stack_push(MEM_ARENA* arena, STACK* stack, STACK_OPERAND op){

	if(stack->count == stack->capacity){
		U64 stack_footprint = sizeof(STACK) + stack->capacity*sizeof(STACK_OPERAND);
		U64 new_size = 2*stack->capacity * sizeof(STACK_OPERAND) + sizeof(STACK);
		if(!mem_arena_grow(arena, stack, stack_footprint, new_size )){
			fprintf(stdout, "stack could not be grown");		
			exit(1);
		} 
		stack->capacity *= 2;
	}

	stack->operands[stack->count] = op;
	stack->count++;
}


NFA build_NFA(MEM_ARENA* stack_arena, MEM_ARENA* output_arena, REGEX_SCANNER* lex){
	STACK* stack = stack_create(stack_arena, 1);

	while(1){
		U16 token = lexer_consume_postfix(lex);
		if(token == EXPR_END || token == EXPR_ERROR){
			fprintf(stdout, "end parse. Check in debugger for bad character\n");
			break;
		}

		STACK_OPERAND a,b,c;
		switch(token){
			case OP_BAR:
				a = stack_pop(stack);
				b = stack_pop(stack);
				c = stack_operand_branch(output_arena, a, b);
				stack_push(stack_arena, stack, c);

			break;
			case OP_CAT :
				a = stack_pop(stack);
				b = stack_pop(stack);
				c = stack_operand_concatenate(output_arena, b, a);
				stack_push(stack_arena, stack, c);
			break;
			case OP_STAR :
				a = stack_pop(stack);
				c = stack_operand_kleene_star(output_arena, a);
				stack_push(stack_arena, stack, c);
			break;
			case OP_PLUS :
				a = stack_pop(stack);
				c = stack_operand_closure(output_arena, a);
				stack_push(stack_arena, stack, c);
			break;
			case OP_OPT :
				a = stack_pop(stack);
				c = stack_operand_optional(output_arena, a);
				stack_push(stack_arena, stack, c);
			break;
			case OP_OR :
				a = stack_pop(stack);
				b = stack_pop(stack);
				c = stack_operand_union(a,b);
				stack_push(stack_arena, stack, c);
			break;
			case OP_AND :
				a = stack_pop(stack);
				b = stack_pop(stack);
				c = stack_operand_intersection(a, b);
				stack_push(stack_arena, stack, c);
			break;
			case OP_NOT :
				a = stack_pop(stack);
				c = stack_operand_complement(a);
				stack_push(stack_arena, stack, c);
			break;
			default:
				c = stack_operand_from_token(token);
				stack_push(stack_arena, stack, c);
		}

	}

	STACK_OPERAND output = stack_pop(stack);


	
	
	return output.nfa;

}



typedef struct {
	CHARACTER_SET set;
	U32 next[2];
} NFA_COMPACT_STATE;

typedef struct {
	U32 num_states;
	U32 start; U32 end;
	NFA_COMPACT_STATE states[];
} NFA_COMPACT;




U64 nfa_add_compacted_state(NFA_COMPACT_STATE* output, NFA_STATE* state){



	static U64 id = 1;
	if(state->id != 0){
		return 0;
	}

	state->id = id++;


	U32 index = state->id;
	if(state->next1 == nullptr && state->next2 == nullptr){
	//	type = STATE_ACCEPT;
		output[index].next[0] = 0;
		output[index].next[1] = 0;
		output[index].set.upper = 0;
		output[index].set.lower = 0;
		return state->id;

	}else if(state->next1 == state->next2){
	//	type = STATE_CONSUME;
		U32 end = nfa_add_compacted_state(output, state->next1);

		output[index].next[0] = state->next1->id;
		output[index].next[1] = state->next2->id;
		output[index].set = state->set;
		return end;
	}
	else if(state->next1 != nullptr && state->next2 == nullptr){
	//	type = STATE_EPSILON1;
		U32 end = nfa_add_compacted_state(output, state->next1);

		output[index].next[0] = state->next1->id;
		output[index].next[1] = 0;
		output[index].set = state->set;
		return end;

	}else if(state->next1 != nullptr && state->next2 != nullptr){
	//	type = STATE_EPSILON2;	
		U32 end1 = nfa_add_compacted_state(output, state->next1);
		U32 end2 = nfa_add_compacted_state(output, state->next2);
		output[index].next[0] = state->next1->id;
		output[index].next[1] = state->next2->id;
		output[index].set = state->set;
		return end1 + end2;
	}

}



NFA_COMPACT* nfa_compact(MEM_ARENA* arena, NFA nfa){
	NFA_COMPACT* compacted_nfa = mem_arena_push(arena, sizeof(NFA_COMPACT_STATE) * (nfa.num_states+1) + sizeof(NFA_COMPACT));

	U32 end	= nfa_add_compacted_state(compacted_nfa->states, nfa.start);

	compacted_nfa->start = 1;
	compacted_nfa->end = end;
	compacted_nfa->num_states = nfa.num_states;
	return compacted_nfa;
}









































