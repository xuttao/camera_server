#include "common.h"
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

static int mkpath(const char *path, mode_t mode)
{
    struct stat st;
    int status = 0;

    if (stat(path, &st) != 0) {
        /* Directory does not exist. EEXIST for race condition */
        if (mkdir(path, mode) != 0 && errno != EEXIST)
            status = -1;
    } else if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        status = -1;
    }
    return (status);
}

int create_dir(const char *path)
{
    char *pp;
    char *sp;
    int status;
    char *copypath = strdup(path);
    mode_t mode = 0755;
    status = 0;
    pp = copypath;
    while (status == 0 && (sp = strchr(pp, '/')) != 0) {
        if (sp != pp) {
            /* Neither root nor double slash in path */
            *sp = '\0';
            status = mkpath(copypath, mode);
            *sp = '/';
        }
        pp = sp + 1;
    }
    if (status == 0)
        status = mkpath(path, mode);
    free(copypath);
    return (status);
}

std::vector<std::string> get_dir_file(const char *path, const char *file_suffix)
{
    DIR *dp;
    struct dirent *dirp;
    if ((dp = opendir(path)) == NULL) {
        assert(false);
    }
    std::string dir_str(path);
    if (dir_str[dir_str.size() - 1] == '/') {
        dir_str = dir_str.substr(0, dir_str.size() - 1);
    }
    std::vector<std::string> res;
    res.reserve(100);
    while ((dirp = readdir(dp)) != NULL) {
        if (DT_REG == dirp->d_type) {
            std::string file = dirp->d_name;
            auto pos2 = file.rfind('.');
            auto suffix = file.substr(pos2 + 1, file.size() - pos2 - 1);
            if (file_suffix == suffix) {
                res.push_back(dir_str + "/" + file);
            }
        }
    }
    return res;
}
