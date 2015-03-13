#include "intel_hpxMP.h"
#include <boost/shared_ptr.hpp>
#include <iostream>
#include <assert.h>

using std::cout;
using std::endl;

boost::shared_ptr<hpx_runtime> hpx_backend;
//mutex_type print_mtx{};

void start_backend(){
    if(!hpx_backend) {
        hpx_backend.reset(new hpx_runtime());
    }
}

int __kmpc_ok_to_fork(ident_t *loc){
    return 1;
}

void __kmpc_begin( ident_t *, kmp_int32 flags ) {
    start_backend();
}

void __kmpc_end(ident_t *loc) {
}

void
__kmpc_fork_call(ident_t *loc, kmp_int32 argc, kmpc_micro microtask, ...) {
    vector<void*> argv(argc);
    if(!hpx_backend) {
        start_backend();
    }

    va_list     ap;
    va_start(   ap, microtask );

    for( int i = 0; i < argc; i++ ){
        argv[i] = va_arg( ap, void * );
    }
    va_end( ap );
    void ** args = argv.data();

    hpx_backend->fork(__kmp_invoke_microtask, microtask, argc, args);
}

// ----- Tasks -----

//sizeof_kmp_task_t includes the private variables for the task
kmp_task_t*
__kmpc_omp_task_alloc( ident_t *loc_ref, kmp_int32 gtid, kmp_int32 flags,
                       size_t sizeof_kmp_task_t, size_t sizeof_shareds,
                       kmp_routine_entry_t task_entry ){

    //kmp_tasking_flags_t *input_flags = (kmp_tasking_flags_t *) & flags;
    //TODO: do I need to do something with these flags?
    
    int task_size = sizeof_kmp_task_t + (-sizeof_kmp_task_t%8);

    kmp_task_t *task = (kmp_task_t*)new char[task_size + sizeof_shareds]; 
    //kmp_task_t *task = (kmp_task_t*)calloc(1,task_size + sizeof_shareds); 


    //This gets deleted at the end of intel_task_setup
    task->routine = task_entry;
    if( sizeof_shareds == 0 ) {
        task->shareds = NULL;
    } else {
        //task->shareds = (char*)task + sizeof_kmp_task_t;
        task->shareds = &((char*) task)[task_size];
    }
    task->part_id = 0;

    return task;
}

int __kmpc_omp_task( ident_t *loc_ref, kmp_int32 gtid, kmp_task_t * new_task){
    hpx_backend->create_intel_task(new_task->routine, gtid, new_task);
    return 1;
}

//do I insert the stub future into the list here? 
//No, after new future is made for this task.
shared_future<void> get_future_from_dep(kmp_depend_info_t dep, depends_map *df_map) {
    if(df_map->count(dep.base_addr) == 0) {
        return hpx::make_ready_future();
    }
    return df_map->at(dep.base_addr);
}

void test_then_func(shared_future<void> in) {
    in.wait();
    cout << "done waiting\n";
}

// return 1 if suspended and queued the current task to be resumed later, 0 if not.
kmp_int32 
__kmpc_omp_task_with_deps( ident_t *loc_ref, kmp_int32 gtid, kmp_task_t * new_task,
                           kmp_int32 ndeps, kmp_depend_info_t *dep_list,
                           kmp_int32 ndeps_noalias, kmp_depend_info_t *noalias_dep_list ){
    //TODO:how to I handle immediate tasks?
    // read them from flags?
    if(ndeps == 0 && ndeps_noalias == 0) {
        hpx_backend->create_intel_task(new_task->routine, gtid, new_task);
        return 1;
    }
    //map<int64_t, shared_future<void>> *df_map = &(hpx_backend->get_task_data()->df_map);
    depends_map *df_map = &(hpx_backend->get_task_data()->df_map);

    vector<shared_future<void>> dep_futures;
    int real_deps = 0;

    for(int i = 0; i < ndeps; i++) {
        if(df_map->count(dep_list[i].base_addr) > 0) {
            real_deps++;
            dep_futures.push_back(df_map->at(dep_list[i].base_addr));
        }
    }
    if(real_deps > 0) {
        //hpx::async(new_task->routine, gtid, new_task);
        //hpx::when_all(dep_futures); //.then(test_then_func);
    } else {
        hpx_backend->create_intel_task(new_task->routine, gtid, new_task);
    }
    auto wrapped_routine = hpx::util::unwrapped(new_task->routine);
    auto futurized_task_data = hpx::make_ready_future(new_task);
    auto futurized_gtid = hpx::make_ready_future(gtid);
    shared_future<void> task_future = dataflow( wrapped_routine, futurized_gtid, futurized_task_data, when_all(dep_futures) );

    //add future to map
    /*
    for(int i = 0 ; i < ndeps; i++) {
        if(dep_list[i].flags.out) {
            df_map->at(dep_list[i].base_addr) = dep_future;
        }
    }
    */
    return 1;
}

