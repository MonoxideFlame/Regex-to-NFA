
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;
typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef float F32;
typedef double F64;
typedef I32 B32;
typedef I8 B8;

static I8 I8_MAX = (I8)0x7full;
static I16 I16_MAX = (I16)0x7fffull;
static I32 I32_MAX = (I32)0x7fffffffull;
static I64 I64_MAX = (I64)0x7fffffffffffffffull;

static I8 I8_MIN = (I8)0x80ull;
static I16 I16_MIN = (I16)0x8000ull;
static I32 I32_MIN = (I32)0x80000000ull;
static I64 I64_MIN = (I64)0x8000000000000000ull;

static U8 U8_MAX = (U8)0xffull;
static U16 U16_MAX = (U16)0xffffull;
static U32 U32_MAX = (U32)0xffffffffull;
static U64 U64_MAX = (U64)0xffffffffffffffffull;

static F32 F32_MACHINE_EPSILON = 1.1920929e-7f;
static F64 F64_MACHINE_EPSILON = 2.22044604925e-16;

#define nullptr ((void*)0ull)

#define KiB(n) ((U64)(n) << 10)
#define MiB(n) ((U64)(n) << 20)
#define GiB(n) ((U64)(n) << 30)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define CLAMP(a,x,b) (((x)<(a))?(a):(((b)<(x))?(b):(x)))
#define ALIGN_UP_POW2(n, p) (((U64)(n) + ((U64)(p) - 1)) & (~((U64)(p) - 1)))



#if defined(_WIN32)

#include <windows.h>

U32 plat_get_pagesize(void) {
    SYSTEM_INFO sysinfo = { 0 };
    GetSystemInfo(&sysinfo);

    return sysinfo.dwPageSize;
}

void* plat_mem_reserve(U64 size) {
    return VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
}

B32 plat_mem_commit(void* ptr, U64 size) {
    void* ret = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    return ret != nullptr;
}

B32 plat_mem_decommit(void* ptr, U64 size) {
    return VirtualFree(ptr, size, MEM_DECOMMIT);
}

B32 plat_mem_release(void* ptr, U64 size) {
    return VirtualFree(ptr, size, MEM_RELEASE);
}


#elif defined(__linux__)

//#define _DEFAULT_SOURCE

#include <unistd.h>
#include <sys/mman.h>

U32 plat_get_pagesize(void) {
    return (U32)sysconf(_SC_PAGESIZE);
}

void* plat_mem_reserve(U64 size) {
    void* out = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (out == MAP_FAILED) {
        return nullptr;
    }
    return out;
}

B32 plat_mem_commit(void* ptr, U64 size) {
    I32 ret = mprotect(ptr, size, PROT_READ | PROT_WRITE);
    return ret == 0;
}

B32 plat_mem_decommit(void* ptr, U64 size) {
    I32 ret = mprotect(ptr, size, PROT_NONE);
    if (ret != 0) return false;
    ret = madvise(ptr, size, MADV_DONTNEED);
    return ret == 0;
}

B32 plat_mem_release(void* ptr, U64 size) {
    I32 ret = munmap(ptr, size);
    return ret == 0;
}


#endif


#ifdef ARENA

#define ARENA_ALIGN (sizeof(void*))


struct MEM_ARENA_FRAME {
    struct MEM_ARENA_FRAME* previous_frame;
    U64 pos;   
};

typedef struct {
    U64 reserve_size;
    U64 commit_size;

    U64 pos;
    U64 commit_pos;
    struct MEM_ARENA_FRAME* last_frame;
} MEM_ARENA;





MEM_ARENA* mem_arena_create(U64 reserve_size, U64 commit_size) {
    U32 pagesize = plat_get_pagesize();

    reserve_size = ALIGN_UP_POW2(reserve_size, pagesize);
    commit_size = ALIGN_UP_POW2(commit_size, pagesize);

    MEM_ARENA* mem_arena = plat_mem_reserve(reserve_size);

    if (!plat_mem_commit(mem_arena, commit_size)) {
        return nullptr;
    }

    mem_arena->reserve_size = reserve_size;
    mem_arena->commit_size = commit_size;
    mem_arena->pos = sizeof(MEM_ARENA);
    mem_arena->commit_pos = commit_size;
    mem_arena->last_frame = nullptr;
    
    return mem_arena;
}

void mem_arena_destroy(MEM_ARENA* mem_arena) {
    plat_mem_release(mem_arena, mem_arena->reserve_size);
}

