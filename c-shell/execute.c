#include <unistd.h>
#include <sys/stat.h>

int execute_is_executable(const char *filepath) {
    struct stat st;
    if (access(filepath, X_OK) != 0) return 0; //does path have exec permissin
    if (stat(filepath, &st) != 0) return 0; //get info about te path
    if (S_ISDIR(st.st_mode)) return 0; //dir not executables
    return 1; //if executable hai, return 1
}