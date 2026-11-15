
typedef struct {
	U32 next_states[128];
} DFA_STATE;

typedef struct {
	U32 num_states;
	U32 start;
	DFA_STATE states[];
} DFA;


typedef struct DFA_CACHE_LINE{
	U64 tag;//zobrist like system to provide fast comparisons. If this passes, we have to check the sets for equality by hand.
	U32 id;
	U32 len;
	DFA_CACHE_LINE* next;
	U32 set[];
} DFA_CACHE_LINE;

typedef struct {
	DFA_CACHE_LINE* lines[65536];
} DFA_SET_CACHE;


DFA_CACHE_LINE* dfa_cache_line_make(




void dfa_cache_push(MEM_ARENA* arena, DFA_SET_CACHE* cache, DFA_CACHE_LINE* line){
	U64 hash = 0;

	for(U32 i = 0; i < len; i++){
		hash ^= dfa_state_set_hash(set[i]);
	}

	U16 index = hash % 65536;
	if(cache->lines[index] == nullptr){
		
	}


}

void dfa_cache_contains(DFA_SET_CACHE* cache, U32* set, U32 len){





}






DFA* nfa_to_dfa(MEM_ARENA* output_arena, NFA_COMPACT* input_nfa){
	
}




