// __kmpc_omp_task_parts: Schedule a thread-switchable task for execution
// loc_ref: location of original task pragma (ignored)
// gtid: Global Thread ID of encountering thread
// new_task: task thunk allocated by __kmp_omp_task_alloc() for the ''new task''
// Returns:
//    TASK_CURRENT_NOT_QUEUED (0) if did not suspend and queue current task to be resumed later.
//    TASK_CURRENT_QUEUED (1) if suspended and queued the current task to be resumed later.
int __kmpc_omp_task_parts( ident_t *loc_ref, int gtid, kmp_task_t * new_task) {
    hpx_backend->create_intel_task(new_task->routine, gtid, new_task);
    return 0;
}


kmp_int32 __kmpc_omp_taskwait( ident_t *loc_ref, kmp_int32 gtid ){
    hpx_backend->task_wait();
    return 0;
}

kmp_int32 __kmpc_omp_taskyield(ident_t *loc_ref, kmp_int32 gtid, int end_part ){
    hpx::this_thread::yield();
    return 0;
}

void
__kmpc_omp_wait_deps( ident_t *loc_ref, kmp_int32 gtid, kmp_int32 ndeps, 
                      kmp_depend_info_t *dep_list, kmp_int32 ndeps_noalias, 
                      kmp_depend_info_t *noalias_dep_list ){
    //Not handling dependencies at the moment
    if(ndeps > 0 || ndeps_noalias > 0){
        cout <<"task has more than 0 dependencies. This runtime does not support task dependencies\n";
        assert(ndeps == 0);
        assert(ndeps_noalias == 0);
    }
}

void __kmpc_omp_task_begin_if0( ident_t *loc_ref, kmp_int32 gtid, kmp_task_t * task ){
    task->routine(gtid, task);
}
void
__kmpc_omp_task_complete_if0( ident_t *loc_ref, kmp_int32 gtid, kmp_task_t *task) {
    //This pairs up with task_begin_if0, waiting for the task that if0 starts.
    //For now, I am just going to execute the thread in task begin, and do nothing here.
}

// ----- End Tasks -----

void 
__kmpc_push_num_threads( ident_t *loc, 
                         kmp_int32 global_tid, 
                         kmp_int32 num_threads ){
    start_backend();
    //TODO: this needs to be local to the task
    //hpx_backend->set_num_threads(num_threads);
    omp_task_data *data = hpx_backend->get_task_data();
    data->set_threads_requested( num_threads );
}

//W: perf
void
__kmpc_barrier(ident_t *loc, kmp_int32 global_tid) {
    hpx_backend->barrier_wait();
}

int  __kmpc_cancel_barrier(ident_t* loc_ref, kmp_int32 gtid){
    hpx_backend->barrier_wait();
    return 0;
}

//W: correctness?
int __kmpc_global_thread_num(ident_t *loc){
    if(hpx_backend)
        return hpx_backend->get_thread_num();
    return 0;
}

int __kmpc_single(ident_t *loc, int tid){
    if(!hpx_backend || !hpx::threads::get_self_ptr() ) {
        return 1;
    }
    parallel_region *team = hpx_backend->get_team();
    int num_threads = hpx_backend->get_num_threads();

    team->single_mtx.lock();
    if(team->current_single_thread == -1 && team->single_counter == 0) {
        team->current_single_thread = tid;
        team->single_counter = 1 - num_threads;
    } else {
        team->single_counter++;
    }
    team->single_mtx.unlock();
    if(team->current_single_thread == tid) {
        return 1;
    }
    return 0;
}

