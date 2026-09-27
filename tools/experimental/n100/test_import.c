#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libvmaf/picture.h"
#include "picture.h"
#include "ref.h"
#include "mem.h"

/* Deterministic allocation failure inside import, not in setup or teardown. */
static int fail_at, alloc_count;
void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__wrap_malloc(size_t size)
{
    if (fail_at && ++alloc_count == fail_at) return NULL;
    return __real_malloc(size);
}
void *__wrap_calloc(size_t count, size_t size)
{
    if (fail_at && ++alloc_count == fail_at) return NULL;
    return __real_calloc(count, size);
}

typedef struct Fixture {
    VmafPictureImportConfiguration cfg;
    atomic_int releases;
} Fixture;

static void release(void *cookie)
{
    Fixture *f = cookie;
    assert(atomic_fetch_add(&f->releases, 1) == 0);
    for (unsigned i = 0; i < 3; i++) aligned_free(f->cfg.data[i]);
}

static void setup(Fixture *f, unsigned bpc)
{
    memset(f, 0, sizeof(*f));
    atomic_init(&f->releases, 0);
    f->cfg.pix_fmt = VMAF_PIX_FMT_YUV420P;
    f->cfg.bpc = bpc;
    f->cfg.w = 64;
    f->cfg.h = 32;
    f->cfg.cookie = f;
    f->cfg.release = release;
    for (unsigned i = 0; i < 3; i++) {
        f->cfg.stride[i] = ((64 >> !!i) * (bpc > 8 ? 2 : 1)) + 64;
        f->cfg.data_size[i] = f->cfg.stride[i] * (32 >> !!i) + 64;
        f->cfg.data[i] = aligned_malloc(f->cfg.data_size[i], 32);
        assert(f->cfg.data[i]);
        memset(f->cfg.data[i], 0x51 + i, f->cfg.data_size[i]);
    }
}

static void *release_on_worker(void *opaque)
{
    VmafPicture *pic = opaque;
    assert(!vmaf_picture_unref(pic));
    return NULL;
}

static void test_lifetime(unsigned bpc)
{
    Fixture f;
    setup(&f, bpc);
    VmafPicture first = {0}, second = {0}, third = {0};
    assert(!vmaf_picture_import(&first, &f.cfg));
    assert(vmaf_ref_load(first.ref) == 1);
    for (unsigned i = 0; i < 3; i++) {
        assert(first.data[i] == f.cfg.data[i]);
        assert(first.stride[i] == f.cfg.stride[i]);
        assert(first.w[i] == (64 >> !!i));
        assert(first.h[i] == (32 >> !!i));
    }
    assert(!vmaf_picture_ref(&second, &first));
    assert(!vmaf_picture_ref(&third, &first));
    assert(vmaf_ref_load(first.ref) == 3);
    assert(!vmaf_picture_unref(&first));
    assert(!atomic_load(&f.releases));
    assert(!vmaf_picture_unref(&second));
    assert(!atomic_load(&f.releases));
    assert(((unsigned char *)third.data[2])[17] == 0x53);
    pthread_t worker;
    assert(!pthread_create(&worker, NULL, release_on_worker, &third));
    assert(!pthread_join(worker, NULL));
    assert(atomic_load(&f.releases) == 1);
    assert(!third.ref && !third.priv && !third.data[0]);
    assert(vmaf_picture_unref(&third) == -EINVAL);
    assert(atomic_load(&f.releases) == 1);
}

static void expect_failure(Fixture *f, VmafPictureImportConfiguration *cfg)
{
    VmafPicture pic, before;
    memset(&pic, 0x42, sizeof(pic));
    before = pic;
    assert(vmaf_picture_import(&pic, cfg) < 0);
    assert(!memcmp(&pic, &before, sizeof(pic)));
    assert(!atomic_load(&f->releases));
}

static void test_invalid(void)
{
    Fixture f;
    setup(&f, 10);
    VmafPictureImportConfiguration cfg;
    expect_failure(&f, NULL);
    assert(vmaf_picture_import(NULL, &f.cfg) == -EINVAL);
#define BAD(field, value) do { cfg = f.cfg; cfg.field = (value); expect_failure(&f, &cfg); } while (0)
    BAD(pix_fmt, VMAF_PIX_FMT_YUV422P);
    BAD(bpc, 9);
    BAD(bpc, 12);
    BAD(w, 0);
    BAD(w, 63);
    BAD(w, UINT_MAX - 1);
    BAD(h, 31);
    BAD(h, 0);
    BAD(release, NULL);
    BAD(data[0], NULL);
    BAD(data[1], (unsigned char *)f.cfg.data[1] + 1);
    BAD(stride[0], -f.cfg.stride[0]);
    BAD(stride[1], 0);
    BAD(stride[2], f.cfg.stride[2] + 1);
    BAD(stride[0], 32);
    BAD(stride[0], (ptrdiff_t)INT_MAX + 1);
    BAD(data_size[0], f.cfg.data_size[0] - 1);
    BAD(data_size[1], 0);
#undef BAD
    for (int point = 1; point <= 2; point++) {
        fail_at = point;
        alloc_count = 0;
        expect_failure(&f, &f.cfg);
        assert(alloc_count == point);
        fail_at = 0;
    }
    release(&f); /* Caller still owns every failed import. */
}

int main(void)
{
    test_lifetime(8);
    test_lifetime(10);
    test_invalid();
    puts("picture import: lifetime, threaded final release, invalid storage and both allocation failures passed");
    return 0;
}
