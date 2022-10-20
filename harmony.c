#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define VERBOSE
#define WORD_LIST_SIZE 15000
#define INTERMEDIATE_BITMAP_SIZE_LIMIT 50000000
#define DICT_INITIAL_SIZE_LIMIT 10000000

typedef struct {
    uint32_t bitmap;
    uint16_t from_ids[5];
} Entry;

typedef struct {
    size_t length;
    size_t size;
    Entry entry[];
} Dict;

char words[WORD_LIST_SIZE][6];
size_t words_size;

uint8_t popcnt(uint32_t n) {
	n -= (n>>1) & 0x55555555;
	n  = (n & 0x33333333) + ((n>>2) & 0x33333333);
	n  = ((n>>4) + n) & 0x0F0F0F0F;
	n += n>>8;
	n += n>>16;
	return (uint8_t) n & 0x3F;
}

// TODO: this is too slow. Rewrite as DFA (and possibly do OMP).
void load_words(const char *filename){
    clock_t start = clock();

    FILE *fp = fopen(filename, "r");
    char s[101];

    while(fgets(s, 100, fp)){

        if(strlen(s) != 6) continue; // '\n' included.
#define F(x) (1 << (s[x] - 'a'))
        uint32_t bitmap = F(0) | F(1) | F(2) | F(3) | F(4);
#undef F
        if(popcnt(bitmap) != 5) continue;

        memcpy(words[words_size++], s, 5);
    }

    qsort(words, words_size, 6, (int (*)(const void *, const void *))strcmp);

    clock_t diff = clock() - start;
    unsigned long long nsec = diff * 1000 * 1000 * 1000 / CLOCKS_PER_SEC;

    printf("Loaded %ld words in %llu us.\n", words_size, nsec / 1000);

}

Dict **make_solo(){
    clock_t start = clock();

    Dict **ret = malloc(26 * sizeof(Dict *));
    for(int i = 0; i < 26; i++) {
        ret[i] = (Dict *)malloc(sizeof(Dict) + WORD_LIST_SIZE * sizeof(Entry));
        ret[i]->length = 1;
        ret[i]->size = 0;
    }

    for(size_t i = 0; i < words_size; i++) {
#define F(x) (1 << (words[i][x] - 'a'))

        uint8_t initial = words[i][0] - 'a';
        Dict *d = ret[initial];
        Entry *e = &d->entry[d->size];
        e->bitmap = F(0) | F(1) | F(2) | F(3) | F(4);
        e->from_ids[0] = i;
        d->size++;

#undef F
    }

    clock_t diff = clock() - start;
    unsigned long long nsec = diff * 1000 * 1000 * 1000 / CLOCKS_PER_SEC;

    printf("Generated solo in %llu us.\n", nsec / 1000);

    return ret;
}

Dict *make_harmony_initial(const Dict *const di1, const Dict *const di2, int print, Dict *ret) {
    clock_t start = clock();

    if(ret){
        assert(ret->length == di1->length + di2->length);
    }else{
        ret = (Dict *)malloc(sizeof(Dict) + DICT_INITIAL_SIZE_LIMIT * sizeof(Entry));
        ret->size = 0;
        ret->length = di1->length + di2->length;
    }

    if(di1->size == 0) return ret;

    for(size_t i = 0; i < di1->size; i++)
        for(size_t j = 0; j < di2->size; j++) {
            const Entry *e1 = &di1->entry[i], *e2 = &di2->entry[j];
            uint32_t combined = e1->bitmap | e2->bitmap;
            if((e1->bitmap & e2->bitmap) == 0
                && e1->from_ids[di1->length - 1] < e2->from_ids[0]
            ){
                Entry *eret = &ret->entry[ret->size++];

                eret->bitmap = combined;
                for(size_t ii = 0; ii < di1->length; ii++)
                    eret->from_ids[ii] = e1->from_ids[ii];
                for(size_t ii = 0; ii < di2->length; ii++)
                    eret->from_ids[ii + di1->length] = e2->from_ids[ii];

                if(print) {
                    for(size_t ii = 0; ii < ret->length; ii++)
                        printf("%s ", words[eret->from_ids[ii]]);
                    printf(" ");
                    if(ret->size % 6 == 0) puts("");
                }

            }
        }
    assert(ret->size < DICT_INITIAL_SIZE_LIMIT);

#ifdef VERBOSE
    clock_t diff = clock() - start;
    size_t n_src = di1->size * di2->size;

    unsigned long long nsec = diff * 1000 * 1000 * 1000 / CLOCKS_PER_SEC;
    printf("|%9ld| x |%9ld| = |%9ld| (%.2f%%), %12llu us, %llu ns/src, %llu ns/dst.\n",
        di1->size, di2->size, ret->size,
        100. * ret->size / n_src,
        nsec / 1000, 
        nsec / n_src,
        nsec / ret->size
    );
#else
    (void)start;
#endif

    return ret;
}

Dict **make_harmony(const Dict *const *const d1, const Dict *const *const d2) {
    assert(d2[0]->length == 1);
    size_t ret_length = d1[0]->length + d2[0]->length;

    Dict *tmp[26][26];

    // TODO: Determine start and end point:
    // TODO: details here.
#pragma omp parallel for schedule(dynamic,1) collapse(2)
    for(int i1 = 0; i1 < 26; i1++)
        for(int i2 = 0; i2 < 26; i2++)
            if(i1 < i2)
                tmp[i1][i2] = make_harmony_initial(d1[i1], d2[i2], 0, NULL);

    Dict **ret = malloc(26 * sizeof(Dict *));
    for(int i = 0; i < 26; i++) {
        ret[i] = (Dict *)malloc(sizeof(Dict) + INTERMEDIATE_BITMAP_SIZE_LIMIT * sizeof(Entry));
        ret[i]->length = ret_length;
        ret[i]->size = 0;
    }

    for(int i1 = 0; i1 < 26; i1++)
//#pragma omp parallel for schedule(dynamic)
        for(int i2 = 0; i2 < 26; i2++)
            if(i1 < i2) {
                memcpy(ret[i2]->entry + ret[i2]->size, tmp[i1][i2]->entry, tmp[i1][i2]->size * sizeof(Entry));
                ret[i2]->size += tmp[i1][i2]->size;
                free(tmp[i1][i2]);
            }

    return ret;
}

int main(int argc, char *argv[]){
    if(argc == 1) return -1;
    load_words(argv[1]);
    Dict **solo = make_solo();

#ifdef VERBOSE 
#define SHOW_DICT(x) { \
        size_t __total = 0; \
        for(int __i = 0; __i < 26; __i++){ \
            printf("%-6s[%c] length = %lu, size = %ld\n", #x, (char)(__i + 'a'), x[__i]->length, x[__i]->size); \
            __total += x[__i]->size; \
        } \
        printf("Total: %lu\n", __total); \
        fflush(stdout); \
    }
#else
#define SHOW_DICT(x)
#endif

    SHOW_DICT(solo);

    Dict **duo    = make_harmony(solo,   solo); SHOW_DICT(duo);

    Dict **trio   = make_harmony(duo,    solo); SHOW_DICT(trio);

    Dict **quadro = make_harmony(trio,   solo); SHOW_DICT(quadro);

    Dict **pento  = make_harmony(quadro, solo); SHOW_DICT(pento);
}
