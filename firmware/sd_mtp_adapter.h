#pragma once
// Adapte le SdFs existant (sd_recorder.h) à l'interface FS de Teensyduino pour MTP.
// N'initialise pas le contrôleur SDIO — réutilise la session FIFO_SDIO déjà ouverte.
#include <SdFat.h>
#include <FS.h>

// ----------------------------------------------------------------
// Implémentation FileImpl wrappant FsFile
// Calquée sur SDFile (SD.h) sans la restriction friend/private
// ----------------------------------------------------------------
class SdFsFileImpl : public FileImpl {
public:
    SdFsFileImpl(FsFile f) : _file(f), _name_cache(nullptr) {}
    virtual ~SdFsFileImpl() { close(); }

    size_t   read(void *buf, size_t n) override   { return _file.read(buf, n); }
    size_t   write(const void *buf, size_t n) override { return _file.write(buf, n); }
    int      available() override                  { return _file.available(); }
    int      peek()      override                  { return _file.peek(); }
    void     flush()     override                  { _file.flush(); }
    bool     truncate(uint64_t sz=0) override      { return _file.truncate(sz); }
    uint64_t position()  override                  { return _file.curPosition(); }
    uint64_t size()      override                  { return _file.size(); }
    bool     isOpen()    override                  { return _file.isOpen(); }
    bool     isDirectory() override                { return _file.isDirectory(); }

    bool seek(uint64_t pos, int mode) override {
        if (mode == SeekSet) return _file.seekSet(pos);
        if (mode == SeekCur) return _file.seekCur(pos);
        if (mode == SeekEnd) return _file.seekEnd(pos);
        return false;
    }

    const char* name() override {
        if (!_name_cache) {
            _name_cache = (char*)malloc(256);
            if (_name_cache) _file.getName(_name_cache, 256);
            else             { static char z = 0; return &z; }
        }
        return _name_cache;
    }

    File openNextFile(uint8_t mode=0) override {
        FsFile next = _file.openNextFile();
        if (next) return File(new SdFsFileImpl(next));
        return File();
    }

    void rewindDirectory() override { _file.rewindDirectory(); }

    void close() override {
        if (_name_cache) { free(_name_cache); _name_cache = nullptr; }
        if (_file.isOpen()) _file.close();
    }

private:
    FsFile _file;
    char*  _name_cache;
};


// ----------------------------------------------------------------
// Implémentation FS wrappant SdFs
// ----------------------------------------------------------------
class SdFsAdapter : public FS {
public:
    SdFsAdapter() : _sd(nullptr) {}

    // À appeler après sd.begin() — ne réinitialise rien
    void init(SdFs& sd) { _sd = &sd; }

    File open(const char* path, uint8_t mode = FILE_READ) override {
        if (!_sd) return File();
        oflag_t flags = O_RDONLY;
        if (mode == FILE_WRITE)       flags = O_RDWR | O_CREAT | O_AT_END;
        if (mode == FILE_WRITE_BEGIN) flags = O_RDWR | O_CREAT;
        FsFile f = _sd->open(path, flags);
        if (f) return File(new SdFsFileImpl(f));
        return File();
    }

    bool exists(const char* path) override           { return _sd && _sd->exists(path); }
    bool mkdir(const char* path) override            { return _sd && _sd->mkdir(path); }
    bool rename(const char* a, const char* b) override { return _sd && _sd->rename(a, b); }
    bool remove(const char* path) override           { return _sd && _sd->remove(path); }
    bool rmdir(const char* path) override            { return _sd && _sd->rmdir(path); }

    uint64_t usedSize() override {
        if (!_sd) return 0;
        return (uint64_t)(_sd->clusterCount() - _sd->freeClusterCount())
               * (uint64_t)_sd->bytesPerCluster();
    }
    uint64_t totalSize() override {
        if (!_sd) return 0;
        return (uint64_t)_sd->clusterCount() * (uint64_t)_sd->bytesPerCluster();
    }

private:
    SdFs* _sd;
};
