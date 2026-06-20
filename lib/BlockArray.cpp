#include <QtDebug>
#include <cstdio>
#include <cstring>
#include <vector>
#include "BlockArray.h"

#if USE_POSIX_MMAP
#  include <sys/mman.h>
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#else
#  include <windows.h>
#  include <io.h>
#  define dup _dup
#  define close _close
#  define lseek _lseek
#  define ftruncate _chsize
#  define write _write
#endif

using namespace Konsole;

static int blocksize = 0;

BlockArray::BlockArray()
        : size(0),
        current(size_t(-1)),
        index(size_t(-1)),
        lastmap(nullptr),
        lastmap_index(size_t(-1)),
        lastblock(nullptr), ion(-1),
        length(0)
{
    if (blocksize == 0) {
        int page = getpagesize();
        blocksize = ((sizeof(Block) / page) + 1) * page;
    }
}

BlockArray::~BlockArray()
{
    setHistorySize(0);
    Q_ASSERT(!lastblock);
}

size_t BlockArray::append(Block * block)
{
    if (!size) {
        return size_t(-1);
    }

    ++current;
    if (current >= size) {
        current = 0;
    }

    int rc;
#if USE_POSIX_MMAP
    rc = lseek(ion, current * blocksize, SEEK_SET);
    if (rc < 0) {
        perror("HistoryBuffer::add.seek");
        setHistorySize(0);
        return size_t(-1);
    }
    rc = write(ion, block, blocksize);
#else
    // Windows _lseek / _write
    rc = lseek(ion, long(current * blocksize), SEEK_SET);
    if (rc < 0) {
        perror("HistoryBuffer::add.seek");
        setHistorySize(0);
        return size_t(-1);
    }
    rc = write(ion, block, blocksize);
#endif
    if (rc < 0) {
        perror("HistoryBuffer::add.write");
        setHistorySize(0);
        return size_t(-1);
    }

    length++;
    if (length > size) {
        length = size;
    }

    ++index;

    delete block;
    return current;
}

size_t BlockArray::newBlock()
{
    if (!size) {
        return size_t(-1);
    }
    append(lastblock);

    lastblock = new Block();
    return index + 1;
}

Block * BlockArray::lastBlock() const
{
    return lastblock;
}

bool BlockArray::has(size_t i) const
{
    if (i == index + 1) {
        return true;
    }

    if (i > index) {
        return false;
    }
    if (index - i >= length) {
        return false;
    }
    return true;
}

const Block * BlockArray::at(size_t i)
{
    if (i == index + 1) {
        return lastblock;
    }

    if (i == lastmap_index) {
        return lastmap;
    }

    if (i > index) {
        qDebug() << "BlockArray::at() i > index\n";
        return nullptr;
    }

    size_t j = i;
    Q_ASSERT(j < size);
    unmap();

#if USE_POSIX_MMAP
    Block * block = (Block *)mmap(nullptr, blocksize, PROT_READ, MAP_PRIVATE, ion, j * blocksize);
    if (block == (Block *)-1) {
        perror("mmap");
        return nullptr;
    }
#else
    // Windows 无mmap，堆内存读取代替
    Block* block = new Block();
    lseek(ion, long(j * blocksize), SEEK_SET);
    _read(ion, block, blocksize);
#endif

    lastmap = block;
    lastmap_index = i;
    return block;
}

void BlockArray::unmap()
{
    if (lastmap) {
#if USE_POSIX_MMAP
        int res = munmap((char *)lastmap, blocksize);
        if (res < 0) {
            perror("munmap");
        }
#else
        // Windows 堆内存释放
        delete lastmap;
#endif
    }
    lastmap = nullptr;
    lastmap_index = size_t(-1);
}

bool BlockArray::setSize(size_t newsize)
{
    return setHistorySize(newsize * 1024 / blocksize);
}

