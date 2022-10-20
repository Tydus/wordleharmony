#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define WORD_LIST_SIZE 15000
#define INTERMEDIATE_BITMAP_SIZE_LIMIT 2000000000

typedef struct {
    uint32_t bitmap;
    uint16_t from_ids[5];
} Entry;

typedef struct {
    uint8_t length;
    size_t size;
    Entry entry[];
} Dict;

char words[WORD_LIST_SIZE][6];
size_t words_size;

char words_initial[26][WORD_LIST_SIZE][6];

uint8_t popcnt(uint32_t n) {
	n -= (n>>1) & 0x55555555;
	n  = (n & 0x33333333) + ((n>>2) & 0x33333333);
	n  = ((n>>4) + n) & 0x0F0F0F0F;
	n += n>>8;
	n += n>>16;
	return (uint8_t) n & 0x3F;
}

static int cmpstringp(const void *p1, const void *p2) {
   return strcmp(p1, p2);
}

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

    qsort(words, words_size, 6, cmpstringp);

    clock_t diff = clock() - start;
    unsigned long long nsec = diff * 1000 * 1000 * 1000 / CLOCKS_PER_SEC;

    printf("Loaded %ld words in %llu us.\n", words_size, nsec / 1000);

}

Dict *make_root(){
    clock_t start = clock();

    Dict *d1 = (Dict *)malloc(sizeof(Dict) + WORD_LIST_SIZE * sizeof(Entry));
    d1->length = 1;
    d1->size = 0;

    for(size_t i = 0; i < words_size; i++) {
#define F(x) (1 << (words[i][x] - 'a'))
        uint32_t bitmap = F(0) | F(1) | F(2) | F(3) | F(4);
#undef F

        Entry *e = &d1->entry[d1->size];
        e->bitmap = bitmap;
        e->from_ids[0] = d1->size;
        d1->size++;
    }

    clock_t diff = clock() - start;
    unsigned long long nsec = diff * 1000 * 1000 * 1000 / CLOCKS_PER_SEC;

    printf("Generate d1 in %llu us.\n", nsec / 1000);

    return d1;
}

Dict *make_harmony(const Dict *d1, const Dict *d2, int print) {
    clock_t start = clock();

    Dict *ret = (Dict *)malloc(sizeof(Dict) + INTERMEDIATE_BITMAP_SIZE_LIMIT * sizeof(Entry));
    ret->size = 0;
    //assert(d1->length >= d2->length);
    ret->length = d1->length + d2->length;

    for(size_t i = 0; i < d1->size; i++)
        //for(size_t j = (d1 == d2 ? i + 1 : 0); j < d2->size; j++) {
        for(size_t j = 0; j < d2->size; j++) {
            const Entry *e1 = &d1->entry[i], *e2 = &d2->entry[j];
            uint32_t combined = e1->bitmap | e2->bitmap;
            if((e1->bitmap & e2->bitmap) == 0
                && e1->from_ids[d1->length - 1] < e2->from_ids[0]
            ){
                Entry *eret = &ret->entry[ret->size++];

                eret->bitmap = combined;
                for(size_t ii = 0; ii < d1->length; ii++)
                    eret->from_ids[ii] = e1->from_ids[ii];
                for(size_t ii = 0; ii < d2->length; ii++)
                    eret->from_ids[ii + d1->length] = e2->from_ids[ii];

                if(print) {
                    //printf("%p | %p = %p:", e1->bitmap, e2->bitmap, eret->bitmap);
                    for(size_t ii = 0; ii < ret->length; ii++)
                        printf("%s ", words[eret->from_ids[ii]]);
                    printf(" ");
                    if(ret->size % 6 == 0) puts("");
                }

            }
        }

    clock_t diff = clock() - start;
    size_t n_src = d1->size * d2->size;

    unsigned long long nsec = diff * 1000 * 1000 * 1000 / CLOCKS_PER_SEC;
    printf("|%9ld| x |%9ld| = |%9ld| (%.2f%%), %12llu us, %llu ns/src, %llu ns/dst.\n",
        d1->size, d2->size, ret->size,
        100. * ret->size / n_src,
        nsec / 1000, 
        nsec / n_src,
        nsec / ret->size
    );

    return ret;
}

int main(int argc, char *argv[]){
    if(argc == 1) return -1;
    load_words(argv[1]);
    Dict *d1 = make_root();

#define SHOW_DICT(x) printf("%s length = %d, size = %ld\n", #x, x->length, x->size); fflush(stdout);

    SHOW_DICT(d1);
    Dict *d2 = make_harmony(d1, d1, 0); SHOW_DICT(d2);

    Dict *d3 = make_harmony(d2, d1, 0); SHOW_DICT(d3);

    Dict *d4 = make_harmony(d3, d1, 0); SHOW_DICT(d4);

    Dict *d5 = make_harmony(d4, d1, 1); SHOW_DICT(d5);
}