void* mem_arena_push(MEM_ARENA* mem_arena, U64 size) {
    if(mem_arena == nullptr){
        return nullptr;
    }
    U64 pos_aligned = ALIGN_UP_POW2(mem_arena->pos, ARENA_ALIGN);
    U64 new_pos = pos_aligned + size;

    if (new_pos > mem_arena->reserve_size) { return nullptr; }

    if (new_pos > mem_arena->commit_pos) {
        U64 new_commit_pos = new_pos;
        new_commit_pos += mem_arena->commit_size - 1;
        new_commit_pos -= new_commit_pos % mem_arena->commit_size;
        new_commit_pos = MIN(new_commit_pos, mem_arena->reserve_size);

        U8* mem = ((U8*)mem_arena) + mem_arena->commit_pos;
        U64 commit_size = new_commit_pos - mem_arena->commit_pos;

        if (!plat_mem_commit(mem, commit_size)) {
            return nullptr;
        }

        mem_arena->commit_pos = new_commit_pos;
    }

    mem_arena->pos = new_pos;

    U8* out = ((U8*)mem_arena) + pos_aligned;

    return out;
}

void mem_arena_pop(MEM_ARENA* mem_arena, U64 size) {
    if(mem_arena == nullptr){
        return;
    }
    size = MIN(size, mem_arena->pos - sizeof(MEM_ARENA));
    mem_arena->pos -= size;
}

void mem_arena_pop_to(MEM_ARENA* mem_arena, U64 pos) {
    if(mem_arena == nullptr){
        return;
    }
    U64 size = pos < mem_arena->pos ? mem_arena->pos - pos : 0;
    mem_arena_pop(mem_arena, size);
}
void mem_arena_clear(MEM_ARENA* mem_arena) {
    if(mem_arena == nullptr){
        return;
    }
    mem_arena_pop_to(mem_arena, sizeof(MEM_ARENA));
}


B32 mem_arena_grow(MEM_ARENA* arena, void* allocation, U64 current_size, U64 new_size){
    if(arena->pos + ((void*)arena) == allocation + current_size){
        mem_arena_push(arena, new_size - current_size);
        return 1;
    }
    return 0;
}




#endif


#ifdef ARENA_SCRATCH

//creates a "bookmark" in the arena. When end frame is called, the arena is cleared until the last bookmark. 
B32 mem_arena_start_frame(MEM_ARENA* arena){
    //get teh data we will need for a frame. 
    if(arena == nullptr){
        return false;
    }
    U64 restore_pos = arena->pos;
    struct MEM_ARENA_FRAME* last_frame = arena->last_frame;

    struct MEM_ARENA_FRAME* new_frame = mem_arena_push(arena, sizeof(struct MEM_ARENA_FRAME));

    if(new_frame == nullptr){
        return false;
    }


    new_frame->previous_frame = last_frame;
    new_frame->pos = restore_pos;
    arena->last_frame = new_frame;
    return true;
}

//reverts to last bookmark. If no bookmark is present, it reverts nothing.
//

B32 mem_arena_end_frame(MEM_ARENA* arena){

    if(arena == nullptr){
        return false;
    }
    if(arena->last_frame == nullptr){
        return false;
    }

    U64 restore_pos = arena->last_frame->pos;

    struct MEM_ARENA_FRAME* new_last_frame = arena->last_frame->previous_frame;
    arena->pos = restore_pos;
    arena->last_frame = new_last_frame;

    return true;
}

MEM_ARENA* mem_scratch_get(MEM_ARENA** bad_arena_list, U64 length){
        
    static __thread MEM_ARENA* scratch_arenas[4]; 
    static __thread U64 scratch_count;

    MEM_ARENA* valid_arena = nullptr;

    //check for an existing arena without conflicts
    for(U64 i = 0; i < scratch_count; i++){
        B32 is_bad = false;
        for(U64 j = 0; j < length; j++){
            if(bad_arena_list[j] == scratch_arenas[i]){
                is_bad = true;
            }
        }
        if(!is_bad){
            valid_arena = scratch_arenas[i];
            break;
        }    
    }
    
    //if we still fail to find a valid arena, make a new one
    if(valid_arena == nullptr){    
        if(scratch_count < 4){
            valid_arena = mem_arena_create(GiB(1), KiB(4));
            if(valid_arena != nullptr){
                scratch_arenas[scratch_count] = valid_arena;
                scratch_count += 1;
            }
        } 
    }
    



//after we find a valid arena
    if(valid_arena != nullptr){
        B32 is_created = mem_arena_start_frame(valid_arena);
        if(is_created){
            return valid_arena;
        } else {
            return nullptr;
        }
    } else {
        return nullptr;
    }
}

