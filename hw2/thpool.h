/**********************************
 * @author      ly3too
 *
 **********************************/

#ifndef _THPOOL_
#define _THPOOL_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG_THPOOL
    #define DEBUG_FUNC(func, arg) func arg
#else
    #define DEBUG_FUNC(func, arg) do{} while(0)
#endif

/**/
int thpool_init(int num_th, int que_size);

/* add job to jobque, arg will be freed after task finshed */
int thpool_add_job(void (*func) (void *), void *arg);

/* wait until jobs in que to finish and free thread pool */
void thpool_destroy();

#ifdef __cplusplus
}
#endif
#endif
