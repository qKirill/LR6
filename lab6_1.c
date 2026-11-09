#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define ROOT "lab6_3"
#define PATH_LEN 1024

typedef struct {
    int dirs;
    int files;
} Counts;

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

static int cmp_names(const void *a, const void *b)
{
    return strcasecmp(*(char *const *)a, *(char *const *)b);
}

static void print_tree(const char *path, const char *prefix, Counts *counts)
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror(path);
        return;
    }

    char **names = NULL;
    size_t n = 0;
    size_t cap = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        if (n == cap) {
            size_t new_cap = cap ? cap * 2 : 16;
            char **tmp = realloc(names, new_cap * sizeof(*names));
            if (tmp == NULL) {
                perror("realloc");
                break;
            }
            names = tmp;
            cap = new_cap;
        }

        names[n] = strdup(entry->d_name);
        if (names[n] == NULL) {
            perror("strdup");
            break;
        }
        n++;
    }
    closedir(dir);

    qsort(names, n, sizeof(*names), cmp_names);

    for (size_t i = 0; i < n; i++) {
        int last = (i == n - 1);
        char full[PATH_LEN];
        struct stat st;

        snprintf(full, sizeof(full), "%s/%s", path, names[i]);
        printf("%s%s%s\n", prefix, last ? "└── " : "├── ", names[i]);

        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            char next[PATH_LEN];

            counts->dirs++;
            snprintf(next, sizeof(next), "%s%s", prefix, last ? "    " : "│   ");
            print_tree(full, next, counts);
        } else {
            counts->files++;
        }

        free(names[i]);
    }
    free(names);
}

int main(void)
{
    Counts counts = {0, 0};

    if (create_structure() == -1) {
        return 1;
    }

    printf("%s\n", ROOT);
    print_tree(ROOT, "", &counts);
    printf("\n%d directories, %d files\n", counts.dirs, counts.files);

    return 0;
}