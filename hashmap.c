#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "hashmap.h"

#define HASH_MAP_INIT_SIZE      16  //ハッシュテーブルの初期サイズ
#define HASH_MAP_MAX_CAPACITY   200 //使用率をこれ以下に抑える
#define HASH_MAP_GROW_FACTOR    2   //リハッシュ時に何倍にするか

static uint32_t fnv1_hash(const char *str);
static uint32_t fnv1a_hash(const char *str);
static uint32_t calc_hash(const char *str);
static void rehash(HASH_MAP_t *hash_map);
static int match(const char *key1, const char *key2);
static void insert_entry(HASH_MAP_t *hash_map, HASH_ENTRY_t *entry);
static void set_entry(HASH_ENTRY_t *entry, const char *key, void *data);

//ハッシュマップを作成する。
HASH_MAP_t *new_hash_map(void) {
    HASH_MAP_t *hash_map = calloc(1, sizeof(HASH_MAP_t));
    hash_map->capacity = HASH_MAP_INIT_SIZE;
    hash_map->array = calloc(HASH_MAP_INIT_SIZE, sizeof(HASH_ENTRY_t*));
    return hash_map;
}

//ハッシュマップをフリーする。
void free_hash_map(HASH_MAP_t *hash_map) {
    if (hash_map) {
        for (int i=0; i<hash_map->capacity; i++) {
            HASH_ENTRY_t *entry = hash_map->array[i];
            while (entry) {
                HASH_ENTRY_t *next = entry->next;
                free(entry);
                entry = next;
            }
        }
        free(hash_map->array);
    }
    free(hash_map);
}

//https://jonosuke.hatenadiary.org/entry/20100406/p1
//http://www.isthe.com/chongo/tech/comp/fnv/index.html
#define FNV_OFFSET_BASIS32  2166136261  //2^24 + 2^8 + 0x93
#define FNV_PRIME32         16777619
static uint32_t fnv1_hash(const char *str) {
    uint32_t hash = FNV_OFFSET_BASIS32;
    for (const char *p=str; *p; p++) {
        hash *= FNV_PRIME32;
        hash ^= *p;
    }
    return hash;
}
static uint32_t fnv1a_hash(const char *str) {
    uint32_t hash = FNV_OFFSET_BASIS32;
    for (const char *p=str; *p; p++) {
        hash ^= *p;
        hash *= FNV_PRIME32;
    }
    return hash;
}
static uint32_t dbg_hash(const char *str) {
    uint32_t hash = 0;
    for (const char *p=str; *p; p++) {
        hash *= 3;
        hash += *p;
    }
    return hash;
}
HASH_MAP_FUNC_TYPE_t hash_map_func = HASH_MAP_FUNC_FNV_1A;
static uint32_t calc_hash(const char *str) {
    switch (hash_map_func) {
        case HASH_MAP_FUNC_FNV_1:
            return fnv1_hash(str);
        case HASH_MAP_FUNC_FNV_1A:
            return fnv1a_hash(str);
        case HASH_MAP_FUNC_DBG:
            return dbg_hash(str);
        default:
            assert(0);
    }
}

//リハッシュ
static void rehash(HASH_MAP_t *hash_map) {
    HASH_MAP_t new_map = {};
    //dump_hash_map(__func__, hash_map, 0);

    //サイズを拡張した新しいハッシュマップを作成する
    new_map.capacity = hash_map->capacity * HASH_MAP_GROW_FACTOR;
    new_map.array = calloc(new_map.capacity, sizeof(HASH_ENTRY_t*));

    //すべてのエントリをコピーする
    for (int i=0; i<hash_map->capacity; i++) {
        HASH_ENTRY_t *entry = hash_map->array[i];
        while (entry) {
            HASH_ENTRY_t *next = entry->next;
            insert_entry(&new_map, entry);
            entry = next;
        }
    }
    assert(hash_map->num==new_map.num);
    free(hash_map->array);
    *hash_map = new_map;
}

//キーの一致チェック
static int match(const char *key1, const char *key2) {
    return strcmp(key1, key2)==0;
}

