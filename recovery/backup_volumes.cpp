#include <fcntl.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr uint64_t kVolumeBytes = 2ULL * 1024 * 1024 * 1024;
constexpr unsigned kVolumeLimit = 10000;
constexpr size_t kBufferBytes = 1024 * 1024;
constexpr size_t kManifestLimit = 1024 * 1024;
constexpr char kManifestHeader[] = "XPERIA_TWRP_VOLUMES_1";
constexpr char kManifest[] = "VOLUMES.tsv";
constexpr char kComplete[] = "BACKUP_COMPLETE";

std::string VolumeName(unsigned index) {
    char name[32];
    snprintf(name, sizeof(name), "archive.tar.%04u", index);
    return name;
}

std::string Hex(const unsigned char* bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(SHA256_DIGEST_LENGTH * 2, '0');
    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        result[i * 2] = digits[bytes[i] >> 4];
        result[i * 2 + 1] = digits[bytes[i] & 15];
    }
    return result;
}

std::string FinishDigest(SHA256_CTX* ctx) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, ctx);
    return Hex(digest);
}

ssize_t Read(int fd, void* buffer, size_t size) {
    ssize_t count;
    do {
        count = read(fd, buffer, size);
    } while (count < 0 && errno == EINTR);
    return count;
}

bool Write(int fd, const void* buffer, size_t size) {
    const auto* bytes = static_cast<const char*>(buffer);
    while (size > 0) {
        const ssize_t count = write(fd, bytes, size);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return false;
        bytes += count;
        size -= count;
    }
    return true;
}

bool Save(int directory, const char* name, const std::string& data) {
    const int fd = openat(directory, name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0)
        return false;
    const bool saved = Write(fd, data.data(), data.size()) && fsync(fd) == 0;
    const bool closed = close(fd) == 0;
    if (!saved || !closed)
        unlinkat(directory, name, 0);
    return saved && closed;
}

// A tar stream ends with at least two zero blocks. Track only the last two
// blocks, so this check does not constrain file sizes or volume boundaries.
struct TarTail {
    std::array<unsigned char, 1024> bytes{};
    uint64_t length = 0;

    void Add(const unsigned char* data, size_t size) {
        if (size >= bytes.size()) {
            memcpy(bytes.data(), data + size - bytes.size(), bytes.size());
        } else {
            memmove(bytes.data(), bytes.data() + size, bytes.size() - size);
            memcpy(bytes.data() + bytes.size() - size, data, size);
        }
        length += size;
    }

    bool Complete() const {
        return length >= bytes.size() && length % 512 == 0 &&
               std::all_of(bytes.begin(), bytes.end(), [](unsigned char byte) { return byte == 0; });
    }
};

bool Stage(int directory, uint64_t limit) {
    std::vector<unsigned char> buffer(kBufferBytes);
    std::string manifest = std::string(kManifestHeader) + "\n";
    TarTail tail;
    for (unsigned index = 0; ; ++index) {
        ssize_t count = Read(STDIN_FILENO, buffer.data(), std::min<uint64_t>(buffer.size(), limit));
        if (count < 0)
            return false;
        if (count == 0)
            return tail.Complete() && Save(directory, kManifest, manifest) && syncfs(directory) == 0;
        if (index == kVolumeLimit)
            return false;

        const auto name = VolumeName(index);
        const int fd = openat(directory, name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0)
            return false;

        SHA256_CTX digest;
        SHA256_Init(&digest);
        uint64_t bytes = 0;
        bool ok = true;
        while (count > 0) {
            if (!Write(fd, buffer.data(), count)) {
                ok = false;
                break;
            }
            SHA256_Update(&digest, buffer.data(), count);
            tail.Add(buffer.data(), count);
            bytes += count;
            if (bytes == limit)
                break;
            count = Read(STDIN_FILENO, buffer.data(),
                         std::min<uint64_t>(buffer.size(), limit - bytes));
        }
        ok = ok && count >= 0 && fsync(fd) == 0;
        if (close(fd) != 0 || !ok)
            return false;
        manifest += FinishDigest(&digest) + "\t" + std::to_string(bytes) + "\t" + name + "\n";
        if (count == 0)
            return tail.Complete() && Save(directory, kManifest, manifest) && syncfs(directory) == 0;
    }
}

bool ParseSize(std::string_view text, uint64_t* bytes) {
    const auto end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, *bytes);
    return result.ec == std::errc{} && result.ptr == end && *bytes > 0 && *bytes <= kVolumeBytes;
}

