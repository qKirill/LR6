#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#define ROOT "lab6_3"
#define PATH_LEN 1024

static int make_dir(const char *path)
{
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        perror(path);
        return -1;
    }
    return 0;
}

static int make_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        perror(path);
        return -1;
    }
    fputs(content, f);
    fclose(f);
    return 0;
}

static int create_structure(void)
{
    static const char *dirs[] = {
        ROOT,
        ROOT "/.git",
        ROOT "/src",
        ROOT "/bin",
        ROOT "/docs",
        ROOT "/img",
    };

    static const struct {
        const char *path;
        const char *content;
    } files[] = {
        {ROOT "/.git/config",          "[core]\n\trepositoryformatversion = 0\n"},
        {ROOT "/src/main.c",           "int main(void)\n{\n    return 0;\n}\n"},
        {ROOT "/docs/manual.txt",      "Manual\n"},
        {ROOT "/docs/presentation.pdf", "%PDF-1.4\n"},
        {ROOT "/img/cat.jpg",          "cat\n"},
        {ROOT "/img/dog.png",          "dog\n"},
        {ROOT "/README.md",            "# lab6_3\n"},
    };

    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        if (make_dir(dirs[i]) == -1) {
            return -1;
        }
    }

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        if (make_file(files[i].path, files[i].content) == -1) {
            return -1;
        }
    }

    return 0;
}

static void print_tree(const char *path, int depth)
{
    DIR *dir = opendir(path);
    struct dirent *entry;

    if (dir == NULL) {
        perror(path);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char full[PATH_LEN];
        struct stat st;

        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
        printf("%*s%s\n", depth * 4, "", entry->d_name);

        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            print_tree(full, depth + 1);
        }
    }

    closedir(dir);
}

int main(void)
{
    if (create_structure() == -1) {
        return 1;
    }

    printf("%s\n", ROOT);
    print_tree(ROOT, 1);
    return 0;
}