//エントリをリストに挿入
static void insert_entry(HASH_MAP_t *hash_map, HASH_ENTRY_t *entry) {
    int idx = calc_hash(entry->key) % hash_map->capacity;
    HASH_ENTRY_t *top_entry = hash_map->array[idx];
    entry->next = top_entry;
    hash_map->array[idx] = entry;
    hash_map->num++;
}

//データ書き込み
//すでにデータが存在する場合は上書きし0を返す。新規データ時は1を返す。
//キーにNULLは指定できない。dataにNULLを指定できる。
int put_hash_map(HASH_MAP_t *hash_map, const char *key, void *data) {
    assert(hash_map);
    assert(key);
    int idx = calc_hash(key) % hash_map->capacity;
    HASH_ENTRY_t *entry = hash_map->array[idx];
    HASH_ENTRY_t **parent = &hash_map->array[idx];
    while (entry) {
        if (match(entry->key, key)) {   //データが存在するので上書き
            *parent = entry = realloc(entry, sizeof(HASH_ENTRY_t)+strlen(key)+1);
            strcpy(entry->key, key);
            entry->data = data;
            return 0;
        }
        parent = &entry->next;
        entry = entry->next;
    }

    //データ追加
    if ((hash_map->num * 100) / hash_map->capacity > HASH_MAP_MAX_CAPACITY ) {
        rehash(hash_map);
    }
    entry = calloc(1, sizeof(HASH_ENTRY_t)+strlen(key)+1);
    strcpy(entry->key, key);
    entry->data = data;
    insert_entry(hash_map, entry);
    return 1;
}

//キーに対応するデータの取得
//存在すればdataに値を設定して1を返す。
//存在しなければ0を返す。
int get_hash_map(HASH_MAP_t *hash_map, const char *key, void **data) {
    assert(hash_map);
    assert(key);
    int idx = calc_hash(key) % hash_map->capacity;
    HASH_ENTRY_t *entry = hash_map->array[idx];
    while (entry) {
        if (match(entry->key, key)) {
            *data = entry->data;
            return 1;
        }
        entry = entry->next;
    }
    return 0;
}

//データの削除
//キーに対応するデータを削除して1を返す。
//データが存在しない場合は0を返す。
int del_hash_map(HASH_MAP_t *hash_map, const char *key) {
    assert(hash_map);
    assert(key);
    int idx = calc_hash(key) % hash_map->capacity;
    HASH_ENTRY_t *entry = hash_map->array[idx];
    HASH_ENTRY_t **parent = &hash_map->array[idx];
    while (entry) {
        if (match(entry->key, key)) {
            *parent = entry->next;
            free(entry);
            hash_map->num--;
            return 1;
        }
        parent = &entry->next;
        entry = entry->next;
    }
    return 0;
}

//ハッシュマップをダンプする
//level=0: 基本情報のみ
//level=1: 有効なキーすべて
void dump_hash_map(const char *str, HASH_MAP_t *hash_map, int level) {
    int n_col = 0;      //衝突の発生回数
    int n_used = 0;     //配列の使用数
    int max_chain = 0;  //チエインの最大長
    for (int i=0; i<hash_map->capacity; i++) {
        HASH_ENTRY_t *entry = hash_map->array[i];
        if (!entry) continue;
        n_used++;
        entry = entry->next;
        int n_chain = 1;
        while (entry) {
            n_col++;
            n_chain++;
            entry = entry->next;
        }
        if (n_chain > max_chain) max_chain = n_chain;
    }
    fprintf(stderr, "= %s: num=%d,\tcapacity=%d(%d%%),\tused=%d%%,\tcollision=%d%%,\tmax_chain=%d\n", 
        str, hash_map->num, hash_map->capacity, hash_map->num*100/hash_map->capacity,
        n_used*100/hash_map->capacity, n_col*100/hash_map->capacity, max_chain);
    if (level>0) {
        for (int i=0; i<hash_map->capacity; i++) {
            HASH_ENTRY_t *entry = hash_map->array[i];
            if (!entry) continue;
            fprintf(stderr, "%03d: \"%s\", %p\n", i, entry->key, entry->data);
            entry = entry->next;
            while (entry) {
                fprintf(stderr, "   : \"%s\", %p\n", entry->key, entry->data);
                entry = entry->next;
            }
        }
    }
}