//in the intel runtime, only the single thread calls this
void __kmpc_end_single(ident_t *loc, int tid){
    if(!hpx_backend || !hpx::threads::get_self_ptr() ) {
        return;
    }
    parallel_region *team = hpx_backend->get_team();
    team->single_mtx.lock();
    if(team->single_counter == 0) {
        team->current_single_thread = -1;
    }
    team->single_mtx.unlock();
}

int __kmpc_master(ident_t *loc, int global_tid){
    //TODO: if master can be called from tasks, then this doesn't work.
    if(hpx_backend->get_thread_num() == 0) {
        return 1;
    }
    return 0;
}

void __kmpc_end_master(ident_t *loc, int global_tid){
}

//W: perf and correctness
void
__kmpc_critical( ident_t * loc, kmp_int32 global_tid, kmp_critical_name * crit ) {
    parallel_region *team = hpx_backend->get_team();
    team->crit_mtx.lock();
}

void
__kmpc_end_critical(ident_t *loc, kmp_int32 global_tid, kmp_critical_name *crit) {
    parallel_region *team = hpx_backend->get_team();
    team->crit_mtx.unlock();
}

void __kmpc_flush(ident_t *loc, ...){
    __sync_synchronize();
}

//W: perf and correctness
//I think I need to pair up *data to with the memory allocated to represend the threadlocal version
void* __kmpc_threadprivate_cached( ident_t *loc, kmp_int32 tid, void *data, size_t size, void ***cache){
    if(!hpx_backend) {
        start_backend();
    }
    parallel_region *team = hpx_backend->get_team();
    int num_threads = hpx_backend->get_num_procs();
    if(!(*cache)){
        team->thread_mtx.lock();
        if(!(*cache)){
            *cache = (void**)calloc( 8, num_threads);//FIXME: this is never deallocated.
        }
        team->thread_mtx.unlock();
    }
    if( !((*cache)[tid]) ) {
        (*cache)[tid] = new char[size];
        std::memcpy((*cache)[tid], data, size);
    }
    return (*cache)[tid];
}

//W: perf and correctness
//Only one of the threads (called the single thread) should have the didit variable set to 1
//This function copies the copyprivate variable of the task that got ran the single
// into the other implicit tasks, at the end of the single region
// it also needs to cause other implicit tasks to wait until the single task is complete
void
__kmpc_copyprivate( ident_t *loc, kmp_int32 gtid, size_t cpy_size, void *cpy_data, void(*cpy_func)(void*,void*), kmp_int32 didit )
{
    void **data_ptr = &(hpx_backend->get_team()->copyprivate_data);
    if(didit) {
        *data_ptr = cpy_data;
    }
    hpx_backend->barrier_wait();
    cpy_func(cpy_data, *data_ptr);
    hpx_backend->barrier_wait();
}

int __kmpc_reduce_nowait( ident_t *loc, kmp_int32 gtid, kmp_int32 num_vars, size_t size,
                      void *data,  void (*reduce)(void *lhs, void *rhs), kmp_critical_name *lck ) {
    //This is 2 because the compiler generates a version where the runtime does the work(0/1 are
    //returned) and a version with calls to atomic operations that is used when 2 is returned.
    return 2;
    //return __kmpc_reduce(loc, gtid, num_vars, size, data, reduce, lck);
}

void __kmpc_end_reduce_nowait( ident_t *loc, kmp_int32 gtid, kmp_critical_name *lck ) {
    __kmpc_end_reduce( loc, gtid, lck );
}

/* A blocking reduce that includes an implicit barrier.
 *
 * num_vars number of items (variables) to be reduced
 * reduce_size size of data in bytes to be reduced
 * reduce_data pointer to data to be reduced
 * reduce_func callback function providing reduction operation on two operands and returning
 * result of reduction in lhs_data
 * param lck pointer to the unique lock data structure
 * @result 1 for the master thread, 0 for all other team threads, 2 for all team threads if atomic
 * reduction needed
 */
