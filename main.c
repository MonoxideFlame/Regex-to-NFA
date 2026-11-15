
#include <stdio.h>
#define ARENA
#define STRING
#include "base.h"


#include "lexer.h"
#include "build_NFA.h"

int strlen(const char* s){

    for(int i = 0;; i++){
        if(s[i] == 0){
            return i;
        }
    }
}

B32 set_contains(CHARACTER_SET set, char a){
	U64 q;
	if(a < 64){
		q = (set.lower >> a) & 1;
	} else {
		q = (set.upper >> (a - 64)) & 1; 
	}
	return q;

}


void printSet(CHARACTER_SET set){
	for(unsigned char a = 0; a < 128; a++){
		if(set_contains(set, a)){
			printf("%c", a);
		}	
	}
}



void printNFA(NFA_COMPACT* nfa){
	
	printf("NUMBER OF STATES: %u\n", nfa->num_states);
	printf("START: %u\n", nfa->start);
	printf("END: %u\n", nfa->end);

	for(U32 i = 1; i < nfa->num_states + 1; i++){
		NFA_COMPACT_STATE s = nfa->states[i];
		if(s.next[0] == 0 && s.next[1] == 0){
			printf("STATE_ID: %u (accept)\n", i);
		}else if(s.next[0] == s.next[1]){
			printf("STATE_ID: %u (consume), \n\t SET:", i);
			printSet(s.set);
			printf("\n\tNEXT: %u\n", s.next[0]);
		}
		else if(s.next[0] != 0 && s.next[1] == 0){
			printf("STATE_ID: %u (epsilon1),\n\tNEXT: %u\n", i, s.next[0]);
		}else if(s.next[0] != 0 && s.next[1] != 0){
			printf("STATE_ID: %u (epsilon2), \n\tNEXT: %u\n\tNEXT: %u\n",i, s.next[0], s.next[1]);
		}
	}


}


int main(int argc, const char** argv){

    STRING8 expression = {nullptr, 0};
    if(argc > 1){
        expression.buffer = (U8*)argv[1];
        expression.length = strlen(argv[1]);
    }
    if(expression.length == 0){
        printf("no expression provided");
        return 1; //no expression provided
    }

	MEM_ARENA* perm_arena = mem_arena_create(MiB(1), KiB(4));

	NFA_COMPACT* compact_nfa;
	
	{
		REGEX_SCANNER lexer = lexer_create(expression);


		MEM_ARENA* nfa_arena = mem_arena_create(MiB(1), KiB(4));

		NFA nfa = build_NFA(perm_arena, nfa_arena, &lexer);

		mem_arena_clear(perm_arena);

		compact_nfa = nfa_compact(perm_arena, nfa);

		mem_arena_destroy(nfa_arena);
	}

	printNFA(compact_nfa);

	mem_arena_destroy(perm_arena);
    return 0;
}