bool BlockArray::setHistorySize(size_t newsize)
{
    if (size == newsize) {
        return false;
    }

    unmap();

    if (!newsize) {
        delete lastblock;
        lastblock = nullptr;
        if (ion >= 0) {
            close(ion);
        }
        ion = -1;
        current = size_t(-1);
        return true;
    }

    if (!size) {
        FILE * tmp = tmpfile();
        if (!tmp) {
            perror("konsole: cannot open temp file.\n");
        } else {
            ion = dup(fileno(tmp));
            if (ion<0) {
                perror("konsole: cannot dup temp file.\n");
                fclose(tmp);
            }
        }
        if (ion < 0) {
            return false;
        }

        Q_ASSERT(!lastblock);
        lastblock = new Block();
        size = newsize;
        return false;
    }

    if (newsize > size) {
        increaseBuffer();
        size = newsize;
        return false;
    } else {
        decreaseBuffer(newsize);
#if USE_POSIX_MMAP
        int res = ftruncate(ion, length*blocksize);
#else
        int res = ftruncate(ion, long(length*blocksize));
#endif
        Q_UNUSED (res);
        size = newsize;
        return true;
    }
}

static void moveBlock(FILE * fion, int cursor, int newpos, char * buffer2)
{
    int res = fseek(fion, cursor * blocksize, SEEK_SET);
    if (res) {
        perror("fseek");
    }
    res = fread(buffer2, blocksize, 1, fion);
    if (res != 1) {
        perror("fread");
    }

    res = fseek(fion, newpos * blocksize, SEEK_SET);
    if (res) {
        perror("fseek");
    }
    res = fwrite(buffer2, blocksize, 1, fion);
    if (res != 1) {
        perror("fwrite");
    }
}

void BlockArray::decreaseBuffer(size_t newsize)
{
    char *buffer1 = nullptr;
    if (index < newsize) {
        return;
    }

    int offset = (current - (newsize - 1) + size) % size;
    if (!offset) {
        return;
    }

    FILE * fion = fdopen(dup(ion), "w+b");
    if (!fion) {
        perror("fdopen/dup");
        return;
    }

    int firstblock;
    if (current <= newsize) {
        firstblock = current + 1;
    } else {
        firstblock = 0;
    }

    buffer1 = new char[blocksize];
    size_t oldpos;
    for (size_t i = 0, cursor=firstblock; i < newsize; i++) {
        oldpos = (size + cursor + offset) % size;
        moveBlock(fion, int(oldpos), cursor, buffer1);
        if (oldpos < newsize) {
            cursor = int(oldpos);
        } else {
            cursor++;
        }
    }

    current = newsize - 1;
    length = newsize;

    delete [] buffer1;
    fclose(fion);
}

void BlockArray::increaseBuffer()
{
    char *buffer1 = nullptr;
    char *buffer2 = nullptr;
    if (index < size) {
        return;
    }

    int offset = (current + size + 1) % size;
    if (!offset) {
        return;
    }

    int runs = 1;
    int bpr = size;

    if (size % offset == 0) {
        bpr = size / offset;
        runs = offset;
    }

    FILE * fion = fdopen(dup(ion), "w+b");
    if (!fion) {
        perror("fdopen/dup");
        return;
    }

    buffer1 = new char[blocksize];
    buffer2 = new char[blocksize];

    int res;
    for (int i = 0; i < runs; i++) {
        int firstblock = (offset + i) % size;
        res = fseek(fion, firstblock * blocksize, SEEK_SET);
        if (res) perror("fseek");
        res = fread(buffer1, blocksize, 1, fion);
        if (res != 1) perror("fread");

        int newpos = 0;
        for (int j = 1, cursor=firstblock; j < bpr; j++) {
            cursor = (cursor + offset) % size;
            newpos = (cursor - offset + size) % size;
            moveBlock(fion, cursor, newpos, buffer2);
        }
        res = fseek(fion, i * blocksize, SEEK_SET);
        if (res) perror("fseek");
        res = fwrite(buffer1, blocksize, 1, fion);
        if (res != 1) perror("fwrite");
    }
    current = size - 1;
    length = size;

    delete [] buffer1;
    delete [] buffer2;
    fclose(fion);
}