int 
__kmpc_reduce( ident_t *loc, kmp_int32 gtid, kmp_int32 num_vars, size_t size, 
               void *data, void (*func)(void *lhs, void *rhs), kmp_critical_name *lck ) {
    /*
    int is_master = __kmpc_single(loc, gtid);
    auto *team = hpx_backend->get_team();
    int num_threads = team->num_threads;
    team->reduce_data[gtid] = data;
    hpx_backend->barrier_wait();
    if(is_master) {
        for( int i = 0; i < num_threads; i++ ) {
            if(i != gtid) {
                func(data, team->reduce_data[i]);
            }
        }
    }
    hpx_backend->barrier_wait();
    return is_master;
    */
    hpx_backend->barrier_wait();
    return 2;
}

void
__kmpc_end_reduce( ident_t *loc, kmp_int32 gtid, kmp_critical_name *lck ) {
}

//Library functions:--------------------------------------------------
int omp_get_thread_num(){
    if(hpx_backend)
        return hpx_backend->get_thread_num();
    else
        return 1;
}

//"returns the number of threads in the current team"
int omp_get_num_threads(){
    if(!hpx_backend) {
        start_backend();
    }                      
    return hpx_backend->get_num_threads();
}

//W: perf and correctness
void omp_get_num_threads(int num_threads){
    if(!hpx_backend) {
        start_backend();
    }                      
    hpx_backend->set_num_threads(num_threads);
}

//W: perf and correctness
int omp_get_max_threads() {
    if(!hpx_backend) {
        start_backend();
    }                      
    return hpx_backend->get_task_data()->icv.nthreads;
}

//W: perf and correctness
int omp_get_num_procs(){
    if(!hpx_backend)
        start_backend();
    return hpx_backend->get_num_procs();
}

//W: perf and correctness
void omp_set_num_threads(int num_threads) {
    if(!hpx_backend) {
        start_backend();
    }
    hpx_backend->get_task_data()->set_threads_requested(num_threads);
}

double omp_get_wtime(){
    if(!hpx_backend)
        start_backend();
    return hpx_backend->get_time();
}

double omp_get_wtick(){
    return .000000001;
}

int omp_in_parallel(){
    if(!hpx_backend) {
        start_backend();
    }
    int active_levels = hpx_backend->get_task_data()->icv.active_levels;
    return (active_levels > 0);
}


void omp_set_dynamic(int dynamic_threads){
    if(!hpx_backend) {
        start_backend();
    }
    hpx_backend->get_task_data()->icv.dyn = (dynamic_threads != 0);
}

int omp_get_dynamic(){
    if(!hpx_backend) {
        start_backend();
    }
    return hpx_backend->get_task_data()->icv.dyn;
}

//W: perf and correctness
void omp_init_lock(omp_lock_t **lock){
    if(!hpx_backend) {
        start_backend();
    }
    *lock = new omp_lock_t;
}

//W: perf and correctness
void omp_init_nest_lock(omp_lock_t **lock){
    if(!hpx_backend) {
        start_backend();
    }
    *lock = new omp_lock_t;
}

//W: perf and correctness
void omp_destroy_lock(omp_lock_t **lock) {
    delete *lock;
}
//W: perf and correctness
void omp_destroy_nest_lock(omp_lock_t **lock) {
    delete *lock;
}

//W: perf and correctness
int omp_test_lock(omp_lock_t **lock) {
    if((*lock)->try_lock())
        return 1;
    return 0;
}
//W: perf and correctness
int omp_test_nest_lock(omp_lock_t **lock) {
    if((*lock)->try_lock())
        return 1;
    return 0;
}

//W: perf and correctness
void omp_set_lock(omp_lock_t **lock) {
    (*lock)->lock();
}
//W: perf and correctness
void omp_set_nest_lock(omp_lock_t **lock) {
    (*lock)->lock();
}

//W: perf and correctness
void omp_unset_lock(omp_lock_t **lock) {
    (*lock)->unlock();
}
//W: perf and correctness
void omp_unset_nest_lock(omp_lock_t **lock) {
    (*lock)->unlock();
}