bool VerifyVolume(int directory, const std::string& name, uint64_t bytes, std::string_view expected,
                  std::vector<unsigned char>* buffer, TarTail* tail) {
    const int fd = openat(directory, name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return false;
    struct stat info {};
    bool ok = fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
              info.st_size >= 0 && static_cast<uint64_t>(info.st_size) == bytes;
    SHA256_CTX digest;
    SHA256_Init(&digest);
    while (ok && bytes > 0) {
        const auto count = Read(fd, buffer->data(), std::min<uint64_t>(buffer->size(), bytes));
        if (count <= 0) {
            ok = false;
            break;
        }
        SHA256_Update(&digest, buffer->data(), count);
        tail->Add(buffer->data(), count);
        bytes -= count;
    }
    unsigned char extra;
    ok = ok && Read(fd, &extra, sizeof(extra)) == 0 && FinishDigest(&digest) == expected;
    return close(fd) == 0 && ok;
}

bool Commit(int directory) {
    const int fd = openat(directory, kManifest, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return false;
    struct stat info {};
    std::vector<unsigned char> buffer(kBufferBytes);
    std::string manifest;
    const bool regular = fstat(fd, &info) == 0 && S_ISREG(info.st_mode);
    ssize_t count = -1;
    while (regular && (count = Read(fd, buffer.data(), buffer.size())) > 0) {
        if (manifest.size() + count > kManifestLimit) {
            count = -1;
            break;
        }
        manifest.append(reinterpret_cast<const char*>(buffer.data()), count);
    }
    if (close(fd) != 0 || count != 0 || manifest.empty() || manifest.back() != '\n')
        return false;

    std::istringstream lines(manifest);
    std::string line;
    if (!std::getline(lines, line) || line != kManifestHeader)
        return false;
    unsigned index = 0;
    TarTail tail;
    while (std::getline(lines, line)) {
        const auto first = line.find('\t');
        const auto second = first == std::string::npos ? first : line.find('\t', first + 1);
        if (first != SHA256_DIGEST_LENGTH * 2 || second == std::string::npos ||
            index >= kVolumeLimit)
            return false;
        const auto name = VolumeName(index++);
        uint64_t bytes;
        if (line.substr(second + 1) != name ||
            !ParseSize(std::string_view(line).substr(first + 1, second - first - 1), &bytes) ||
            !VerifyVolume(directory, name, bytes, std::string_view(line).substr(0, first),
                          &buffer, &tail))
            return false;
    }
    if (!tail.Complete() || syncfs(directory) != 0)
        return false;

    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(manifest.data()), manifest.size(), digest);
    if (!Save(directory, kComplete, Hex(digest) + "\n"))
        return false;
    if (syncfs(directory) == 0)
        return true;
    unlinkat(directory, kComplete, 0);
    return false;
}
}  // namespace

int main(int argc, char** argv) {
    const bool stage = argc >= 2 && strcmp(argv[1], "stage") == 0;
    const bool commit = argc >= 2 && strcmp(argv[1], "commit") == 0;
    uint64_t limit = kVolumeBytes;
    if ((!stage && !commit) || argc < 4 || argc > 5 ||
        (argc == 5 && (!stage || !ParseSize(argv[4], &limit)))) {
        fprintf(stderr, "Usage: %s stage PARENT NEW_NAME [VOLUME_BYTES] < ARCHIVE\n"
                        "       %s commit PARENT NAME\n", argv[0], argv[0]);
        return 2;
    }
    const std::string_view name(argv[3]);
    if (name.empty() || name.size() > 80 || name == "." || name == ".." ||
        name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-")
            != std::string_view::npos) {
        fprintf(stderr, "Invalid backup directory name\n");
        return 2;
    }
    const int parent = open(argv[2], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (parent < 0) {
        perror("Opening backup destination");
        return 1;
    }
    if (stage && mkdirat(parent, argv[3], 0700) != 0) {
        perror("Creating new backup directory");
        close(parent);
        return 1;
    }
    const int directory = openat(parent, argv[3], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    close(parent);
    if (directory < 0) {
        perror("Opening backup directory");
        return 1;
    }
    const bool ok = stage ? Stage(directory, limit) : Commit(directory);
    close(directory);
    if (!ok) {
        fprintf(stderr, "Backup %s failed\n", stage ? "staging" : "verification");
        return 1;
    }
    puts(stage ? "VOLUMES_STAGED" : "BACKUP_COMMITTED");
}
