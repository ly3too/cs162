/**********************************
 * @author      ly3too
 *
 **********************************/

#ifndef _REALPATH_
#define _REALPATH_

#ifdef __cplusplus
extern "C" {
#endif

char *realpath(const char *path, char resolved_path[PATH_MAX]);

#ifdef __cplusplus
}
#endif
#endif
