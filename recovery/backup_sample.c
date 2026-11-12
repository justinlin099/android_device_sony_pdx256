#include <errno.h>
#include <fcntl.h>
#include <libtar.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const uint8_t fixture[] = {0x00, 0x41, 0x7f, 0x80, 0xfe, 0xff, '\n'};

static bool write_fixture(const char* path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0640);
    if (fd < 0) return false;
    bool ok = fchmod(fd, 0640) == 0 &&
              TEMP_FAILURE_RETRY(write(fd, fixture, sizeof(fixture))) == (ssize_t)sizeof(fixture);
    return close(fd) == 0 && ok;
}

static bool check_fixture(const char* path) {
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat info;
    uint8_t actual[sizeof(fixture)];
    bool ok = fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
              (info.st_mode & 0777) == 0640 && info.st_size == (off_t)sizeof(fixture) &&
              TEMP_FAILURE_RETRY(read(fd, actual, sizeof(actual))) == (ssize_t)sizeof(actual) &&
              memcmp(actual, fixture, sizeof(fixture)) == 0;
    return close(fd) == 0 && ok;
}

static bool archive_fixture(const char* archive, char* source) {
    TAR* tar;
    if (tar_open(&tar, archive, NULL, O_WRONLY | O_CREAT | O_EXCL, 0600, TAR_GNU) < 0)
        return false;
    char name[] = "fixture";
    bool ok = tar_append_tree(tar, source, name) == 0 && tar_append_eof(tar) == 0;
    return tar_close(tar) == 0 && ok;
}

static bool restore_fixture(const char* archive, char* destination) {
    TAR* tar;
    if (tar_open(&tar, archive, NULL, O_RDONLY, 0, TAR_GNU) < 0) return false;
    const int no_progress_fd = 0;
    bool ok = tar_extract_all(tar, destination, &no_progress_fd) == 0;
    return tar_close(tar) == 0 && ok;
}

int main(void) {
    int log = open("/tmp/recovery.log", O_WRONLY | O_APPEND | O_NOFOLLOW | O_CLOEXEC);
    if (log < 0) return 1;
    char root[] = "/tmp/xperia-backup-sample.XXXXXX";
    if (!mkdtemp(root)) {
        dprintf(log, "Sony backup sample: temporary directory failed\n");
        close(log);
        return 1;
    }

    char source[96], destination[96], archive[96], input[96], output[96], restored_dir[96];
    snprintf(source, sizeof(source), "%s/source", root);
    snprintf(destination, sizeof(destination), "%s/restore", root);
    snprintf(archive, sizeof(archive), "%s/fixture.tar", root);
    snprintf(input, sizeof(input), "%s/source/binary", root);
    snprintf(output, sizeof(output), "%s/restore/fixture/binary", root);
    snprintf(restored_dir, sizeof(restored_dir), "%s/restore/fixture", root);

    bool backed_up = mkdir(source, 0700) == 0 && mkdir(destination, 0700) == 0 &&
                     write_fixture(input) && archive_fixture(archive, source);
    bool restored = backed_up && restore_fixture(archive, destination) && check_fixture(output);
    unlink(output);
    rmdir(restored_dir);
    unlink(input);
    unlink(archive);
    rmdir(source);
    rmdir(destination);
    bool cleaned = rmdir(root) == 0;

    dprintf(log, "Sony backup sample: archived=%d restored_bytes_and_mode=%d cleaned=%d\n",
            backed_up, restored, cleaned);
    close(log);
    return backed_up && restored && cleaned ? 0 : 1;
}
