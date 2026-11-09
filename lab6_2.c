#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define LOG_FILE "sort_report.log"
#define PATH_LEN 4096

static FILE *log_file;
static char dest_real[PATH_MAX];

static const struct {
    const char *ext;
    const char *category;
} ext_map[] = {
    {".txt", "Documents"},
    {".md",  "Documents"},
    {".c",   "Sources"},
    {".h",   "Sources"},
    {".go",  "Sources"},
    {".jpg", "Images"},
    {".png", "Images"},
    {".pdf", "PDFs"},
};

static void log_msg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static void log_msg(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    putchar('\n');
    va_end(ap);

    if (log_file != NULL) {
        va_start(ap, fmt);
        vfprintf(log_file, fmt, ap);
        fputc('\n', log_file);
        va_end(ap);
        fflush(log_file);
    }
}

static int make_path(const char *path)
{
    char tmp[PATH_LEN];
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) == -1 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) == -1 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static int ensure_dir(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        log_msg("Помилка: %s існує і не є папкою", path);
        return -1;
    }

    if (make_path(path) == -1) {
        log_msg("Помилка: не вдалося створити папку %s: %s", path, strerror(errno));
        return -1;
    }

    log_msg("Створено папку: %s", path);
    return 0;
}

static int copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buf[8192];
    size_t n;
    int rc = 0;

    if (in == NULL) {
        return -1;
    }

    out = fopen(dst, "wb");
    if (out == NULL) {
        int saved = errno;
        fclose(in);
        errno = saved;
        return -1;
    }

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            rc = -1;
            break;
        }
    }
    if (ferror(in)) {
        rc = -1;
    }

    fclose(in);
    if (fclose(out) != 0) {
        rc = -1;
    }

    if (rc == -1) {
        int saved = errno;
        unlink(dst);
        errno = saved;
    }
    return rc;
}

static int move_file(const char *src, const char *dst)
{
    if (rename(src, dst) == 0) {
        return 0;
    }
    if (errno != EXDEV) {
        return -1;
    }
    if (copy_file(src, dst) == -1) {
        return -1;
    }
    return unlink(src);
}

static const char *category_for(const char *name)
{
    const char *dot = strrchr(name, '.');

    if (dot == NULL || dot == name) {
        return "Other";
    }

    for (size_t i = 0; i < sizeof(ext_map) / sizeof(ext_map[0]); i++) {
        if (strcasecmp(dot, ext_map[i].ext) == 0) {
            return ext_map[i].category;
        }
    }
    return "Other";
}

static void sort_file(const char *src, const char *name, const char *dest)
{
    char dir[PATH_LEN];
    char dst[PATH_LEN + NAME_MAX + 2];

    snprintf(dir, sizeof(dir), "%s/%s", dest, category_for(name));
    if (ensure_dir(dir) == -1) {
        return;
    }

    snprintf(dst, sizeof(dst), "%s/%s", dir, name);
    if (access(dst, F_OK) == 0) {
        log_msg("Помилка: файл вже існує, пропущено: %s", dst);
        return;
    }

    if (move_file(src, dst) == -1) {
        log_msg("Помилка: не вдалося перемістити %s: %s", src, strerror(errno));
        return;
    }

    log_msg("Переміщено: %s -> %s", src, dst);
}

static void scan_dir(const char *path, const char *dest)
{
    DIR *dir = opendir(path);
    struct dirent *entry;

    if (dir == NULL) {
        log_msg("Помилка: не вдалося відкрити %s: %s", path, strerror(errno));
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char full[PATH_LEN];
        struct stat st;

        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);

        if (stat(full, &st) == -1) {
            log_msg("Помилка: stat %s: %s", full, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            char real[PATH_MAX];

            if (realpath(full, real) != NULL && strcmp(real, dest_real) == 0) {
                continue;
            }
            scan_dir(full, dest);
        } else if (S_ISREG(st.st_mode)) {
            sort_file(full, entry->d_name, dest);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[])
{
    const char *source = NULL;
    const char *dest = NULL;
    struct stat st;
    int opt;

    static const struct option long_options[] = {
        {"source", required_argument, NULL, 's'},
        {"dest",   required_argument, NULL, 'd'},
        {NULL, 0, NULL, 0}
    };

    while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (opt) {
        case 's':
            source = optarg;
            break;
        case 'd':
            dest = optarg;
            break;
        default:
            fprintf(stderr, "Використання: %s --source=<шлях> --dest=<шлях>\n", argv[0]);
            return 1;
        }
    }

    if (source == NULL || dest == NULL) {
        fprintf(stderr, "Використання: %s --source=<шлях> --dest=<шлях>\n", argv[0]);
        return 1;
    }

    if (stat(source, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Помилка: %s не є директорією\n", source);
        return 1;
    }

    log_file = fopen(LOG_FILE, "w");
    if (log_file == NULL) {
        perror(LOG_FILE);
        return 1;
    }

    log_msg("--- Програма сортування запущена ---");

    if (ensure_dir(dest) == -1 || realpath(dest, dest_real) == NULL) {
        fclose(log_file);
        return 1;
    }

    scan_dir(source, dest);

    log_msg("Сортування завершено! Див. '%s'.", LOG_FILE);
    fclose(log_file);

    return 0;
}