B32 mem_scratch_release(MEM_ARENA* scratch){
    if(scratch == nullptr){
        return false;
    }
    return mem_arena_end_frame(scratch);
}



#endif




#ifdef STRING 
typedef struct {
    U8* buffer;
    U64 length;
} STRING8;


#define STRING8_LIT(s) (STRING8){ (U8*)(s), sizeof((s)) - 1 }
#define STRING8_FMT(s8) (int)(s8).length, (s8).buffer


STRING8 string8_substring(STRING8 input, U64 start, U64 end){
    end = MIN(end, input.length);
    start = MIN(end, start);
    return (STRING8){input.buffer + start, end - start};
}

STRING8 string8_concatenate(MEM_ARENA* arena, STRING8 first, STRING8 last){
    STRING8 ret;
    ret.buffer = nullptr;
    ret.length = 0;

    U8* buffer = mem_arena_push(arena, first.length + last.length);

    if(buffer == nullptr){
        return ret;
    }

    ret.length = first.length + last.length;

    ret.buffer = buffer;

    for(U64 i = 0; i < first.length; i++){
        ret.buffer[i] = first.buffer[i];
    }

    for(U64 i = 0; i < last.length; i++){
        ret.buffer[i + first.length] = last.buffer[i];
    }

    return ret;
}

#endif

#ifdef ERRORS

#include <stdio.h>
#include <stdlib.h>

typedef struct STRING8_NODE {
    struct STRING8_NODE* next;
    U8* buffer;
    U64 length;
} STRING8_NODE;


#define ERROR_MSG_MAX_SIZE 106

typedef struct {
    STRING8_NODE* info_msgs_tail;
    STRING8_NODE* info_msgs_head;

    I32 info_msg_count;

    U8 err_code;
    U8 err_msg[ERROR_MSG_MAX_SIZE];
    U8 err_len;

} LOG_CONTEXT;

static __thread MEM_ARENA* _log_info_arena = nullptr;


typedef enum {
    ERR_NONE = 0,
    ERR_LOG = 1,
    ERR_FILE_NOT_FOUND = 2, 
    ERR_ILLEGAL_ARG = 3, 
    ERR_OUT_OF_BOUNDS = 4, 
    ERR_ALLOC_FAIL = 5,
    //error codes only go to 254

} ERROR_CODE;






LOG_CONTEXT log_context_start(){
    LOG_CONTEXT context = {
        .info_msgs_tail = nullptr,
        .info_msgs_head = nullptr,
        .info_msg_count = 0,
        .err_code = 255,
        .err_len = 0
    };
    if(!_log_info_arena){
        _log_info_arena = mem_arena_create(MiB(1), KiB(4));
    }
    B32 info_init = mem_arena_start_frame(_log_info_arena);
    if(!info_init){
        // can happen, will use without logging capability
        context.info_msg_count = -1;
    }

    return context;

}




void log_context_end(LOG_CONTEXT* context){
    if(context->info_msg_count == 0){
        return; //nothing to free
    }


    //if allocations were made to our error arena, we free all the memory
    
    B32 is_ended = mem_arena_end_frame( _log_info_arena);

    if(!is_ended){
        //this is a catastrophic case failure and should never happen, if it does something seriously wrong with the OS and memory map has occurred.
        fprintf(stderr, "log_context_end failed");
        exit(1);
    }
}

void log_emit_error(LOG_CONTEXT* context, U8 error_code, STRING8 msg){
    if(context->err_code == 255){

        U64 length = MIN(ERROR_MSG_MAX_SIZE, msg.length);
        for(U64 i = 0; i < length; i++){
            context->err_msg[i] = msg.buffer[i];
        }
        context->err_len = (U8)length;
        context->err_code = error_code;
    }
}

