#pragma once
// Host shim of the Arduino SD/FS API. Two jobs:
//   1. make ProgressStore/CourseCatalog headers compile on the host (their
//      SD calls are never executed by the preview);
//   2. actually READ files so the preview loads real lesson JSON from the
//      repo. Device paths ("/courses/...") map onto paths relative to the
//      working directory (run the preview from the repo root).
//
// Portable between MSVC and MinGW: uses std::filesystem only.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#define FILE_READ "r"
#define FILE_WRITE "w"

namespace fs = std::filesystem;

// BSD strlcpy — present in ESP32 newlib, missing from MinGW/MSVC headers.
// Only compiled into the preview host build.
#if !defined(__linux__)
inline size_t strlcpy(char* dst, const char* src, size_t size) {
  const size_t len = strlen(src);
  if (size) {
    const size_t n = (len < size - 1) ? len : size - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
  }
  return len;
}
#endif

class File {
 public:
  File() {}

  explicit operator bool() const { return impl_ != nullptr; }

  size_t size() const { return impl_ ? impl_->size : 0; }

  size_t readBytes(char* buf, size_t n) {
    if (!impl_ || !impl_->f) return 0;
    return fread(buf, 1, n, impl_->f);
  }

  // Write side: only needed so ProgressStore::write compiles; the preview
  // never saves. Routed to fwrite when a file happens to be open.
  size_t write(uint8_t c) {
    return (impl_ && impl_->f) ? fwrite(&c, 1, 1, impl_->f) : 0;
  }
  size_t write(const uint8_t* buf, size_t n) {
    return (impl_ && impl_->f) ? fwrite(buf, 1, n, impl_->f) : 0;
  }
  void flush() {}

  void close() {
    if (impl_ && impl_->f) {
      fclose(impl_->f);
      impl_->f = nullptr;
    }
  }

  bool isDirectory() const { return impl_ && impl_->isDir; }

  // Base name without the path (matches the device SD File::name()).
  const char* name() const { return impl_ ? impl_->base.c_str() : ""; }

  File openNextFile() {
    if (!impl_ || !impl_->isDir || impl_->idx >= impl_->entries.size()) {
      return File();
    }
    return File::open(impl_->path / impl_->entries[impl_->idx++]);
  }

  static File create(const fs::path& path) {
    auto impl=std::make_shared<Impl>();impl->path=path;
    impl->f=fopen(path.string().c_str(),"wb");if(!impl->f)return File();
    File out;out.impl_=std::move(impl);return out;
  }
  static File open(const fs::path& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) return File();
    auto impl = std::make_shared<Impl>();
    impl->path = path;
    impl->base = path.filename().string();
    impl->isDir = fs::is_directory(path, ec);
    impl->size = impl->isDir ? 0 : (size_t)fs::file_size(path, ec);

    if (impl->isDir) {
      for (const auto& e : fs::directory_iterator(path, ec)) {
        impl->entries.push_back(e.path().filename());
      }
      // Device listing order is unspecified; sort for reproducible previews.
    } else {
      impl->f = fopen(path.string().c_str(), "rb");
      if (!impl->valid()) return File();
    }
    File out;
    out.impl_ = std::move(impl);
    return out;
  }

 private:
  struct Impl {
    std::FILE* f = nullptr;
    size_t size = 0;
    bool isDir = false;
    fs::path path;
    std::string base;
    std::vector<fs::path> entries;
    size_t idx = 0;
    bool valid() const { return isDir || f != nullptr; }
  };
  std::shared_ptr<Impl> impl_;
};

class SDClass {
 public:
  static inline bool allowWrites=false;
  static inline std::string writeRoot="out/test-sd";
  // Device paths map onto the repo: "/courses/<rest>" holds the course
  // tree on SD, which in this repository lives at "data/sd/<rest>".
  static fs::path mapPath(const char* path) {
    std::string p = path ? path : "";
    if(p=="/lingoink" || p.rfind("/lingoink/",0)==0) return fs::path(writeRoot)/p.substr(1);
    if (p.rfind("/courses/", 0) == 0) {
      return fs::path("data/sd") / p.substr(strlen("/courses/"));
    }
    if (p == "/courses") return fs::path("data/sd");
    if (!p.empty() && p[0] == '/') return fs::path(p.substr(1));
    return fs::path(p);
  }

  bool exists(const char* path) {
    std::error_code ec;
    return fs::exists(mapPath(path), ec);
  }

  bool mkdir(const char* p) {if(!allowWrites)return false; std::error_code ec;fs::create_directories(mapPath(p),ec);return !ec;}

  File open(const char* path, const char* mode = FILE_READ) {
    fs::path p = mapPath(path);
    if (std::string(mode) == FILE_READ) {
      return File::open(p);  // read-only: real file support for previews
    }
    return allowWrites?File::create(p):File();
  }

  bool remove(const char* p) {if(!allowWrites)return false;std::error_code ec;return fs::remove(mapPath(p),ec);}
  bool rename(const char* a,const char* b) {if(!allowWrites)return false;std::error_code ec;fs::rename(mapPath(a),mapPath(b),ec);return !ec;}
};

inline SDClass SD;