void log_emit_info(LOG_CONTEXT* context, STRING8 msg){
    //if no logging capability
    if(context->info_msg_count == -1){
        log_emit_error(context, 0, STRING8_LIT("no logging capability"));
        return;
    }


    //if we are able to emit info
    STRING8_NODE* node = mem_arena_push(_log_info_arena, sizeof(STRING8_NODE));
    U8 *buffer = mem_arena_push(_log_info_arena, msg.length);
    //if not enough memory

    if(node == nullptr || buffer == nullptr){
        log_emit_error(context, 0, STRING8_LIT("no space to store log"));
        return;
    }

    //happy path, copy string to arena, and add to our linked list
    
    for(U64 i = 0; i < msg.length; i++){
        buffer[i] = msg.buffer[i];
    }

    node->buffer = buffer;
    node->length = msg.length;
    node->next = nullptr;

    if(context->info_msg_count == 0){
        context->info_msgs_head = node;
        context->info_msgs_tail = node;
        context->info_msg_count++;
        return;
    }

    context->info_msgs_tail->next = node;
    context->info_msgs_tail = node;
    context->info_msg_count++;

}

STRING8 log_error_unwrap(LOG_CONTEXT* context){
    static char* error_codes[] = {"NO_ERROR", "ERR_LOG", "ERR_FILE_NOT_FOUND", "ERR_ILLEGAL_ARG", "ERR_OUT_OF_BOUNDS", "ERR_ALLOC_FAIL"};

    static U8 error_lengths[] = {8, 7, 18, 15, 17, 14};

    if(context->err_code > 5 || context->err_code == 0){
        return (STRING8){context->err_msg, context->err_len};
    }
    return (STRING8){(U8*)error_codes[context->err_code], error_lengths[context->err_code]};

}

//can fail, it simply prints the error and info to the stdout - can be blocking
void log_print_info(LOG_CONTEXT* context){
    if(context->info_msg_count == -1){
        log_emit_error(context, 0, STRING8_LIT("no log capability. nothing to report"));
        return;
    }

    STRING8_NODE* node = context->info_msgs_head;

    while(node != nullptr){
        printf("%*s \n", (int)node->length, node->buffer);
        node = node->next;
    }
}





#endif




#ifdef RANDOM 
//this uses the PCG algorithm to implement an RNG. 



// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

typedef struct { 
    U64 state;  
    U64 inc; 
} PRNG_STATE;

static __thread PRNG_STATE s_prng_state = {
    0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL
};

U32 prng_rand32_r(PRNG_STATE* rng) {
    U64 oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    U32 xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    U32 rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

void prng_seed_r(PRNG_STATE* rng, U64 init, U64 init_seq) {
    rng->state = 0ULL;
    rng->inc = (init_seq << 1ull) | 1ull;
    prng_rand_r(rng);
    rng->state += init;
    prng_rand_r(rng);
}

void prng_seed(U64 init, U64 init_seq){
    return prng_seed_r(s_prng_state, init, init_seq);
}

U32 prng_rand32(void){
    return prng_rand_r(&s_prng_state);
}

U64 prng_rand64_r(PRNG_STATE* rng){
    U64 a = prng_rand32_r(rng);
    U64 b =  prng_rand32_r(rng);
    return (a << 32) | b;
}

U64 prng_rand64(void){
    U64 a = prng_rand32_r(&s_prng_state);
    U64 b = prng_rand32_r(&s_prng_state);
    return (a << 32) | b;
}

//returns random float uniformly distributed from 0 to 1.
//the better way would be to build the float up manually between 1.0 and 2.0, and subtraact 1 after.
F32 prng_randf_r(PRNG_STATE* rng){
    return (F32) (prng_rand32_r(rng) / ((F32)U32_MAX));
}

F32 prng_randf(void){
    return prng_randf_r(&s_prng_state);
}

F64 prng_randd_r(PRNG_STATE* rng){
    return (F64) (prng_rand64_r(rng) / ((F64)U64_MAX));
}

F32 prng_randd(void){
    return prng_randd_r(&s_prng_state);
}

F64 prng_binom12_r(PRNG_STATE* rng){
    double sum = -6.0;
    for(U32 i = 0; i < 12; i++){
        sum += prng_randd_r(rng);
    }
    return sum;
}

F64 prng_binom12(void){
    return prng_binom12_r(&s_prng_state);
}

U64 prng_rand_int_r(PRNG_STATE* rng, U64 lower, U64 upper){
    U64 raw = prng_rand64_r(rng);
    U64 range = (upper - lower);

    U64 lim = ((U64_MAX % range);
    
    while(raw <= lim){
        raw = prng_rand64(rng);
    }
    
    return (raw % range) + lower;
}

U64 prng_rand_int(U64 lower, U64 upper){
    return prng_ranf_int_r(&s_prng_state);
}

#endif

